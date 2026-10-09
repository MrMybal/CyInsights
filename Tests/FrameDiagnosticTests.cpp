// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "FrameDiagnostics.hpp"
#include "AnalysisWorkers.hpp"
#include <cmath>
#include <iostream>
#include <random>
#include <algorithm>
using namespace cyinsights;
int main() {
    int checks=0,failures=0;
    auto check=[&](bool value,const char* name){++checks;if(!value){++failures;std::cerr<<name<<'\n';}};
    auto near=[](double a,double b){return std::abs(a-b)<1e-9;};
    check(DiagnosticDomain("GameThread")==FrameDomain::Game,"Identify GameThread");
    check(DiagnosticDomain("RenderThread")==FrameDomain::Render,"Identify RenderThread");
    check(DiagnosticDomain("GPU1")==FrameDomain::Gpu,"Identify native GPU timeline");
    check(DiagnosticDomain("Foreground Worker")==FrameDomain::Other,"Workers do not masquerade as a frame thread");
    check(DiagnosticDomain("GpuWorkerThread")==FrameDomain::Other,"CPU worker with GPU in its name is not a GPU queue");
    check(DiagnosticWait("FTaskGraphInterface::WaitUntilTasksComplete"),"Recognize namespaced task waits");
    check(DiagnosticWait("WaitForTasks")&&DiagnosticWait("WaitFrame"),"Recognize synchronization and pacing waits");
    check(!DiagnosticWait("AwaitedLighting")&&!DiagnosticWait("RHICmdList_FlushRHIThread"),"Do not guess arbitrary wait substrings or flush work");
    std::vector<FrameCoverage> tracks{FrameCoverage(FrameDomain::Game),FrameCoverage(FrameDomain::Render),FrameCoverage(FrameDomain::Gpu)};
    tracks[0].Add(0,.03,false,false);tracks[0].Add(.001,.029,false,false);
    tracks[0].Add(.003,.027,true,false);tracks[0].Add(.006,.02,true,false);
    tracks[1].Add(-.01,.02,false,false);tracks[1].Add(.003,.012,false,false);
    tracks[2].Add(.002,.026,false,false);tracks[2].Add(.004,.01,false,false);
    auto frames=BuildFrameDiagnostics(tracks,{{0,.03},{.03,.06}});
    check(near(frames[0].work[0].total,.03),"Nested CPU scopes counted once");
    check(near(frames[0].work[0].wait,.024)&&near(frames[0].work[0].active,.006),"Overlapping nested waits subtracted once");
    check(near(frames[0].work[1].active,.02),"Render work crossing frame start clipped to common time interval");
    check(near(frames[0].work[2].active,.024),"GPU nested pass timings unioned rather than summed");
    check(!frames[1].work[0].available&&!frames[1].work[2].available,"Missing interval coverage is unavailable, not measured zero");
    check(DiagnoseFrame(frames[0],.03,.016)==FrameCause::Gpu,"GPU candidate beats waiting GameThread");
    auto diagnostic=frames[0];diagnostic.work[2]={};
    check(DiagnoseFrame(diagnostic,.03,.016)==FrameCause::Render,"CPU-only trace can identify RenderThread candidate with GPU unavailable");
    diagnostic.work[1]={};
    check(DiagnoseFrame(diagnostic,.03,.016)==FrameCause::Wait,"Long wait with little productive work is wait/pacing, not CPU workload");
    diagnostic.work[0].wait=0;diagnostic.work[0].active=.025;
    check(DiagnoseFrame(diagnostic,.03,.016)==FrameCause::Game,"GameThread work over budget");
    diagnostic.work[1]={.024,0,.024,true,false};
    check(DiagnoseFrame(diagnostic,.03,.016)==FrameCause::Mixed,"Similar concurrent work is ambiguous rather than unique cause");
    check(DiagnoseFrame(diagnostic,.03,.04)==FrameCause::WithinBudget,"Budget is configurable for frame rate");
    diagnostic.work[0].incomplete=true;
    check(DiagnoseFrame(diagnostic,.03,.016)==FrameCause::Unknown,"Unfinished timing cannot prove a bottleneck");
    check(DiagnoseFrame({},.03,.016)==FrameCause::Unknown,"Uninstrumented slow frame unresolved");
    check(DiagnoseFrame({},.016,.016)==FrameCause::WithinBudget,"Exact budget boundary");
    check(DiagnoseFrame({},.03,0)==FrameCause::Unknown,"Invalid budget");
    FrameCoverage unordered(FrameDomain::Game);
    unordered.Add(5,7,false,false);unordered.Add(1,3,false,false);unordered.Add(2,6,true,false);unordered.Finish();
    auto work=unordered.Query(0,10);
    check(near(work.total,6)&&near(work.wait,4)&&near(work.active,2),"Out-of-order scopes remain complete and union correctly");
    check(near(unordered.Query(2.5,5.5).active,0),"Query can begin and end inside wait union");
    check(!unordered.Query(7,8).available,"Half-open frame boundary never counts ended scopes");
    std::vector<FrameCoverage> queues{FrameCoverage(FrameDomain::Gpu),FrameCoverage(FrameDomain::Gpu)};
    queues[0].Add(0,.02,false,false);queues[1].Add(.005,.025,false,false);
    auto concurrent=BuildFrameDiagnostics(queues,{{0,.03}});
    check(near(concurrent[0].work[2].total,.02),"Parallel GPU queues are not added together as serial workload");
    FrameCoverage unfinished(FrameDomain::Game);unfinished.Add(1,1,false,true);unfinished.Finish();
    check(unfinished.Query(1,2).incomplete&&!unfinished.Query(1,2).available,"Open-ended scope has no invented cost");
    FrameCoverage pumping(FrameDomain::Game);
    pumping.Add(0,10,false,false,0);pumping.Add(1,9,true,false,1);
    pumping.Add(2,7,false,false,2);pumping.Add(3,4,true,false,3);
    pumping.Add(8,8.5,false,false,2);pumping.Finish();auto pumped=pumping.Query(0,10);
    check(near(pumped.wait,3.5)&&near(pumped.active,6.5),"Observed work inside a scheduling wait remains active, nested waits still excluded");
    check(near(pumping.Query(2.5,4.5).wait,1)&&near(pumping.Query(2.5,4.5).active,1),"Exclusive wait coverage clipped across resumed task boundaries");
    FrameCoverage crossing(FrameDomain::Render);crossing.Add(0,2,false,false,0);crossing.Add(1,3,true,false,0);crossing.Finish();
    check(crossing.Query(0,3).incomplete,"Crossing non-nested intervals use conservative wait cost without asserting a unique cause");
    FrameCoverage boundaries(FrameDomain::Game);boundaries.Add(0,1,false,false,0);boundaries.Add(.5,1,true,false,1);
    boundaries.Add(1,2,false,false,0);boundaries.Add(1.5,2,true,false,1);boundaries.Finish();
    check(!boundaries.Query(0,2).incomplete&&near(boundaries.Query(0,2).wait,1),"Coincident child/parent endings do not invalidate consecutive frame nesting");
    // Independent discretized oracle for random overlapping, nested and unordered scopes.
    std::mt19937 random(23);std::vector<bool> busy(1000),wait(1000);FrameCoverage fuzz(FrameDomain::Game);
    for(int i=0;i<700;++i){int a=int(random()%999),b=a+1+int(random()%(1000-a));bool w=random()%3==0;
        fuzz.Add(a,b,w,false);for(int t=a;t<b;++t){busy[t]=true;if(w)wait[t]=true;}}
    fuzz.Finish();bool accurate=true;
    for(int i=0;i<200;++i){int a=int(random()%999),b=a+1+int(random()%(1000-a));double total=0,waiting=0;
        for(int t=a;t<b;++t){total+=busy[t]?1:0;waiting+=wait[t]?1:0;}
        auto measured=fuzz.Query(a,b);accurate&=near(measured.total,total)&&near(measured.wait,waiting)&&near(measured.active,total-waiting);}
    check(accurate,"Random union/clip results agree with independent occupancy oracle");
    std::vector<std::pair<double,double>> many;for(int i=0;i<2000;++i)many.emplace_back(i*.001,i*.001+.03);
    SetAnalysisWorkerCount(1);auto serial=BuildFrameDiagnostics(tracks,many);
    SetAnalysisWorkerCount(0);auto parallel=BuildFrameDiagnostics(tracks,many);bool same=serial.size()==parallel.size();
    for(size_t i=0;i<serial.size();++i)for(size_t d=0;d<3;++d) {auto a=serial[i].work[d],b=parallel[i].work[d];same&=a.total==b.total&&a.wait==b.wait&&a.active==b.active&&a.available==b.available&&a.incomplete==b.incomplete;}
    check(same,"Parallel preparation preserves all frame diagnostic results");
    std::atomic<bool> cancel{true};bool cancelled=false;try{BuildFrameDiagnostics(tracks,many,&cancel);}catch(...){cancelled=true;}
    check(cancelled,"Frame preparation respects cancellation");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
