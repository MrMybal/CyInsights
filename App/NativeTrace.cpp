// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeTrace.hpp"
#include "AnalysisWorkers.hpp"
#include "PagedTimingStore.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <set>
#include <cstring>
#include "../Tools/NativeTiming/Format.hpp"
namespace cyinsights {
std::wstring NativeTrace::Quote(const std::wstring &s) {
    std::wstring r = L"\"";
    size_t n = 0;
    for (auto c : s) {
        if (c == L'\\') {
            ++n;
            continue;
        }
        if (c == L'\"')
            r.append(n * 2 + 1, L'\\');
        else
            r.append(n, L'\\');
        n = 0;
        r += c;
    }
    r.append(n * 2, L'\\');
    return r + L"\"";
}
bool NativeTrace::Launch(const std::filesystem::path &exe, const std::wstring &args, HANDLE *process,
                         std::string &error,const std::filesystem::path& working_directory) {
    if (!std::filesystem::is_regular_file(exe)) {
        error = "Choose the matching installed UnrealInsights.exe";
        return false;
    }
    std::wstring cmd = Quote(exe.wstring()) + L" " + args;
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const auto directory=working_directory.empty()?exe.parent_path():working_directory;
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        directory.c_str(), &si, &pi)) {
        error = "Cannot start Unreal Insights: " + std::to_string(GetLastError());
        return false;
    }
    CloseHandle(pi.hThread);
    if (process)
        *process = pi.hProcess;
    else
        CloseHandle(pi.hProcess);
    return true;
}
NativeTrace::~NativeTrace() {
    progress_.cancel.store(true);
    if(csv_task_.valid())csv_task_.wait();
    if (process_) {
        if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT)
            TerminateProcess(process_, 0);
        CloseHandle(process_);
    }
}
void NativeTrace::Cancel() {
    progress_.cancel.store(true);
    if(process_) { TerminateProcess(process_, 1); }
}
uint64_t NativeTrace::ExportBytes() const {
    std::error_code ec;
    auto size=std::filesystem::file_size(csv_,ec);
    return ec?0:size;
}
bool NativeTrace::Start(const std::filesystem::path &exe, const std::filesystem::path &trace,
                        const std::filesystem::path &cache, std::string &error, bool paged) {
    if (Busy()) {
        error = "Native analysis is already running";
        return false;
    }
    std::filesystem::create_directories(cache);
    paged_=paged;store.reset();
    error_.clear();progress_.cancel.store(false);
    progress_.bytes.store(0);progress_.total.store(0);progress_.events.store(0);
    started_=std::chrono::steady_clock::now();log_=cache/"analysis.log";
    csv_ = cache / "timing.csv";
    binary_source_=false;
    decode_seconds_=index_seconds_=0;
    read_counters_ = true;
    counters = CounterModel{};
    if (std::filesystem::exists(cache / "counters.csv")) std::filesystem::remove(cache / "counters.csv");
    if (std::filesystem::exists(csv_))
        std::filesystem::remove(csv_);
    // The matching local adapter is optional; the existing bridge covers
    // installations where no native reader has been built yet.
    auto binaries=exe.parent_path();std::ifstream version(binaries.parent_path().parent_path()/"Build/Build.version");
    std::string metadata((std::istreambuf_iterator<char>(version)),{});
    auto number=[&](const char* key){auto p=metadata.find(key);if(p==std::string::npos)return -1;p=metadata.find(':',p);if(p==std::string::npos)return -1;try{return std::stoi(metadata.substr(p+1));}catch(...){return -1;}};
    const int major=number("MajorVersion"),minor=number("MinorVersion");
    wchar_t app[32768]{};GetModuleFileNameW(nullptr,app,32768);
    auto helper=std::filesystem::path(app).parent_path()/"NativeTiming"/(std::to_string(major)+"."+std::to_string(minor))/"CyNativeTiming.exe";
    wchar_t forceCsv[2]{};const bool forced=GetEnvironmentVariableW(L"CYINSIGHTS_TIMING_CSV",forceCsv,2)==1&&forceCsv[0]==L'1';
    if(!forced&&major==5&&minor>=5&&minor<=8&&std::filesystem::is_regular_file(helper)) {
        csv_=cache/"timing.cynt";log_=cache/"timing.cynt.log";std::error_code ec;std::filesystem::remove(csv_,ec);binary_source_=true;
        return Launch(helper,Quote(trace.wstring())+L" "+Quote(csv_.wstring())+L" "+Quote(binaries.wstring()),&process_,error,binaries);
    }
    auto cmds = cache / "export.txt";
    std::ofstream out(cmds);
    // Insights' command tokenizer treats backslashes as escapes inside quoted paths.
    out << "TimingInsights.ExportTimingEvents \"" << csv_.generic_u8string()
        << "\" -columns=ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth\n";
    out << "TimingInsights.ExportCounters " << (cache / "counters.csv").generic_u8string() << "\n";
    out.close();
    if (!out) {error="Cannot create native analysis command file";return false;}
    const bool launched=Launch(exe,
                  L"-NoUI -AutoQuit -OpenTraceFile=" + Quote(trace.wstring()) + L" -ABSLOG=" +
                      Quote((cache / "analysis.log").wstring()) + L" -ExecOnAnalysisCompleteCmd=" +
                      Quote(L"@=" + cmds.wstring()),
                  &process_, error);
    if(!launched)read_counters_=false;
    return launched;
}
bool NativeTrace::ImportCsv(const std::filesystem::path &file, std::string &error) {
    if(Busy()){error="Native analysis is already running";return false;}
    binary_source_=false;return ImportTiming(file,error);
}
bool NativeTrace::ImportTiming(const std::filesystem::path &file, std::string &error) {
    if (Busy()) {
        error = "Native analysis is already running";
        return false;
    }
    try {
        error_.clear();progress_.cancel.store(false);
        progress_.bytes.store(0);progress_.total.store(0);progress_.events.store(0);
        const bool read_counters=read_counters_;read_counters_=false;
        csv_task_ = std::async(std::launch::async, [this,file,read_counters] {
            CsvResult result;auto begin=std::chrono::steady_clock::now();
            if(paged_) {
                result.store=PagedTimingStore::Build(file,file.parent_path()/"timing.cyindex",progress_,result.error,binary_source_);
                result.success=result.store!=nullptr;

            }else if(binary_source_){result.success=ScanBinary(file,[&](TimingEvent&& e){result.events.push_back(std::move(e));},result.error,&progress_);
                double last=0;for(const auto& e:result.events)last=std::max(last,e.open_ended?e.start:e.end);
                for(auto& e:result.events)if(e.open_ended)e.end=last;
            }
            else result.success = ReadCsv(file, result.events, result.error,&progress_);
            if(read_counters&&result.success) CounterModel::ReadCatalog(file.parent_path()/"counters.csv",result.counters,result.counters.error);
            result.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
            return result;
        });
        return true;
    } catch (const std::exception &e) {
        error = "Cannot start native timing import: " + std::string(e.what());
        return false;
    }
}
bool NativeTrace::Poll(std::string &message) {
    if (csv_task_.valid()) {
        if (csv_task_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return false;
        try {
            auto result = csv_task_.get();
            index_seconds_=result.seconds;
            store=std::move(result.store);
            counters = std::move(result.counters);
            if (result.success) {
                events = std::move(result.events);
                message = "Native timing indexed: " + std::to_string(store?store->count:events.size()) + " recorded events";
            } else
                message = error_ = result.error;
        } catch (const std::exception &e) {
            message = error_ = "Native timing import failed: " + std::string(e.what());
        }
        return true;
    }
    if (!process_)return false;
    if (WaitForSingleObject(process_, 0) == WAIT_TIMEOUT)return false;
    DWORD code = 1;
    GetExitCodeProcess(process_, &code);
    CloseHandle(process_);
    process_ = nullptr;
    decode_seconds_=std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count();
    if(progress_.cancel.load()){message=error_="Analysis cancelled";return true;}
    if (code != 0) {
        if(error_.empty())error_=progress_.cancel.load()?"Analysis cancelled":"Native analyzer failed (exit code "+std::to_string(code)+"); "+(binary_source_?"see timing.cynt.log and check that NativeTiming matches the selected Unreal installation":"see analysis.log");
        message=error_;
        return true;
    }
    if (!ImportTiming(csv_, message))
        return true;
    message = "Loading native timing in background...";
    return false;
}
namespace {
std::vector<std::string> Row(const std::string &line) {
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (quoted && i + 1 < line.size() && line[i + 1] == '"') {
                cell += '"';
                ++i;
            } else
                quoted = !quoted;
        } else if (c == ',' && !quoted) {
            cells.push_back(cell);
            cell.clear();
        } else if (c != '\r')
            cell += c;
    }
    cells.push_back(cell);
    return cells;
}
uint64_t Identity(const std::string &value) {
    if (value.empty() || value.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid native timing identity");
    return std::stoull(value);
}
} // namespace
std::vector<std::string> NativeTrace::CsvFields(const std::string &line) {
    return Row(line);
}
bool NativeTrace::ScanCsv(const std::filesystem::path &file,const std::function<void(TimingEvent&&)>& sink,
                          std::string &error, NativeReadProgress* progress) {
    std::error_code size_error;
    auto bytes = std::filesystem::file_size(file, size_error);
    if(progress){progress->total.store(size_error?0:bytes);progress->bytes.store(0);progress->events.store(0);}
    std::ifstream in(file);
    if (!in) {
        error = "Native analyzer produced no timing CSV; see analysis.log";
        return false;
    }
    std::string line;
    std::getline(in, line);
    if(line.rfind("\xEF\xBB\xBF",0)==0)line.erase(0,3);
    auto header = Row(line);
    std::map<std::string, size_t> columns;
    for (size_t i = 0; i < header.size(); ++i)
        columns[header[i]] = i;
    for (const auto *name : {"ThreadName", "TimerName", "StartTime", "EndTime", "Depth"})
        if (!columns.count(name)) {
            error = "Unsupported timing CSV header";
            return false;
        }
    const auto thread_name=columns.at("ThreadName"),timer_name=columns.at("TimerName"),
        start_time=columns.at("StartTime"),end_time=columns.at("EndTime"),depth=columns.at("Depth");
    const auto thread_id=columns.find("ThreadId"),timer_id=columns.find("TimerId");
    uint64_t count=0;
    try {
        if(progress&&progress->cancel.load())throw std::runtime_error("Analysis cancelled");
        auto parse=[&](const std::string& text) {
            if(text.empty())return TimingEvent{};
            auto row = Row(text);
            if (row.size() != header.size())
                throw std::runtime_error("Invalid timing CSV row");
            TimingEvent e{std::move(row[thread_name]), std::move(row[timer_name]),
                          std::stod(row[start_time]), std::stod(row[end_time]),std::stoi(row[depth])};
            if (thread_id!=columns.end()) {
                e.thread_id = Identity(row[thread_id->second]);
                e.has_thread_id = true;
            }
            if (timer_id!=columns.end()) {
                e.timer_id = Identity(row[timer_id->second]);
                e.has_timer_id = true;
            }
            if(e.has_thread_id&&e.has_timer_id&&e.thread_id>=(1ull<<24)&&e.thread_id<=(1ull<<24)+1) {
                const int type=static_cast<int>(e.thread_id-(1ull<<24));
                const char* thread=type==0?"Game Frames":"Rendering Frames";
                const char* prefix=type==0?"Game Frame ":"Rendering Frame ";
                if(e.thread==thread&&e.name.rfind(prefix,0)==0) {
                    e.frame_index=Identity(e.name.substr(std::char_traits<char>::length(prefix)));
                    if(static_cast<uint32_t>(e.frame_index)!=e.timer_id)throw std::runtime_error("Native frame identity disagrees with export");
                    e.frame_type=type;e.has_timer_id=false;
                }
            }
            // Insights exports +inf for a scope still active when tracing stopped.
            // Retain its identity, but never report its clipped display span as a measured duration.
            e.open_ended = std::isinf(e.end) && e.end > 0;
            if (!std::isfinite(e.start) || (!e.open_ended && !std::isfinite(e.end)) || e.end < e.start ||
                e.depth < 0 || e.depth > 4096)
                throw std::runtime_error("Invalid timing values");
            return e;
        };
        uint64_t consumed=line.size()+1;
        // Independent CSV decoding runs in bounded batches; ingestion stays in
        // source order so nesting, frame identity and progress remain valid.
        constexpr size_t batch_size=8192,chunk_size=512;
        std::vector<std::string> lines(batch_size);std::vector<TimingEvent> decoded;
        for(;;) {
            size_t used=0;
            while(used<batch_size&&std::getline(in,lines[used])) {
                consumed+=lines[used].size()+1;++used;
                if(used%1024==0&&progress&&progress->cancel.load())throw std::runtime_error("Analysis cancelled");
            }
            if(!used)break;
            decoded.resize(used);
            ParallelAnalysis((used+chunk_size-1)/chunk_size,used>=2048?AnalysisWorkerCount():1,[&](size_t chunk) {
                auto last=std::min(used,(chunk+1)*chunk_size);
                for(size_t i=chunk*chunk_size;i<last;++i)decoded[i]=parse(lines[i]);
            },progress?&progress->cancel:nullptr);
            for(size_t i=0;i<used;++i) {
                if(count%4096==0&&progress) {
                    if(progress->cancel.load())throw std::runtime_error("Analysis cancelled");
                    progress->bytes.store(std::min(consumed,bytes));progress->events.store(count);
                }
                if(lines[i].empty())continue;
                sink(std::move(decoded[i]));++count;
            }
        }
        if(!in.eof())throw std::runtime_error("Timing CSV read failed");
        if(progress&&progress->cancel.load())throw std::runtime_error("Analysis cancelled");
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
    if(progress){progress->bytes.store(bytes);progress->events.store(count);}
    return true;
}
bool NativeTrace::ReadCsv(const std::filesystem::path& file,std::vector<TimingEvent>& output,
                          std::string& error,NativeReadProgress* progress) {
    std::vector<TimingEvent> result;
    if(!ScanCsv(file,[&](TimingEvent&& event){result.push_back(std::move(event));},error,progress))return false;
    double last = 0;
    for (const auto &e : result)
        last = std::max(last, e.open_ended ? e.start : e.end);
    for (auto &e : result)
        if (e.open_ended)
            e.end = last;
    output = std::move(result);
    return true;
}
bool NativeTrace::ScanBinary(const std::filesystem::path& file,const std::function<void(TimingEvent&&)>& sink,
                            std::string& error,NativeReadProgress* progress) {
    try {
        std::ifstream in(file,std::ios::binary);if(!in)throw std::runtime_error("Native timing binary missing");
        const uint64_t bytes=std::filesystem::file_size(file);uint64_t consumed=0,count=0;
        if(progress){progress->total.store(bytes);progress->bytes.store(0);progress->events.store(0);}
        auto check=[&]{if(progress&&progress->cancel.load())throw std::runtime_error("Analysis cancelled");};
        auto read=[&](void* data,size_t size){check();if(size>bytes-consumed)throw std::runtime_error("Truncated native timing binary");if(!in.read(static_cast<char*>(data),static_cast<std::streamsize>(size)))throw std::runtime_error("Native timing binary read failed");consumed+=size;};
        auto u32=[&]{uint32_t v;read(&v,sizeof(v));return v;};
        auto u64=[&]{uint64_t v;read(&v,sizeof(v));return v;};
        auto string=[&]{const auto n=u32();if(n>bytes-consumed)throw std::runtime_error("Invalid native timing string length");std::string s(n,'\0');read(s.data(),n);return s;};
        char magic[8];read(magic,8);if(std::memcmp(magic,cynt::magic,8))throw std::runtime_error("Unsupported native timing binary version");
        struct Timer {uint64_t id;std::string name;};std::vector<Timer> timers;std::set<uint64_t> ids;
        auto n=u32();if(n>(bytes-consumed)/12)throw std::runtime_error("Invalid native timing dictionary length");
        for(uint32_t i=0;i<n;++i){auto id=u64();if(!ids.insert(id).second)throw std::runtime_error("Duplicate native timer identity");timers.push_back({id,string()});}
        auto tracks=u32();if(tracks>(bytes-consumed)/20)throw std::runtime_error("Invalid native timing track count");ids.clear();
        auto progressUpdate=[&]{if(progress){progress->bytes.store(consumed);progress->events.store(count);}};
        auto valid=[&](const TimingEvent& e){if(!std::isfinite(e.start)||(!e.open_ended&&!std::isfinite(e.end))||e.end<e.start||e.depth<0||e.depth>4096)throw std::runtime_error("Invalid native timing values");};
        std::vector<cynt::Event> buffer(8192);
        for(uint32_t i=0;i<tracks;++i){
            auto id=u64();if(!ids.insert(id).second)throw std::runtime_error("Duplicate native track identity");auto name=string();auto remaining=u64();
            if(remaining>(bytes-consumed)/sizeof(cynt::Event))throw std::runtime_error("Invalid native timing event count");
            while(remaining){auto used=static_cast<size_t>(std::min<uint64_t>(remaining,buffer.size()));read(buffer.data(),used*sizeof(cynt::Event));
                for(size_t j=0;j<used;++j){const auto& r=buffer[j];if(r.timer>=timers.size()||r.flags>1||r.reserved||r.depth>4096)throw std::runtime_error("Invalid native timing event reference");
                    TimingEvent e{name,timers[r.timer].name,r.start,r.end,static_cast<int>(r.depth)};e.thread_id=id;e.has_thread_id=true;e.timer_id=timers[r.timer].id;e.has_timer_id=true;
                    e.open_ended=r.flags==1;if(e.open_ended&&!(std::isinf(e.end)&&e.end>0))throw std::runtime_error("Invalid unfinished native timing scope");valid(e);sink(std::move(e));++count;
                }remaining-=used;check();progressUpdate();
            }
        }
        auto frames=u64();if(frames>(bytes-consumed)/sizeof(cynt::Frame))throw std::runtime_error("Invalid native frame count");
        for(uint64_t i=0;i<frames;++i){cynt::Frame r;read(&r,sizeof(r));if(r.type>1||r.reserved)throw std::runtime_error("Invalid native frame type");
            TimingEvent e{r.type?"Rendering Frames":"Game Frames",(r.type?"Rendering Frame ":"Game Frame ")+std::to_string(r.index),r.start,r.end,1};
            e.thread_id=(1ull<<24)+r.type;e.has_thread_id=true;e.frame_type=static_cast<int>(r.type);e.frame_index=r.index;e.open_ended=std::isinf(r.end)&&r.end>0;valid(e);sink(std::move(e));++count;
        }
        read(magic,8);if(std::memcmp(magic,cynt::footer,8)||u64()!=count||consumed!=bytes)throw std::runtime_error("Incomplete or inconsistent native timing binary");
        check();progressUpdate();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
} // namespace cyinsights
