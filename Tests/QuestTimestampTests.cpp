// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "QuestTimestampDispatch.h"
#include "QuestPassClock.h"
#include <iostream>
using Function=void(*)(unsigned,unsigned,unsigned);
cyinsights_quest::TimestampDispatch<Function> dispatch;
unsigned stage_seen=0,pool_seen=0,query_seen=0,calls=0;
void Driver(unsigned stage,unsigned pool,unsigned query){stage_seen=stage;pool_seen=pool;query_seen=query;++calls;}
void Wrapper(unsigned stage,unsigned pool,unsigned query){dispatch.Next()(dispatch.Transform(stage,1u,8192u),pool,query);}
void Other(unsigned,unsigned,unsigned){}
int main(){
    int failures=0,checks=0;
    auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}};
    Function slot=nullptr;check(!dispatch.Install(slot,Wrapper),"No wrapper when driver dispatch unavailable");
    slot=Driver;check(dispatch.Install(slot,Wrapper),"Install onto an existing driver dispatch");
    slot(1,7,42);check(stage_seen==1&&pool_seen==7&&query_seen==42&&calls==1,"Disabled capture forwards unchanged query once");
    dispatch.SetEnabled(true);slot(1,8,43);check(stage_seen==8192&&pool_seen==8&&query_seen==43&&calls==2,"Completion capture rewrites actual forwarded stage, preserving pool/index");
    slot(2048,9,44);check(stage_seen==2048,"Other pipeline stages retained");
    slot(8192,9,44);check(stage_seen==8192,"Existing completion stage retained");
    dispatch.Disable();slot(1,10,45);check(stage_seen==1,"Stop restores native timestamp semantics");
    check(dispatch.Install(slot,Wrapper),"Next capture reuses wrapper without recursive chaining");
    dispatch.SetEnabled(true);slot(1,11,46);check(stage_seen==8192,"Repeated capture reactivates stage transformation");
    slot=Other;check(!dispatch.Install(slot,Wrapper),"Later interceptor cannot be replaced");
    dispatch.Remove(slot,Wrapper);check(slot==Other,"Shutdown preserves later owner's dispatch");
    Wrapper(1,12,47);check(stage_seen==1&&pool_seen==12,"A later owner can still chain through disabled wrapper");
    slot=Wrapper;dispatch.Remove(slot,Wrapper);check(slot==Driver,"Owned dispatch restored at shutdown");
    double a=0,b=0;
    using cyinsights_quest::AlignPass;using cyinsights_quest::ClockAnchor;
    check(!AlignPass(10,20,{},a,b),"Missing calibration never substitutes CPU receipt time");
    std::vector<ClockAnchor> clocks{{5000000,uint64_t(1000000)-5000000,0}};
    check(AlignPass(5010000,5022345,clocks,a,b)&&std::abs(a-1.01)<1e-9&&std::abs(b-a-.012345)<1e-9,"Signed engine bias wraps correctly; measured GPU duration retained");
    check(!AlignPass(10,9,clocks,a,b),"Reversed GPU timestamps unavailable");
    check(!AlignPass(7000001,7001000,clocks,a,b),"Stale clock calibration not extended indefinitely");
    clocks.push_back({5100000,uint64_t(1000100)-5100000,0});
    check(AlignPass(5090000,5091234,clocks,a,b)&&std::abs(a-.9901)<1e-9,"Nearest clock anchor respects drift");
    clocks={{5000000,0,-4.}};
    check(AlignPass(5010000,5011000,clocks,a,b)&&std::abs(a-1.01)<1e-9,"Direct device-clock sample shares native session origin");
    uint64_t delta=0;
    check(cyinsights_quest::TimestampSpan((1ull<<48)-5,3,48,delta)&&delta==8,"Quest 48-bit counter wrap retains duration");
    check(cyinsights_quest::TimestampSpan(100,125,64,delta)&&delta==25,"Full-width counter duration");
    check(!cyinsights_quest::TimestampSpan(100,90,48,delta),"Ambiguous backward timestamp not fabricated");
    check(!cyinsights_quest::TimestampSpan(100,90,0,delta),"Unsupported queue timestamps rejected");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
