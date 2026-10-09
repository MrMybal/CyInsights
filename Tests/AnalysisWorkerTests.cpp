// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AnalysisWorkers.hpp"
#include <future>
#include <iostream>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include <stdexcept>
#include <climits>
using namespace cyinsights;
int main() {
    int checks=0,failures=0;auto check=[&](bool v,const char* name){++checks;if(!v){++failures;std::cerr<<name<<'\n';}};
    auto caller=std::this_thread::get_id();std::mutex mutex;std::set<std::thread::id> threads;
    std::vector<std::atomic<int>> visits(64);for(auto& value:visits)value.store(0);
    ParallelAnalysis(visits.size(),8,[&](size_t i){++visits[i];{std::lock_guard<std::mutex> lock(mutex);threads.insert(std::this_thread::get_id());}std::this_thread::sleep_for(std::chrono::milliseconds(1));});
    bool exact=true;for(auto& value:visits)exact&=value.load()==1;
    check(exact,"Parallel batches execute every job exactly once");
    check(AnalysisWorkerLimit()==1||threads.size()>1,"Sufficient work uses multiple real threads");
    check(threads.size()<=AnalysisWorkerLimit(),"One batch respects its participant budget");
    threads.clear();SetAnalysisWorkerCount(1);
    ParallelAnalysis(32,0,[&](size_t){threads.insert(std::this_thread::get_id());});
    check(threads.size()==1&&*threads.begin()==caller,"Reference mode executes on the calling thread");
    SetAnalysisWorkerCount(UINT_MAX);check(AnalysisWorkerCount()==AnalysisWorkerLimit(),"Worker settings respect the CPU limit");SetAnalysisWorkerCount(0);
    std::atomic<int> active{0};bool thrown=false;
    struct Active {std::atomic<int>& count;Active(std::atomic<int>& v):count(v){++count;}~Active(){--count;}};
    try{ParallelAnalysis(128,8,[&](size_t i){Active guard(active);if(i==3)throw std::runtime_error("Injected worker error");std::this_thread::sleep_for(std::chrono::milliseconds(1));});}catch(const std::runtime_error&){thrown=true;}
    check(thrown&&active.load()==0,"Worker errors propagate only after running tasks are drained");
    std::atomic<bool> cancel{false};thrown=false;
    try{ParallelAnalysis(128,8,[&](size_t i){Active guard(active);if(i==3)cancel.store(true);std::this_thread::sleep_for(std::chrono::milliseconds(1));},&cancel);}catch(const std::runtime_error&){thrown=true;}
    check(thrown&&active.load()==0,"Mid-batch cancellation drains tasks before returning");
    int forbidden=0;try{ParallelAnalysis(16,8,[&](size_t){++forbidden;},&cancel);}catch(const std::runtime_error&){}
    check(forbidden==0,"Already-cancelled batches start no work");
    std::atomic<int> nested{0};ParallelAnalysis(16,8,[&](size_t){ParallelAnalysis(12,8,[&](size_t){++nested;});});
    check(nested.load()==192,"Nested analysis completes without pool starvation or deadlock");
    std::atomic<int> concurrent{0};auto read=[&]{ParallelAnalysis(64,8,[&](size_t){++concurrent;});};
    auto a=std::async(std::launch::async,read);auto b=std::async(std::launch::async,read);a.get();b.get();
    check(concurrent.load()==128,"Independent concurrent callers share the worker pool safely");
    ParallelAnalysis(0,8,[&](size_t){++forbidden;});check(forbidden==0,"Empty batches start no work");
    check(AnalysisWorkerCount()>=1&&AnalysisWorkerCount()<=AnalysisWorkerLimit(),"Automatic mode remains valid after exceptions and cancellation");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
