// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "PagedTimingStore.hpp"
#include "NativeTrace.hpp"
#include "AnalysisWorkers.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <fstream>
using namespace cyinsights;
using Clock=std::chrono::steady_clock;
static double Milliseconds(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
static uint64_t Fingerprint(const TimingWindow& window,const TimingModel& model) {
    uint64_t value=14695981039346656037ull;
    auto integer=[&](uint64_t n){for(int i=0;i<8;++i){value^=(n>>(i*8))&255;value*=1099511628211ull;}};
    auto number=[&](double n){uint64_t bits;std::memcpy(&bits,&n,8);integer(bits);};
    auto text=[&](const std::string& s){integer(s.size());for(unsigned char c:s){value^=c;value*=1099511628211ull;}};
    for(const auto& e:window.events){text(e.thread);text(e.name);number(e.start);number(e.end);integer(e.depth);integer(e.represented);
        integer(e.thread_id);integer(e.timer_id);integer(e.has_thread_id);integer(e.has_timer_id);integer(e.open_ended);integer(e.frame_type);integer(e.frame_index);integer(e.display_aggregate);}
    for(const auto* totals:{&window.totals,&window.cpu_totals,&window.gpu_totals})for(const auto& t:*totals){integer(t.count);integer(t.unfinished);number(t.inclusive);number(t.exclusive);}
    for(auto p:model.parents)integer(p);for(auto t:model.timers)integer(t);for(const auto& name:model.names)text(name);
    return value;
}
int main(int argc,char** argv) {
    if(argc<3||argc>4){std::cerr<<"Usage: CyTimingBenchmark timing.csv disposable.cyindex [--sweep|workers]\n";return 2;}
    const bool sweep=argc==4&&std::string(argv[3])=="--sweep";
    if(argc==4&&!sweep)SetAnalysisWorkerCount(static_cast<unsigned>(std::stoul(argv[3])));
    NativeReadProgress progress;std::string error;auto begun=Clock::now();
    auto store=PagedTimingStore::Build(std::filesystem::u8path(argv[1]),std::filesystem::u8path(argv[2]),progress,error);
    if(!store){std::cerr<<error;return 1;}
    std::cout<<std::setprecision(17)<<"events,"<<store->count<<",index_ms,"<<Milliseconds(begun)<<"\n";
    {
        auto report=std::filesystem::u8path(argv[2]);report+=L".frames.csv";
        std::ofstream output(report);output<<std::setprecision(17)<<"frame_preparation_ms,"<<store->frame_preparation_ms<<"\n";
        output<<"frame,start,end,game_total,game_wait,game_work,game_available,render_total,render_wait,render_work,render_available,gpu_total,gpu_available,cause_60fps,cause_90fps\n";
        for(size_t i=0;i<store->frames.size();++i){const auto& f=store->frame_diagnostics[i];auto interval=store->frames[i];
            output<<(store->frames_native?store->frame_ids[i]:i)<<','<<interval.first<<','<<interval.second;
            for(size_t d=0;d<2;++d){const auto& w=f.work[d];output<<','<<w.total<<','<<w.wait<<','<<w.active<<','<<w.available;}
            output<<','<<f.work[2].total<<','<<f.work[2].available<<','<<FrameCauseName(DiagnoseFrame(f,interval.second-interval.first,1./60))
                <<','<<FrameCauseName(DiagnoseFrame(f,interval.second-interval.first,1./90))<<'\n';}
    }
    double anchor=(store->start+store->end)/2.;
    if(!store->frames.empty())anchor=store->frames[store->frames.size()/2].first;
    std::vector<unsigned> counts=sweep?std::vector<unsigned>{1,2,4,8,0}:std::vector<unsigned>{AnalysisWorkerSetting()};
    for(auto workers:counts) {
    SetAnalysisWorkerCount(workers);store->ClearReadCache();
    std::cout<<"workers,"<<workers<<"\n";
    std::cout<<"span,step,read_ms,build_ms,aggregate_ms,totals_ms,summary_ms,total_ms,render_events,represented,measured_count,unfinished,inclusive,exclusive,blocks_read,cache_hits,records_scanned,workers,fingerprint\n";
    std::vector<double> latencies;
    for(double span:{.05,.5,5.})for(int step=0;step<12;++step) {
        const double a=std::clamp(anchor+span*(step<8?step*.08:(step-8)*.08),store->start,store->end-span),b=a+span;
        const double left=std::max(store->start,a-span),right=std::min(store->end,b+span);
        auto total=Clock::now();auto begin=total;auto window=store->Window(left,right,a,b,float(1800*(right-left)/span));double read=Milliseconds(begin);
        TimingModel model;begin=Clock::now();model.Build(window.events,false);double build=Milliseconds(begin);
        begin=Clock::now();model.Aggregate(window.events,a,b);double aggregate=Milliseconds(begin);
        begin=Clock::now();store->ApplyTotals(window,model);double totals=Milliseconds(begin);
        begin=Clock::now();auto summary=store->Summary(window,model);double summarise=Milliseconds(begin);
        double elapsed=Milliseconds(total);latencies.push_back(elapsed);
        uint64_t represented=0,count=0,unfinished=0;double inclusive=0,exclusive=0;
        for(const auto& e:window.events)represented+=e.represented;
        for(const auto& t:window.totals){count+=t.count;unfinished+=t.unfinished;inclusive+=t.inclusive;exclusive+=t.exclusive;}
        std::cout<<span<<','<<step<<','<<read<<','<<build<<','<<aggregate<<','<<totals<<','<<summarise<<','<<elapsed<<','<<window.events.size()<<','<<represented<<','<<count<<','<<unfinished<<','<<inclusive<<','<<exclusive<<','<<window.blocks_read<<','<<window.cache_hits<<','<<window.records_scanned<<','<<window.workers<<','<<Fingerprint(window,model)<<'\n';
    }
    std::sort(latencies.begin(),latencies.end());
    std::cout<<"median_ms,"<<latencies[latencies.size()/2]<<",p95_ms,"<<latencies[size_t((latencies.size()-1)*.95)]<<'\n';
    }
}
