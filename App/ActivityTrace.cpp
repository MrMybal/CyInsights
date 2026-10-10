// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ActivityTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <stdexcept>
#include <sstream>
#include <iomanip>
namespace cyinsights {
ActivityTrace::~ActivityTrace(){Cancel();if(task_.valid())task_.wait();}
bool ActivityTrace::Start(const std::filesystem::path& helper,const std::filesystem::path& binaries,const std::filesystem::path& trace,
    const std::filesystem::path& output,std::string& error) {
    if(Busy()){error="Activity analysis already running";return false;}
    if(!std::filesystem::is_regular_file(helper)||!std::filesystem::is_regular_file(binaries/L"UnrealInsights-TraceAnalysis.dll")){
        error="Native activity adapter unavailable for the selected Unreal installation";return false;}
    try {
        auto run=output/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());std::filesystem::create_directories(run);
        journal_=run/"activity.csv";cancel_.store(false);phase_.store(1);
        task_=std::async(std::launch::async,[this,helper,binaries,trace,journal=journal_]{
            Result result;HANDLE process=nullptr;
            try {
                std::string problem;if(!NativeTrace::Launch(helper,NativeTrace::Quote(trace.wstring())+L" "+NativeTrace::Quote(journal.wstring())+L" "+NativeTrace::Quote(binaries.wstring()),&process,problem,binaries))throw std::runtime_error(problem);
                while(WaitForSingleObject(process,50)==WAIT_TIMEOUT)if(cancel_.load()){TerminateProcess(process,1);WaitForSingleObject(process,INFINITE);throw std::runtime_error("Activity analysis cancelled");}
                DWORD code=0;
                if(!GetExitCodeProcess(process,&code))throw std::runtime_error("Cannot read native activity decoder exit status; source trace preserved");
                if(code!=0){std::ostringstream diagnostic;diagnostic<<"Native activity decoder failed (0x"<<std::hex<<std::uppercase<<std::setw(8)<<std::setfill('0')<<code<<")";if(code==0xC0000005u)diagnostic<<": invalid memory access";diagnostic<<"; source trace preserved. Check that the adapter matches the selected Unreal installation.";throw std::runtime_error(diagnostic.str());}
                CloseHandle(process);process=nullptr;phase_.store(2);
                result.store=ActivityStore::Build(journal,journal.parent_path()/"activity.cyindex",&cancel_);phase_.store(3);
            }catch(const std::exception& e){result.error=e.what();phase_.store(4);}
            if(process){if(WaitForSingleObject(process,0)==WAIT_TIMEOUT){TerminateProcess(process,1);WaitForSingleObject(process,INFINITE);}CloseHandle(process);}
            return result;
        });return true;
    }catch(const std::exception& e){error=e.what();phase_.store(4);return false;}
}
bool ActivityTrace::Poll(std::shared_ptr<ActivityStore>& store,std::string& message) {
    if(!task_.valid()||task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    try{auto result=task_.get();if(!result.error.empty())message=result.error;else {store=std::move(result.store);message=store->count?"Native activities ready: "+std::to_string(store->count):"No network, file, loadtime or memory events recorded in this trace";}}
    catch(const std::exception& e){message=e.what();}return true;
}
}
