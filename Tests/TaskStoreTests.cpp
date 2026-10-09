// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TaskStore.hpp"
#include "TaskTrace.hpp"
#include "NativeTrace.hpp"
#include "PagedTimingStore.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
using namespace cyinsights;
static const char* header="Event,Time,TaskId,ThreadId,RelatedId,Name,Tracked,TargetThread,Size\n";
int main(int argc,char** argv) {
    if(argc==3&&std::string(argv[1])=="--diagnose") {
        auto source=std::filesystem::u8path(argv[2]);auto binary=source;binary+=L".cyindex";
        auto store=TaskStore::Build(source,binary,10000);auto rows=store->Query(-1,10000,-1,"",0);
        uint64_t executed=0,queued=0,incomplete=0;store->Visit([&](const TaskRecord& r){executed+=std::isfinite(r.Duration(TaskPhase::Started,TaskPhase::Finished));queued+=std::isfinite(r.Duration(TaskPhase::Scheduled,TaskPhase::Started));incomplete+=!std::isfinite(r.time[4]);});
        std::cout<<"tasks="<<store->count<<" events="<<store->events<<" links="<<store->relations<<" execution_durations="<<executed<<" queue_durations="<<queued<<" incomplete="<<incomplete<<" page="<<rows.rows.size()<<" threads="<<store->threads.size()<<'\n';return 0;
    }
    if(argc==3){if(std::string(argv[1]).find("slow-task")!=std::string::npos)Sleep(10000);std::ofstream(argv[2])<<header<<"Started,1,1,5,0,,0,0,0\nFinished,2,1,5,0,,0,0,0\nEnd,0,2,0,0,,0,0,0\n";return 0;}
    int checks=0,failures=0;auto check=[&](bool v,const char* n){++checks;if(!v){++failures;std::cerr<<n<<'\n';}};
    auto folder=std::filesystem::temp_directory_path()/("CyTaskTests_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(folder);
    auto journal=folder/"tasks.csv",binary=folder/"tasks.bin";uint64_t big=18014398509482107ull;
    std::ofstream out(journal);out<<header<<"Thread,0,10,10,0,Worker,0,0,0\nInit,0,0,0,0,,0,0,0\n"
        <<"Launched,1,"<<big<<",2,0,\"Quoted, \"\"task\"\"\",1,255,128\n"
        <<"Scheduled,2,"<<big<<",2,0,,0,0,0\nStarted,3,"<<big<<",10,0,,0,0,0\n"
        <<"Finished,5,"<<big<<",10,0,,0,0,0\nCompleted,6,"<<big<<",10,0,,0,0,0\nDestroyed,7,"<<big<<",10,0,,0,0,0\n"
        <<"SubsequentAdded,2,"<<big<<",2,8,,0,0,0\nStarted,4,8,11,0,,0,0,0\nEnd,0,8,0,0,,0,0,0\n";out.close();
    auto store=TaskStore::Build(journal,binary,10);
    check(store->count==2&&store->events==8&&store->relations==1,"Every task and recorded link retained, including relation-only partial identities");
    check(store->start==1&&store->end==10,"Task time domain retains earliest observation and supplied capture end");
    auto task=store->Get(big);check(task&&task->id==big&&store->names[task->name]=="Quoted, \"task\"","64-bit native identity and quoted names preserved");
    check(task&&task->Duration(TaskPhase::Started,TaskPhase::Finished)==2&&task->Duration(TaskPhase::Scheduled,TaskPhase::Started)==1,"Execution and queue durations use matching native lifecycle timestamps");
    check(task&&task->Duration(TaskPhase::Launched,TaskPhase::Scheduled)==1&&task->size==128&&(task->flags&1),"Dependency delay and launch metadata retained");
    auto partial=store->Get(8);check(partial&&std::isnan(partial->Duration(TaskPhase::Started,TaskPhase::Finished))&&!(partial->flags&4),"Missing finish/tracking metadata remains unavailable");
    auto page=store->Query(0,10,-1,"",0);check(page.matched==2&&page.rows.size()==2,"Native task paging counts all matches");
    check(store->Query(0,10,10,"",0).matched==1,"Execution thread filter uses native ID");
    check(store->Query(0,10,-1,"Quoted",0).matched==1&&store->Query(0,10,-1,std::to_string(big),0).matched==1,"Search by name and exact decimal identity");
    check(store->Query(6,9,-1,"",0).matched==1,"Completed execution excluded; unfinished execution crosses selected interval");
    auto links=store->Links(big);check(links.size()==1&&links[0].from==big&&links[0].to==8,"Raw recorded edge direction retained without guessed dependency kind");
    check(!store->Get(99),"Absent IDs do not map to another task");
    std::atomic<bool> cancel{true};bool cancelled=false;try{store->Query(0,10,-1,"",0,&cancel);}catch(...){cancelled=true;}check(cancelled,"Native task queries respect cancellation");
    std::ofstream(journal)<<header<<"Started,1,1,5,0,,0,0,0\n";bool failed=false;auto incomplete=folder/"incomplete.bin";try{TaskStore::Build(journal,incomplete,10);}catch(...){failed=true;}
    check(failed&&!std::filesystem::exists(incomplete),"Incomplete helper output never publishes a cache");
    std::ofstream(journal)<<header<<"Init,0,7,0,0,,0,0,0\nEnd,0,0,0,0,,0,0,0\n";failed=false;try{TaskStore::Build(journal,incomplete,10);}catch(...){failed=true;}check(failed,"Unknown task protocol rejected explicitly");
    std::ofstream(journal)<<header<<"Started,1,1,5,0,,0,0,0\nStarted,2,1,5,0,,0,0,0\nEnd,0,2,0,0,,0,0,0\n";
    auto conflict=TaskStore::Build(journal,incomplete,10);check(conflict->Get(1)->flags&2,"Conflicting lifecycle observations flagged, not silently changed");
    auto many=folder/"many.bin";out.open(journal);out<<header;
    for(int i=0;i<6000;++i)out<<"Started,1,"<<i<<",10,0,,0,0,0\n";
    for(int i=0;i<6000;++i)out<<"Finished,2,"<<i<<",10,0,,0,0,0\n";
    out<<"End,0,12000,0,0,,0,0,0\n";out.close();auto large=TaskStore::Build(journal,many,10);
    check(large->count==6000&&large->Get(0)->Duration(TaskPhase::Started,TaskPhase::Finished)==1&&large->Get(5999)->Duration(TaskPhase::Started,TaskPhase::Finished)==1,"Working-cache flush/readback preserves tasks across more than 4096 active records");
    check(large->Query(0,10,-1,"",46).rows.size()==112&&large->Query(0,10,-1,"",46).matched==6000,"Last page retains remainder without limiting total task count");
    check(large->Query(0,10,-1,"",UINT64_MAX).rows.empty(),"Task page offsets cannot wrap");
    std::ofstream(journal)<<header<<"Started,20,1,10,0,,0,0,0\nEnd,0,1,0,0,,0,0,0\n";
    auto outside=TaskStore::Build(journal,folder/"outside.bin",10);
    check(outside->end==20&&outside->Query(outside->start,std::nextafter(outside->end,INFINITY),-1,"",0).matched==1,"Whole-task domain includes events outside CPU scope range and the final incomplete observation");
    check(IsTaskScope("ExecuteForegroundTask")&&!IsTaskScope("WaitForTasks")&&!IsTaskScope("Unrelated"),"CPU candidates are labeled execution scopes, not waits or all timers");
    auto csv=folder/"timing.csv";std::ofstream(csv)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n1,Worker,3,ExecuteForegroundTask,1,2,0\n1,Worker,4,WaitForTasks,2,3,0\n";
    NativeReadProgress progress;std::string error;auto timings=PagedTimingStore::Build(csv,folder/"timing.bin",progress,error);
    auto candidates=TaskStore{}.ScopeCandidates(*timings,0,10,-1,"",0);check(candidates.matched==1&&candidates.rows[0].time[3]==1&&std::isnan(candidates.rows[0].time[2]),"Candidate timings keep native scope times without inventing queue lifecycle");
    std::ofstream(folder/"UnrealInsights-TraceAnalysis.dll")<<"test-owned presence marker, never loaded";
    auto trace=folder/"fixture-task.utrace";std::ofstream(trace)<<"unchanged fixture";
    TaskTrace reader;auto own=std::filesystem::absolute(argv[0]);check(reader.Start(own,folder,trace,folder/"output",10,error),"Background native decoder starts without rebuilding engine");
    std::shared_ptr<TaskStore> decoded;std::string message;auto poll=[&]{auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(reader.Busy()&&std::chrono::steady_clock::now()<limit){reader.Poll(decoded,message);std::this_thread::sleep_for(std::chrono::milliseconds(1));}return !reader.Busy();};
    check(poll()&&decoded&&decoded->count==1,"Background decoder and disk indexing publish a complete native dataset");
    auto slow=folder/"slow-task.utrace";std::ofstream(slow)<<"unchanged";check(reader.Start(own,folder,slow,folder/"output",10,error),"Owned slow decoder starts");reader.Cancel();decoded.reset();
    check(poll()&&!decoded&&message.find("cancelled")!=std::string::npos,"Cancellation stops only owned decoder and publishes no partial dataset");
    check(std::filesystem::file_size(trace)==17&&std::filesystem::file_size(slow)==9,"Source trace fixtures unchanged by decoder and cancellation");
    // All recursive cleanup is restricted to this exact test-owned temp directory.
    std::filesystem::remove_all(folder);
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
