// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TaskTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <stdexcept>
namespace cyinsights {
TaskTrace::~TaskTrace(){Cancel();if(task_.valid())task_.wait();}
bool TaskTrace::Start(const std::filesystem::path& helper,const std::filesystem::path& binaries,const std::filesystem::path& trace,
    const std::filesystem::path& output,double capture_end,std::string& error) {
    if(Busy()){error="Task analysis already running";return false;}
    if(!std::filesystem::is_regular_file(helper)||!std::filesystem::is_regular_file(binaries/L"UnrealInsights-TraceAnalysis.dll")){
        error="Native task adapter unavailable for the selected Unreal installation";return false;}
    try {
        auto run=output/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());std::filesystem::create_directories(run);
        journal_=run/"tasks.csv";cancel_.store(false);phase_.store(1);
        task_=std::async(std::launch::async,[this,helper,binaries,trace,capture_end,journal=journal_]{
            Result result;HANDLE process=nullptr;
            try {
                std::string problem;if(!NativeTrace::Launch(helper,NativeTrace::Quote(trace.wstring())+L" "+NativeTrace::Quote(journal.wstring()),&process,problem,binaries))throw std::runtime_error(problem);
                while(WaitForSingleObject(process,50)==WAIT_TIMEOUT)if(cancel_.load()){TerminateProcess(process,1);WaitForSingleObject(process,INFINITE);throw std::runtime_error("Task analysis cancelled");}
                DWORD code=0;if(!GetExitCodeProcess(process,&code)||code!=0)throw std::runtime_error("Native task decoder failed (code "+std::to_string(code)+"); source trace preserved");
                CloseHandle(process);process=nullptr;phase_.store(2);
                result.store=TaskStore::Build(journal,journal.parent_path()/"tasks.cyindex",capture_end,&cancel_);phase_.store(3);
            }catch(const std::exception& e){result.error=e.what();phase_.store(4);}
            if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT){TerminateProcess(process,1);WaitForSingleObject(process,INFINITE);}CloseHandle(process);}
            return result;
        });return true;
    }catch(const std::exception& e){error=e.what();phase_.store(4);return false;}
}
bool TaskTrace::Poll(std::shared_ptr<TaskStore>& store,std::string& message) {
    if(!task_.valid()||task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    try{auto result=task_.get();if(!result.error.empty())message=result.error;else {store=std::move(result.store);message=store->count?"Native tasks ready: "+std::to_string(store->count):"No native TaskTrace tasks recorded; CPU task scopes remain available as candidates";}}
    catch(const std::exception& e){message=e.what();}return true;
}
}
