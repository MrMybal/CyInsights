// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ComparisonTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
using namespace cyinsights;
namespace {
int checks=0,failures=0;
void Check(bool good,const char* text){++checks;if(!good){++failures;std::cerr<<"FAIL: "<<text<<"\n";}}
bool Near(double a,double b){return std::abs(a-b)<1e-9;}
TimingEvent Event(const char* thread,const char* name,double start,double end,int depth,uint64_t tid,uint64_t timer) {
    TimingEvent event{thread,name,start,end,depth};event.thread_id=tid;event.timer_id=timer;
    event.has_thread_id=event.has_timer_id=true;return event;
}
std::shared_ptr<ComparisonDataset> Dataset(std::vector<TimingEvent> events) {
    auto data=std::make_shared<ComparisonDataset>();data->events=std::move(events);data->timing.Build(data->events);
    data->index.Build(data->events,data->timing);data->start=data->timing.start;data->end=data->timing.end;
    data->summary=data->index.Summarize(data->events,data->timing);return data;
}
const ComparisonRow& Find(const std::vector<ComparisonRow>& rows,const char* name,bool gpu=false) {
    for(const auto& row:rows)if(row.key.name==name&&row.key.gpu==gpu)return row;
    throw std::runtime_error(std::string("Missing comparison row ")+name);
}
bool Wait(ComparisonTrace& worker,std::shared_ptr<ComparisonDataset>& data,std::string& message,int seconds=3) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    while(worker.Busy()&&std::chrono::steady_clock::now()<deadline){worker.Poll(data,message);if(worker.Busy())std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    if(worker.Busy()){worker.Cancel();return false;}return true;
}
}
int main(int argc,char** argv) {
    // Source-owned fake analyzer for cancellation tests; it does not load Unreal.
    if(argc>1&&std::string(argv[1])=="-NoUI") {
        for(int i=1;i<argc;++i)if(std::string(argv[i]).rfind("-ABSLOG=",0)==0) {
            std::ofstream(fs::u8path(std::string(argv[i]).substr(8)))<<GetCurrentProcessId()<<"\n";
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));return 0;
    }
    if(argc==4&&std::string(argv[1])=="--csv-compare") {
        std::vector<TimingEvent> a_events,b_events;std::string error;
        if(!NativeTrace::ReadCsv(fs::u8path(argv[2]),a_events,error)||!NativeTrace::ReadCsv(fs::u8path(argv[3]),b_events,error)){std::cerr<<error;return 1;}
        auto a=Dataset(std::move(a_events)),b=Dataset(std::move(b_events));
        auto rows=CompareTiming(a->summary,b->summary);size_t candidates=0,ambiguous=0,unpaired=0;
        for(const auto& row:rows){candidates+=std::string(row.Relation())=="Name candidate";ambiguous+=std::string(row.Relation())=="Ambiguous name group";unpaired+=!row.defined_a||!row.defined_b;}
        std::cout<<"A: "<<a->events.size()<<" events / "<<a->summary.size()<<" scope groups / native frames: "<<(a->timing.frames_native?a->timing.frames.size():0)<<"\n"
            <<"B: "<<b->events.size()<<" events / "<<b->summary.size()<<" scope groups / native frames: "<<(b->timing.frames_native?b->timing.frames.size():0)<<"\n"
            <<"Rows: "<<rows.size()<<" / candidates: "<<candidates<<" / ambiguous: "<<ambiguous<<" / unpaired: "<<unpaired<<"\n";
        bool consistent=true;
        for(const auto& row:CompareTiming(a->summary,a->summary))if(auto delta=ComparisonDelta(row,ComparisonMode::Total);delta&&*delta!=0)consistent=false;
        std::cout<<"A self-comparison zero delta: "<<consistent<<"\n";
        if(!b->timing.frames.empty()) {
            const auto bounds=b->timing.frames[b->timing.frames.size()/2];ComparisonTrace worker;
            if(!worker.Range(b,bounds.first,bounds.second,error)||!Wait(worker,b,error,30)||b->start!=bounds.first||b->end!=bounds.second){std::cerr<<error;return 1;}
            size_t count=0;for(const auto& value:b->summary)count+=value.count;
            std::cout<<std::setprecision(12)<<"B interval: "<<b->start<<" .. "<<b->end<<" s / complete overlapping scopes: "<<count<<"\n";
            if(!count)return 1;
        }
        return consistent&&!rows.empty()?0:1;
    }
    if(argc==6&&std::string(argv[1])=="--compare") {
        std::shared_ptr<ComparisonDataset> a,b;std::string message;
        fs::path cache=fs::u8path(argv[5]);
        ComparisonTrace worker;
        if(!worker.Start(fs::u8path(argv[3]),fs::u8path(argv[2]),cache/"A",message)||!Wait(worker,a,message,120)||!a){std::cerr<<message;return 1;}
        std::cout<<"A: "<<a->events.size()<<" events / "<<a->summary.size()<<" groups\n";
        if(!worker.Start(fs::u8path(argv[4]),fs::u8path(argv[2]),cache/"B",message)||!Wait(worker,b,message,120)||!b){std::cerr<<message;return 1;}
        auto rows=CompareTiming(a->summary,b->summary);size_t candidates=0,ambiguous=0,unpaired=0;
        for(const auto& row:rows){auto relation=std::string(row.Relation());candidates+=relation=="Name candidate";ambiguous+=relation=="Ambiguous name group";unpaired+=!row.defined_a||!row.defined_b;}
        std::cout<<"B: "<<b->events.size()<<" events / "<<b->summary.size()<<" groups\n"
            <<"Rows: "<<rows.size()<<" / candidates: "<<candidates<<" / ambiguous: "<<ambiguous<<" / unpaired: "<<unpaired<<"\n";
        auto same=CompareTiming(a->summary,a->summary);bool consistent=true;
        for(const auto& row:same)if(auto delta=ComparisonDelta(row,ComparisonMode::Total);delta&&*delta!=0)consistent=false;
        std::cout<<"A self-comparison zero delta: "<<consistent<<"\n";
        auto bounds=b->timing.frames.empty()?std::pair<double,double>{b->start,(b->start+b->end)/2}:b->timing.frames[b->timing.frames.size()/2];
        if(!worker.Range(b,bounds.first,bounds.second,message)||!Wait(worker,b,message,30)){std::cerr<<message;return 1;}
        size_t clipped_count=0;for(const auto& value:b->summary)clipped_count+=value.count;
        std::cout<<std::setprecision(12)<<"B range update: "<<b->start<<" .. "<<b->end<<" s / complete overlapping scopes: "<<clipped_count<<"\n";
        return consistent&&!rows.empty()&&clipped_count?0:1;
    }
    auto a=Dataset({Event("GameThread","Outer",0,4,0,1,10),Event("GameThread","Work",1,3,1,1,20),
        Event("GPU Graphics","Work",1,3,0,2,30),Event("Worker","Duplicate",0,1,0,3,40),
        Event("Worker","Duplicate",1,2,0,3,41),Event("Worker","Only A",3,4,0,3,50),Event("Worker","Open",1,4,0,3,60)});
    a->events.back().open_ended=true;a->timing.Build(a->events);a->index.Build(a->events,a->timing);a->summary=a->index.Summarize(a->events,a->timing);
    auto b=Dataset({Event("GameThread","Outer",10,18,0,5,900),Event("GameThread","Work",12,16,1,5,901),
        Event("GPU Graphics","Work",12,13,0,6,902),Event("Worker","Duplicate",10,12,0,7,903),
        Event("Worker","Only B",12,13,0,7,50)});
    auto rows=CompareTiming(a->summary,b->summary);
    Check(rows.size()==7,"union retains CPU/GPU groups, incomplete scopes and one-sided rows");
    const auto& work=Find(rows,"Work");const auto& gpu=Find(rows,"Work",true);const auto& outer=Find(rows,"Outer");
    Check(Near(work.a.inclusive,2)&&Near(work.b.inclusive,4),"independent time origins do not shift or align captures");
    Check(Near(gpu.a.inclusive,2)&&Near(gpu.b.inclusive,1),"CPU/GPU name groups remain separate");
    Check(work.a.group.native_ids[0]==20&&work.b.group.native_ids[0]==901,"session-local native IDs are retained");
    Check(std::string(work.Relation())=="Name candidate","different IDs with same name remain candidate matching");
    Check(std::string(Find(rows,"Duplicate").Relation())=="Ambiguous name group","multiple native timers are not paired arbitrarily");
    Check(Find(rows,"Duplicate").a.group.native_ids.size()==2&&Near(Find(rows,"Duplicate").a.inclusive,2),"ambiguous group explicitly aggregates preserved identities");
    Check(std::string(Find(rows,"Only A").Relation())=="Only A"&&std::string(Find(rows,"Only B").Relation())=="Only B","same numeric ID with different names must not match across sessions");
    Check(!ComparisonDelta(Find(rows,"Only A"),ComparisonMode::Total),"missing counterpart has unavailable delta, not synthetic zero");
    Check(Near(outer.a.exclusive,2)&&Near(outer.b.exclusive,4),"exclusive costs use contained immediate children");
    Check(Near(*ComparisonDelta(work,ComparisonMode::Total),2),"delta is B minus A");
    Check(Near(*ComparisonDelta(work,ComparisonMode::Total,true),100),"percentage uses A as baseline");
    Check(Near(*ComparisonDelta(work,ComparisonMode::PerSecond),0),"different interval durations can be normalized independently");
    Check(Near(*ComparisonDelta(work,ComparisonMode::PerOccurrence),2),"per-occurrence means use complete overlapping scopes");
    Check(Find(rows,"Open").a.unfinished==1&&!ComparisonCost(Find(rows,"Open").a,ComparisonMode::Total),"unfinished scopes are reported without duration");
    for(const auto& data:{a,b}) {
        double grouped=0,total=0;
        for(const auto& value:data->summary)grouped+=value.inclusive;
        for(const auto& value:data->timing.totals)total+=value.inclusive;
        Check(Near(grouped,total),"comparison grouping conserves all native inclusive totals");
    }
    {
        auto once=Dataset({Event("Worker","Repeat",0,2,0,1,1)});
        auto twice=Dataset({Event("Worker","Repeat",0,2,0,1,2),Event("Worker","Repeat",2,4,0,1,2)});
        auto repeated=CompareTiming(once->summary,twice->summary);const auto& row=Find(repeated,"Repeat");
        Check(row.a.count==1&&row.b.count==2,"occurrence counts remain independent across captures");
        Check(Near(*ComparisonDelta(row,ComparisonMode::PerOccurrence),0),"per-occurrence comparison accounts for different counts");
        Check(Near(*ComparisonDelta(row,ComparisonMode::PerSecond),0),"window normalization accounts for repeated work over longer captures");
    }
    a->timing.Aggregate(a->events,2.5,3.5);auto clipped=a->index.Summarize(a->events,a->timing);
    auto slice=CompareTiming(clipped,b->summary);
    Check(Near(Find(slice,"Work").a.inclusive,.5)&&Near(Find(slice,"Outer").a.inclusive,1),"cross-frame scopes are clipped to the selected interval");
    Check(Near(Find(slice,"Outer").a.exclusive,.5),"clipped exclusive cost retains containment");
    Check(Find(slice,"Duplicate").defined_a&&Find(slice,"Duplicate").a.count==0&&!ComparisonCost(Find(slice,"Duplicate").a,ComparisonMode::Total),"defined timer absent from interval is distinct from absent catalog identity");
    a->timing.Aggregate(a->events,2,2);
    auto empty=a->index.Summarize(a->events,a->timing);
    Check(CompareTiming(empty,{}).empty(),"zero interval contains no complete or unfinished observations");
    auto same=CompareTiming(b->summary,b->summary);
    bool zero=true;for(const auto& row:same)if(auto delta=ComparisonDelta(row,ComparisonMode::Total);delta&&*delta!=0)zero=false;
    Check(zero,"self-comparison has zero delta");
    auto missing_id=Dataset({Event("Worker","Work",0,1,0,1,0)});missing_id->events[0].has_timer_id=false;
    missing_id->timing.Build(missing_id->events);missing_id->index.Build(missing_id->events,missing_id->timing);
    auto unknown=CompareTiming(missing_id->index.Summarize(missing_id->events,missing_id->timing),b->summary);
    Check(std::string(Find(unknown,"Work").Relation())=="Ambiguous name group","missing native identity remains explicitly ambiguous");
    ComparisonValue zero_value;zero_value.count=1;zero_value.interval=1;
    ComparisonRow zero_row;zero_row.defined_a=zero_row.defined_b=true;zero_row.a=zero_value;zero_row.b=zero_value;zero_row.b.exclusive=1;
    Check(!ComparisonDelta(zero_row,ComparisonMode::Total,true,true)&&Near(*ComparisonDelta(zero_row,ComparisonMode::Total,false,true),1),"zero baseline yields unavailable percentage and valid absolute delta");
    zero_value.interval=0;Check(!ComparisonCost(zero_value,ComparisonMode::PerSecond),"zero normalization interval is unavailable");
    bool mismatch=false;try{a->index.Summarize({},a->timing);}catch(...){mismatch=true;}
    Check(mismatch,"mismatched model/index is rejected");
    {
        auto frame=Event("Game Frames","Game Frame 7",0,1,1,1ull<<24,7);frame.frame_type=0;frame.frame_index=7;frame.has_timer_id=false;
        auto with_frames=Dataset({frame,Event("Worker","Work",0,1,0,1,7)});
        Check(with_frames->summary.size()==1&&with_frames->summary[0].group.key.name=="Work", "Native frame rows are excluded from scope comparison groups");
        auto only_frames=Dataset({frame});Check(only_frames->summary.empty()&&CompareTiming(only_frames->summary,{}).empty(),"Frame-only traces do not fabricate scope costs");
    }
    {
        ComparisonTrace worker;std::string message;std::shared_ptr<ComparisonDataset> published=b;
        Check(!worker.Range({},0,1,message),"missing B dataset rejects interval updates");
        Check(!worker.Range(b,12,11,message)&&!worker.Range(b,9,11,message)&&!worker.Range(b,10,19,message)&&!worker.Range(b,NAN,12,message),"invalid or out-of-capture B intervals are rejected");
        Check(worker.Range(b,12,13,message)&&worker.Busy(),"B interval update starts in background");
        Check(!worker.Range(b,13,14,message),"concurrent B update is refused");
        Check(Wait(worker,published,message)&&Near(published->start,12)&&Near(published->end,13),"background B update publishes its own interval");
        auto new_rows=CompareTiming(clipped,published->summary);
        Check(Near(Find(new_rows,"Work").b.inclusive,1),"B interval contains clipped scopes");
        auto folder=fs::temp_directory_path()/("CyComparisonTests_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Check(worker.Start(folder/"missing.utrace",folder/"missing.exe",folder/"cache",message),"analyzer launch occurs in background");
        Check(Wait(worker,published,message)&&published==b&&message.find("UnrealInsights")!=std::string::npos,"failed B analysis preserves the last usable reference and releases busy state");
        Check(worker.Start(folder/"unsupported.csv",{},folder/"bad",message)&&Wait(worker,published,message)&&message.find("Choose")!=std::string::npos,"unsupported capture type reports its own failure");
        Check(worker.Start(folder/"missing.cycapture",{},folder/"archive",message)&&Wait(worker,published,message)&&published==b,"invalid archive preserves existing comparison");
        wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,32768);
        Check(worker.Start(folder/"cancel.utrace",self,folder/"cancel",message),"source-owned fake analyzer starts for cancellation check");
        auto marker=folder/"cancel/analysis/analysis.log";DWORD pid=0;
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!pid&&std::chrono::steady_clock::now()<deadline){std::ifstream(marker)>>pid;std::this_thread::sleep_for(std::chrono::milliseconds(5));}
        HANDLE owned=pid?OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid):nullptr;
        Check(owned!=nullptr,"fake analyzer exposes its owned process identity");
        worker.Cancel();Check(Wait(worker,published,message)&&published==b&&message.find("cancelled")!=std::string::npos,"cancelled analysis preserves prior B and releases busy state");
        Check(owned&&WaitForSingleObject(owned,500)==WAIT_OBJECT_0,"cancellation terminates only the owned analyzer process");
        if(owned)CloseHandle(owned);
        std::error_code error;fs::remove_all(folder,error);
    }
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
