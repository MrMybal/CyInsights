// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeTrace.hpp"
#include "AnalysisWorkers.hpp"
#include "PagedTimingStore.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
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
    read_counters_ = true;
    counters = CounterModel{};
    if (std::filesystem::exists(cache / "counters.csv")) std::filesystem::remove(cache / "counters.csv");
    if (std::filesystem::exists(csv_))
        std::filesystem::remove(csv_);
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
    if (Busy()) {
        error = "Native analysis is already running";
        return false;
    }
    try {
        error_.clear();progress_.cancel.store(false);
        progress_.bytes.store(0);progress_.total.store(0);progress_.events.store(0);
        const bool read_counters=read_counters_;read_counters_=false;
        csv_task_ = std::async(std::launch::async, [this,file,read_counters] {
            CsvResult result;
            if(paged_) {
                result.store=PagedTimingStore::Build(file,file.parent_path()/"timing.cyindex",progress_,result.error);
                result.success=result.store!=nullptr;

            }else result.success = ReadCsv(file, result.events, result.error,&progress_);
            if(read_counters&&result.success) CounterModel::ReadCatalog(file.parent_path()/"counters.csv",result.counters,result.counters.error);
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
    if(progress_.cancel.load()){message=error_="Analysis cancelled";return true;}
    if (code != 0) {
        if(error_.empty())error_=progress_.cancel.load()?"Analysis cancelled":"Native analyzer failed (exit code "+std::to_string(code)+"); see analysis.log";
        message=error_;
        return true;
    }
    if (!ImportCsv(csv_, message))
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
} // namespace cyinsights
