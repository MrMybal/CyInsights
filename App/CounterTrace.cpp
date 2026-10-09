// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "CounterTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <fstream>
#include <stdexcept>
namespace cyinsights {
CounterTrace::~CounterTrace(){cancel_.store(true);if(task_.valid())task_.wait();}
bool CounterTrace::Start(const std::filesystem::path& exe,const std::filesystem::path& trace,
                         const std::filesystem::path& cache,CounterModel catalog,std::string& error) {
    if(Busy()){error="Counter analysis already running";return false;}
    cancel_.store(false);
    try {
        task_=std::async(std::launch::async,[this,exe,trace,cache,model=std::move(catalog)]() mutable {
            HANDLE process=nullptr;
            try {
                auto dir=cache/"counter_values";std::filesystem::create_directories(dir);
                auto cmds=cache/"counter-export.txt";std::ofstream out(cmds);
                size_t queries=0;
                for(const auto& c:model.series)if(c.reason.empty()) {
                    auto file=dir/(std::to_string(c.id)+".csv");
                    if(std::filesystem::exists(file))std::filesystem::remove(file);
                    out<<"TimingInsights.ExportCounterValues \""<<file.generic_u8string()<<"\" -counter=\""<<c.name<<"\"\n";++queries;
                }
                out.close();if(!out)throw std::runtime_error("Cannot write counter export commands");
                if(queries) {
                    std::string launch_error;
                    if(!NativeTrace::Launch(exe,L"-NoUI -AutoQuit -OpenTraceFile="+NativeTrace::Quote(trace.wstring())+
                        L" -ABSLOG="+NativeTrace::Quote((cache/"counter-analysis.log").wstring())+
                        L" -ExecOnAnalysisCompleteCmd="+NativeTrace::Quote(L"@="+cmds.wstring()),&process,launch_error))
                        throw std::runtime_error(launch_error);
                    while(WaitForSingleObject(process,50)==WAIT_TIMEOUT) {
                        if(cancel_.load()) {
                            TerminateProcess(process,1);throw std::runtime_error("Counter analysis cancelled or timed out; native trace preserved");
                        }
                    }
                    DWORD code=1;GetExitCodeProcess(process,&code);CloseHandle(process);process=nullptr;
                    if(code)throw std::runtime_error("Counter analyzer failed; see counter-analysis.log");
                }
                if(cancel_.load())throw std::runtime_error("Counter import cancelled");
                model.LoadValues(dir,&cancel_);
            }catch(const std::exception& e){if(process)CloseHandle(process);model.error=e.what();}
            return model;
        });return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool CounterTrace::Poll(CounterModel& result,std::string& message) {
    if(!task_.valid()||task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    try {
        result=task_.get();size_t available=0,samples=0;
        for(const auto& c:result.series){available+=c.reason.empty()&&!c.samples.empty();samples+=c.samples.size();}
        message=result.error.empty()?"Counters: "+std::to_string(available)+" with values / "+std::to_string(result.series.size())+
            " defined; "+std::to_string(samples)+" samples":result.error;
    }
    catch(const std::exception& e){result.error=e.what();message=result.error;}
    return true;
}
}
