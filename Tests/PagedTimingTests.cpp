// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "PagedTimingStore.hpp"
#include "NativeTrace.hpp"
#include "AnalysisWorkers.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
using namespace cyinsights;
int main(int argc,char** argv) {
    auto csv=argc>1?std::filesystem::u8path(argv[1]):std::filesystem::temp_directory_path()/("CyPaged_"+std::to_string(GetCurrentProcessId())+".csv");
    auto binary=argc>2?std::filesystem::u8path(argv[2]):csv.parent_path()/("CyPaged_"+std::to_string(GetCurrentProcessId())+".bin");
    NativeReadProgress progress;std::string error;
    if(argc>1) {
        auto begin=std::chrono::steady_clock::now();auto store=PagedTimingStore::Build(csv,binary,progress,error,csv.extension()==".cynt");
        if(!store){std::cerr<<error;return 1;}
        const double index_seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
        double a=(store->start+store->end)/2,b=std::min(store->end,a+.05);
        if(!store->frames.empty()){auto frame=store->frames[store->frames.size()/2];a=frame.first;b=frame.second;}
        auto window=store->Window(a,b,a,b,2000);auto overview=store->Window(store->start,store->end,store->start,store->end,2000);
        uint64_t represented=0;for(const auto& e:overview.events)represented+=e.represented;
        std::cout<<"Indexed events: "<<store->count<<" / disk bytes: "<<std::filesystem::file_size(binary)<<"\n"
            <<"Index seconds: "<<index_seconds<<"\n"
            <<"Window render records: "<<window.events.size()<<" / overview render records: "<<overview.events.size()<<" representing "<<represented<<" events\n"
            <<"Elapsed seconds: "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()<<"\n";
        return represented==store->count?0:1;
    }
    int checks=0,failures=0;auto check=[&](bool v,const char* name){++checks;if(!v){++failures;std::cerr<<name<<"\n";}};
    std::ofstream out(csv);out<<std::setprecision(17)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n";
    out<<"1,GameThread,1,FEngineLoop::Tick,0,10,0\n1,GameThread,2,Outer,1,9,1\n1,GameThread,3,Child,2,3,2\n";
    for(int i=0;i<5000;++i)out<<"2,Worker,4,Tiny,"<<4.+i*.00000001<<","<<4.000000001+i*.00000001<<",0\n";
    out<<"3,Worker,5,Unfinished,5,inf,0\n";out.close();
    auto store=PagedTimingStore::Build(csv,binary,progress,error);
    check(store&&store->count==5004,"All source events are retained in the disk cache");if(!store){std::cerr<<error;return 1;}
    check(progress.bytes.load()==progress.total.load()&&progress.events.load()==5004,"Streaming progress reaches the exact byte and event totals");
    auto mask=store->FunctionMask("Child");
    ScopeVisitMetrics visit_metrics;std::vector<StoredScope> chosen;
    store->VisitScopes(SIZE_MAX,0,10,false,[&](const StoredScope& s){chosen.push_back(s);},nullptr,&mask,&visit_metrics);
    check(chosen.size()==1&&chosen[0].start==2&&chosen[0].end==3&&visit_metrics.records_scanned<5004,"Timer postings skip unrelated disk blocks and retain full measured durations");
    chosen.clear();store->VisitScopes(SIZE_MAX,2.1,2.9,false,[&](const StoredScope& s){chosen.push_back(s);},nullptr,&mask);
    check(chosen.empty(),"Histogram uses starts inside selected range, not a clipped parent duration");
    chosen.clear();store->VisitScopes(SIZE_MAX,2,3,false,[&](const StoredScope& s){chosen.push_back(s);},nullptr,&mask);
    check(chosen.size()==1&&chosen[0].track==0,"All-thread query preserves native track identity and left-inclusive boundary");
    std::vector<uint8_t> empty_mask(mask.size());ScopeVisitMetrics empty_metrics;
    store->VisitScopes(SIZE_MAX,0,10,false,[](const StoredScope&){},nullptr,&empty_mask,&empty_metrics);
    check(empty_metrics.records_scanned==0&&empty_metrics.blocks_read==0,"Absent function requires no disk reads");
    auto narrow=store->Window(2.2,2.8,2.2,2.8,2000);
    check(narrow.events.size()==3,"Interval lookup keeps parent scopes crossing both window boundaries");
    TimingModel model;model.Build(narrow.events);store->ApplyTotals(narrow,model);
    check(model.start==0&&model.end==10&&store->frames.size()==1,"Window model keeps the entire capture extent and global frame overview");
    check(store->frame_diagnostics.size()==1&&std::abs(store->frame_diagnostics[0].work[0].total-10)<1e-8,
        "Paged indexing computes frame coverage from full raw scopes, not display aggregates");
    check(!store->frame_diagnostics[0].work[2].available,"CPU-only capture does not invent GPU cost");
    double inclusive=0,exclusive=0;for(const auto& t:model.totals){inclusive+=t.inclusive;exclusive+=t.exclusive;}
    check(std::abs(inclusive-1.8)<1e-8&&std::abs(exclusive-.6)<1e-8,"Streamed inclusive/exclusive statistics use real clipped scopes");
    auto overview=store->Window(0,10,0,10,100);
    TimingModel overview_model;overview_model.Build(overview.events);store->ApplyTotals(overview,overview_model);
    auto summary=store->Summary(overview,overview_model);
    auto child=std::find_if(summary.begin(),summary.end(),[](const auto& value){return value.group.key.name=="Child";});
    check(child!=summary.end()&&child->count==1&&child->inclusive==1&&child->exclusive==1,"Comparison summaries use full native records even when display scopes are aggregated");
    auto unfinished=std::find_if(summary.begin(),summary.end(),[](const auto& value){return value.group.key.name=="Unfinished";});
    check(unfinished!=summary.end()&&unfinished->count==0&&unfinished->unfinished==1,"Disk summaries retain unfinished observations without inventing measured cost");
    uint64_t represented=0;for(const auto& e:overview.events)represented+=e.represented;
    check(overview.aggregated&&represented==5004&&overview.events.size()<5004,"Pixel aggregation conserves all events while reducing display memory");
    check(std::any_of(overview.events.begin(),overview.events.end(),[](const auto& e){return e.represented>1&&!e.has_timer_id&&e.display_aggregate;}),"Aggregated display cannot claim a unique native scope or timer identity");
    auto nearest=store->Nearest(3,true,"Child",6);
    check(nearest&&nearest->start==2&&nearest->end==3,"Timer focus searches disk data beyond the loaded window");
    std::atomic<bool> cancel{true};bool cancelled=false;try{store->Window(0,10,0,10,100,&cancel);}catch(const std::exception&){cancelled=true;}
    check(cancelled,"Window queries honor cancellation");
    auto other=binary;other+=L".cancel";progress.cancel.store(true);
    check(!PagedTimingStore::Build(csv,other,progress,error)&&!std::filesystem::exists(other),"Cancelled indexing publishes no partial cache");
    progress.cancel.store(false);std::ofstream(csv)<<"ThreadName,TimerName,StartTime,EndTime,Depth\nWorker,A,5,6,0\nWorker,B,1,2,0\n";
    auto unordered=PagedTimingStore::Build(csv,other,progress,error);
    check(unordered&&!unordered->relations_valid&&unordered->count==2,"Unordered source retains scopes and explicitly marks global nesting unavailable");
    progress.cancel.store(true);std::vector<TimingEvent> untouched(1);untouched.front().name="Keep";
    check(!NativeTrace::ReadCsv(csv,untouched,error,&progress)&&untouched.size()==1&&untouched.front().name=="Keep","Cancelled streaming import preserves the last published dataset");
    auto nav=NavigateTimingWindow(2,4,0,10,3,0);
    check(nav.first==5&&nav.second==7,"Horizontal navigation pans without changing span");
    nav=NavigateTimingWindow(2,4,0,10,0,std::log(.5));
    check(std::abs(nav.first-2.5)<1e-9&&std::abs(nav.second-3.5)<1e-9,"Vertical navigation zooms around its anchor");
    nav=NavigateTimingWindow(2,4,0,10,1000,1000);
    check(nav.first==0&&nav.second==10,"Extreme navigation stays inside the entire capture");
    progress.cancel.store(false);
    {
        std::ofstream fixture(csv);fixture<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n";
        fixture<<"1,Worker,1,Outer,0,12000,0\n1,Worker,3,Unfinished,0,inf,2\n";
        for(int i=0;i<12000;++i)fixture<<"1,Worker,2,Shared,"<<i<<','<<i+1<<",1\n";
        for(int i=0;i<12000;++i)fixture<<"2,GPU,2,Shared,"<<i<<','<<i+1<<",0\n";
    }
    auto indexed=PagedTimingStore::Build(csv,other,progress,error);
    check(indexed&&indexed->count==24002,"Many-block interval index retains every CPU/GPU record");
    if(indexed) {
        auto selected=indexed->Window(9000.25,9000.75,100.25,100.75,100);
        uint64_t visible=0;for(const auto& e:selected.events)visible+=e.represented;
        check(visible==4,"Separate display and statistics intervals retain long and unfinished crossing scopes");
        check(selected.records_scanned<indexed->count/2,"Narrow reads visit intersecting lane blocks instead of the entire capture");
        auto repeated=indexed->Window(9000.25,9000.75,100.25,100.75,100);
        check(selected.blocks_read>0&&repeated.blocks_read==0&&repeated.cache_hits==selected.block_candidates,"Repeated reads reuse disk blocks without repeating I/O");
        TimingModel exact;exact.Build(selected.events);indexed->ApplyTotals(selected,exact);
        check(!exact.tracks.empty()&&exact.tracks.front().gpu,"GPU remains above CPU after paged statistics publication despite file order");
        auto values=indexed->Summary(selected,exact);
        auto cpu=std::find_if(values.begin(),values.end(),[](const auto& v){return v.group.key.name=="Shared"&&!v.group.key.gpu;});
        auto gpu=std::find_if(values.begin(),values.end(),[](const auto& v){return v.group.key.name=="Shared"&&v.group.key.gpu;});
        check(cpu!=values.end()&&gpu!=values.end()&&cpu->count==1&&gpu->count==1&&cpu->inclusive==.5&&gpu->inclusive==.5,"Cached domains keep CPU/GPU statistics separate for the same native TimerId");
        auto outer=std::find_if(exact.totals.begin(),exact.totals.end(),[&](const auto& t){return exact.names[t.timer]=="Outer";});
        check(outer!=exact.totals.end()&&outer->inclusive==.5&&outer->exclusive==0,"Interval indexing preserves exclusive cost of crossing parents");
        auto all=indexed->Window(indexed->start,indexed->end,indexed->start,indexed->end,10);
        visible=0;for(const auto& e:all.events)visible+=e.represented;
        check(all.aggregated&&visible==indexed->count,"Full overview after cached/disjoint queries still represents every event");
        auto at_boundary=indexed->Window(2048,2048.5,2048,2048.5,100);
        visible=0;for(const auto& e:at_boundary.events)visible+=e.represented;
        check(visible==6,"Closed display boundaries include scopes ending at a block/window boundary");
        auto after=indexed->Window(9000.25,9000.75,100.25,100.75,100);
        const bool same_totals=std::equal(after.totals.begin(),after.totals.end(),selected.totals.begin(),[](const auto& a,const auto& b){
            return a.count==b.count&&a.unfinished==b.unfinished&&a.inclusive==b.inclusive&&a.exclusive==b.exclusive;});
        check(same_totals&&after.events.size()==selected.events.size(),"Changing zoom does not contaminate cached statistics or display density");
        cancel.store(true);cancelled=false;try{indexed->Window(9000.25,9000.75,100.25,100.75,100,&cancel);}catch(const std::exception&){cancelled=true;}
        check(cancelled,"Cancellation remains effective when every requested block is cached");
        auto totals_equal=[](const auto& a,const auto& b){return a.size()==b.size()&&std::equal(a.begin(),a.end(),b.begin(),[](const auto& x,const auto& y){return x.count==y.count&&x.unfinished==y.unfinished&&x.inclusive==y.inclusive&&x.exclusive==y.exclusive;});};
        auto equal=[&](const TimingWindow& a,const TimingWindow& b) {
            if(!totals_equal(a.totals,b.totals)||!totals_equal(a.cpu_totals,b.cpu_totals)||!totals_equal(a.gpu_totals,b.gpu_totals)||a.events.size()!=b.events.size())return false;
            for(size_t i=0;i<a.events.size();++i){const auto& x=a.events[i];const auto& y=b.events[i];
                if(x.name!=y.name||x.thread!=y.thread||x.start!=y.start||x.end!=y.end||x.depth!=y.depth||x.represented!=y.represented||x.timer_id!=y.timer_id||x.thread_id!=y.thread_id||x.has_timer_id!=y.has_timer_id||x.open_ended!=y.open_ended||x.frame_type!=y.frame_type||x.display_aggregate!=y.display_aggregate)return false;}
            return true;
        };
        auto reference=indexed->Window(0,12000,100.25,11900.75,40,nullptr,1);
        bool all_workers=true;for(unsigned workers:{2u,4u,8u})all_workers&=equal(reference,indexed->Window(0,12000,100.25,11900.75,40,nullptr,workers));
        check(all_workers,"Worker counts retain bit-identical display ordering, identities and all per-timer/domain statistics");
        auto sparse_reference=indexed->Window(9000,9001,100.25,100.75,70000,nullptr,1);
        check(equal(sparse_reference,indexed->Window(9000,9001,100.25,100.75,70000,nullptr,8)),"Sparse pixel lookup preserves concurrent results when dense lookup is unsuitable");
        auto concurrent=std::async(std::launch::async,[&]{return indexed->Window(0,12000,100.25,11900.75,40,nullptr,8);});
        auto another=indexed->Window(0,12000,100.25,11900.75,40,nullptr,4);
        check(equal(reference,concurrent.get())&&equal(reference,another),"Concurrent window queries share the block cache safely");
        indexed->ClearReadCache();auto fresh=indexed->Window(0,12000,100.25,11900.75,40,nullptr,8);
        check(fresh.blocks_read>0&&equal(reference,fresh),"Cache reset affects only I/O reuse, never native results");
    }
    {
        std::ofstream fixture(csv);fixture<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n"
            "16777216,Game Frames,7,Game Frame 7,0,0.03,0\n"
            "3,GPU1,20,WaitForOcclusion,0,0.024,0\n"
            "4,GameThread,10,WaitUntilTasksComplete,0,0.03,0\n"
            "4,GameThread,11,TaskWork,0.01,0.012,1\n";
    }
    auto diagnosis=PagedTimingStore::Build(csv,other,progress,error);
    check(diagnosis&&diagnosis->frames_native&&diagnosis->frame_ids==std::vector<uint64_t>{7}&&diagnosis->count==4,
        "Measured frame export retains identity and every source scope for diagnostics");
    if(diagnosis) {
        const auto& d=diagnosis->frame_diagnostics[0];
        check(std::abs(d.work[0].active-.002)<1e-9&&std::abs(d.work[0].wait-.028)<1e-9,
            "Raw import excludes frame records from work and keeps observed tasks inside waits");
        check(std::abs(d.work[2].total-.024)<1e-9&&d.work[2].wait==0&&DiagnoseFrame(d,.03,1./60)==FrameCause::Gpu,
            "GPU pass names are not CPU waits; real GPU scope union can be the candidate");
    }
    {
        std::ofstream fixture(csv);fixture<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n"
          "1,GameThread,10,Foo,0,1,0\n1,GameThread,11,FooExtra,1,2,0\n"
          "2,Worker,12,Foo,2,3,0\n2,Worker,12,Foo,3,4,0\n2,Worker,13,foo,4,5,0\n";
    }
    auto names=PagedTimingStore::Build(csv,other,progress,error);auto exact=names->FunctionMask("Foo");std::vector<StoredScope> functions;
    names->VisitScopes(SIZE_MAX,0,5,false,[&](const StoredScope& s){functions.push_back(s);},nullptr,&exact);
    check(functions.size()==3&&std::count(exact.begin(),exact.end(),1)==2,"Exact name unites distinct native IDs and threads without matching FooExtra or foo");
    functions.clear();names->VisitScopes(1,3,4,false,[&](const StoredScope& s){functions.push_back(s);},nullptr,&exact);
    check(functions.size()==1&&functions[0].start==3,"Thread and timeline-range filters intersect exact function selection");
    auto marked=names->Window(0,5,0,5,1,nullptr,1,"Foo"),unmarked=names->Window(0,5,0,5,1,nullptr,1);
    check(marked.events.size()==2&&std::all_of(marked.events.begin(),marked.events.end(),[](const auto& e){return e.display_aggregate&&e.exact_name_match&&!e.has_timer_id;}),"Mixed display aggregates retain exact-name membership without inventing unique identity");
    check(std::equal(marked.totals.begin(),marked.totals.end(),unmarked.totals.begin(),[](auto a,auto b){return a.count==b.count&&a.inclusive==b.inclusive&&a.exclusive==b.exclusive;}),"Function highlighting preserves every unfiltered source statistic");
    auto query=[&](const std::string& name){auto m=names->FunctionMask(name);size_t count=0;names->VisitScopes(SIZE_MAX,0,5,false,[&](const StoredScope&){++count;},nullptr,&m);return count;};
    auto concurrent=std::async(std::launch::async,[&]{return query("Foo");});
    check(query("FooExtra")==1&&concurrent.get()==3,"Concurrent function queries retain exact results while sharing disk blocks");
    {
        std::ofstream fixture(csv);fixture<<std::setprecision(17)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n"
            <<"9,GPU,100,MobileSceneRender,0,0.02,0\n";
        for(int i=0;i<5;++i)fixture<<"9,GPU,"<<101+i<<",Group"<<i<<","<<.019+i*1e-6<<","<<.019+i*1e-6+1e-6<<",1\n";
    }
    auto quest=PagedTimingStore::Build(csv,other,progress,error);auto quest_window=quest->Window(0,.02,0,.02,10);
    check(quest_window.aggregated&&quest_window.gpu_details.size()==1&&quest_window.gpu_details[0].Limited()&&quest_window.gpu_details[0].nested==5,
        "Raw GPU timing diagnostic survives pixel aggregation of differently named near-zero groups");
    TimingModel quest_model;quest_model.Build(quest_window.events,false);quest->ApplyTotals(quest_window,quest_model);
    check(quest_model.tracks[0].gpu_detail_source.Limited(),"Paged GPU detail diagnostic reaches its native track identity");
    auto outside=quest->Window(.1,.2,.1,.2,10);
    check(!outside.gpu_details[0].Limited(),"GPU timing warning does not survive a query outside the measured interval");
    std::filesystem::remove(csv);std::filesystem::remove(binary);std::filesystem::remove(other);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
