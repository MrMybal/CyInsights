// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TaskStore.hpp"
#include "NativeTrace.hpp"
#include "PagedTimingStore.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <stdexcept>
namespace cyinsights {
namespace {
void Check(const std::atomic<bool>* cancel){if(cancel&&cancel->load())throw std::runtime_error("Task analysis cancelled");}
uint64_t Number(const std::string& text){size_t used=0;if(text.empty()||text[0]=='-')throw std::runtime_error("Invalid task identity");auto value=std::stoull(text,&used);if(used!=text.size())throw std::runtime_error("Invalid task identity");return value;}
uint32_t Thread(const std::string& text){auto value=Number(text);if(value>UINT32_MAX)throw std::runtime_error("Invalid task thread identity");return static_cast<uint32_t>(value);}
constexpr uint64_t header=8,page_size=128;
}
double TaskRecord::Duration(TaskPhase a,TaskPhase b) const {auto first=time[static_cast<size_t>(a)],last=time[static_cast<size_t>(b)];return std::isfinite(first)&&std::isfinite(last)&&last>=first?last-first:NAN;}
bool IsTaskScope(const std::string& name) {
    if(DiagnosticWait(name))return false;
    return (TimingMatches(name,"execute")&&TimingMatches(name,"task"))||TimingMatches(name,"parallelfor");
}
std::shared_ptr<TaskStore> TaskStore::Build(const std::filesystem::path& journal,const std::filesystem::path& binary,double capture_end,const std::atomic<bool>* cancel) {
    auto store=std::make_shared<TaskStore>();store->file_=binary;store->links_=binary;store->links_+=L".links";store->end=capture_end;
    auto partial=binary;partial+=L".partial";auto links_partial=store->links_;links_partial+=L".partial";
    try {
        std::ifstream source(journal);std::string line;if(!std::getline(source,line)||line!="Event,Time,TaskId,ThreadId,RelatedId,Name,Tracked,TargetThread,Size")throw std::runtime_error("Invalid native task export header");
        std::fstream out(partial,std::ios::binary|std::ios::in|std::ios::out|std::ios::trunc);std::ofstream links(links_partial,std::ios::binary);
        if(!out||!links)throw std::runtime_error("Cannot create native task cache");out.write("CYTASK01",8);
        std::unordered_map<uint64_t,uint64_t> ids;std::unordered_map<std::string,uint32_t> names;
        std::map<uint64_t,TaskRecord> working;
        auto flush=[&]{Check(cancel);for(const auto& entry:working){out.seekp(static_cast<std::streamoff>(header+entry.first*sizeof(TaskRecord)));out.write(reinterpret_cast<const char*>(&entry.second),sizeof(TaskRecord));}
            out.flush();if(!out)throw std::runtime_error("Native task cache write failed");working.clear();};
        auto record=[&](uint64_t id)->TaskRecord& {
            auto found=ids.try_emplace(id,store->count);if(found.second)++store->count;
            auto i=found.first->second;auto cached=working.try_emplace(i);
            if(cached.second){if(found.second)cached.first->second.id=id;else {out.clear();out.seekg(static_cast<std::streamoff>(header+i*sizeof(TaskRecord)));out.read(reinterpret_cast<char*>(&cached.first->second),sizeof(TaskRecord));if(!out)throw std::runtime_error("Native task cache read failed");}}
            return cached.first->second;
        };
        bool complete=false;
        const char* phases[]={"Created","Launched","Scheduled","Started","Finished","Completed","Destroyed"};
        while(std::getline(source,line)) {
            Check(cancel);if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
            auto cells=NativeTrace::CsvFields(line);if(cells.size()!=9)throw std::runtime_error("Invalid native task export row");
            auto id=Number(cells[2]);
            if(cells[0]=="End"){if(id!=store->events)throw std::runtime_error("Incomplete task event count");complete=true;break;}
            if(cells[0]=="Init"){if(id!=0)throw std::runtime_error("Unsupported TaskTrace protocol");continue;}
            if(cells[0]=="Thread"){store->threads[Thread(cells[3])]=cells[5];continue;}
            size_t used=0;double time=std::stod(cells[1],&used);if(used!=cells[1].size()||!std::isfinite(time))throw std::runtime_error("Invalid task timestamp");store->start=std::isfinite(store->start)?std::min(store->start,time):time;store->end=std::max(store->end,time);++store->events;
            if(cells[0]=="SubsequentAdded") {
                TaskLink edge{id,Number(cells[4]),time};record(edge.from);record(edge.to);
                links.write(reinterpret_cast<const char*>(&edge),sizeof(edge));++store->relations;
            }else {
                auto phase=std::find(std::begin(phases),std::end(phases),cells[0]);if(phase==std::end(phases))throw std::runtime_error("Unknown task lifecycle event");size_t p=static_cast<size_t>(phase-std::begin(phases));
                auto& r=record(id);if(std::isfinite(r.time[p])&&r.time[p]!=time)r.flags|=2;
                else {r.time[p]=time;r.thread[p]=Thread(cells[3]);}
                if(p==1){if(!cells[5].empty()){auto name=names.try_emplace(cells[5],static_cast<uint32_t>(store->names.size()));if(name.second)store->names.push_back(cells[5]);r.name=name.first->second;}
                    r.flags|=4;
                    if(cells[6]=="1")r.flags|=1;else if(cells[6]!="0")throw std::runtime_error("Invalid tracked task flag");
                    auto target=std::stoll(cells[7],&used);if(used!=cells[7].size()||target<INT32_MIN||target>INT32_MAX)throw std::runtime_error("Invalid task target");r.target=static_cast<int32_t>(target);}
                auto size=Number(cells[8]);if(size)r.size=size;
                if(p==0||p==1)r.flags|=8;
            }
            if(working.size()>=4096)flush(); // Working cache only, never an acceptance limit.
        }
        if(!complete||source.bad())throw std::runtime_error("Native task export incomplete; original trace preserved");
        while(std::getline(source,line))if(!line.empty()&&line!="\r")throw std::runtime_error("Trailing native task export data");
        flush();out.close();links.close();if(!links)throw std::runtime_error("Task relation cache write failed");
        store->ids_.reserve(ids.size());for(auto entry:ids)store->ids_.push_back(entry);std::sort(store->ids_.begin(),store->ids_.end());Check(cancel);
        std::filesystem::rename(links_partial,store->links_);std::filesystem::rename(partial,binary);return store;
    }catch(...) {std::error_code ec;std::filesystem::remove(partial,ec);std::filesystem::remove(links_partial,ec);throw;}
}
void TaskStore::Visit(const std::function<void(const TaskRecord&)>& sink,const std::atomic<bool>* cancel) const {
    std::ifstream input(file_,std::ios::binary);input.seekg(header);std::vector<TaskRecord> batch(2048);
    for(uint64_t first=0;first<count;first+=batch.size()) {
        Check(cancel);auto size=static_cast<size_t>(std::min<uint64_t>(batch.size(),count-first));input.read(reinterpret_cast<char*>(batch.data()),static_cast<std::streamsize>(size*sizeof(TaskRecord)));
        if(!input)throw std::runtime_error("Native task cache missing or corrupt");for(size_t i=0;i<size;++i)sink(batch[i]);
    }
}
std::optional<TaskRecord> TaskStore::Get(uint64_t id) const {
    auto it=std::lower_bound(ids_.begin(),ids_.end(),std::make_pair(id,uint64_t{0}));if(it==ids_.end()||it->first!=id)return {};
    std::ifstream input(file_,std::ios::binary);TaskRecord r;input.seekg(static_cast<std::streamoff>(header+it->second*sizeof(r)));input.read(reinterpret_cast<char*>(&r),sizeof(r));if(!input)throw std::runtime_error("Native task cache missing or corrupt");return r;
}
TaskPage TaskStore::Query(double a,double b,int64_t thread,const std::string& search,uint64_t page,const std::atomic<bool>* cancel) const {
    if(!std::isfinite(a)||!std::isfinite(b)||b<=a)throw std::runtime_error("Invalid task interval");
    TaskPage result;std::ifstream input(file_,std::ios::binary);input.seekg(header);
    std::vector<TaskRecord> batch(2048);uint64_t skip=page>UINT64_MAX/page_size?UINT64_MAX:page*page_size;
    for(uint64_t first=0;first<count;first+=batch.size()) {
        Check(cancel);auto size=static_cast<size_t>(std::min<uint64_t>(batch.size(),count-first));input.read(reinterpret_cast<char*>(batch.data()),static_cast<std::streamsize>(size*sizeof(TaskRecord)));if(!input)throw std::runtime_error("Native task cache missing or corrupt");
        for(size_t i=0;i<size;++i){const auto& r=batch[i];double start=NAN,stop=NAN;
            for(double t:r.time)if(std::isfinite(t)){start=std::isfinite(start)?std::min(start,t):t;stop=std::isfinite(stop)?std::max(stop,t):t;}
            // Executing tasks use their measured execution interval. Tasks not
            // yet started retain observed lifetime/pending context.
            if(std::isfinite(r.time[3])){start=r.time[3];stop=std::isfinite(r.time[4])?r.time[4]:end;}
            else if(!std::isfinite(r.time[6]))stop=end;
            if(!std::isfinite(start)||stop<a||start>=b)continue;
            if(thread>=0&&r.thread[3]!=static_cast<uint32_t>(thread))continue;
            if(!search.empty()&&!TimingMatches(names[r.name],search.c_str())&&std::to_string(r.id).find(search)==std::string::npos)continue;
            if(result.matched>=skip&&result.rows.size()<page_size)result.rows.push_back(r);++result.matched;
        }
    }
    Check(cancel);return result;
}
std::vector<TaskLink> TaskStore::Links(uint64_t id,const std::atomic<bool>* cancel) const {
    std::ifstream input(links_,std::ios::binary);std::vector<TaskLink> result;TaskLink edge;
    for(uint64_t i=0;i<relations;++i){Check(cancel);input.read(reinterpret_cast<char*>(&edge),sizeof(edge));if(!input)throw std::runtime_error("Task relation cache missing or corrupt");if(edge.from==id||edge.to==id)result.push_back(edge);}return result;
}
TaskPage TaskStore::ScopeCandidates(const PagedTimingStore& store,double a,double b,int64_t thread,const std::string& search,uint64_t page,const std::atomic<bool>* cancel) const {
    TaskPage result;uint64_t skip=page>UINT64_MAX/page_size?UINT64_MAX:page*page_size;
    std::vector<uint8_t> matches(store.timers.size());for(size_t i=0;i<matches.size();++i)matches[i]=IsTaskScope(store.timers[i].name)&&TimingMatches(store.timers[i].name,search.c_str())?1:0;
    store.VisitScopes(thread<0?SIZE_MAX:static_cast<size_t>(thread),a,b,false,[&](const StoredScope& s){
        if(store.tracks[s.track].gpu)return;
        if(result.matched>=skip&&result.rows.size()<page_size){TaskRecord r;r.id=result.matched;r.name=s.timer;r.thread[3]=s.track;r.time[3]=s.start;if(!s.unfinished)r.time[4]=s.end;result.rows.push_back(r);}++result.matched;
    },cancel,&matches);
    return result;
}
}
