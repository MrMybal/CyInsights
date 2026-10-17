// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "CounterTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <fstream>
#include <stdexcept>
namespace cyinsights {
CounterTrace::~CounterTrace(){cancel_.store(true);if(task_.valid())task_.wait();}
bool CounterTrace::Start(const std::filesystem::path& exe,const std::filesystem::path& trace,
                         const std::filesystem::path& cache,CounterModel catalog,std::string& error,bool frame_only) {
    return Start(exe,trace,cache,std::make_shared<CounterModel>(std::move(catalog)),error,frame_only);
}
bool CounterTrace::Start(const std::filesystem::path& exe,const std::filesystem::path& trace,
                         const std::filesystem::path& cache,std::shared_ptr<const CounterModel> catalog,std::string& error,bool frame_only) {
    if(Busy()){error="Counter analysis already running";return false;}
    cancel_.store(false);
    try {
        task_=std::async(std::launch::async,[this,exe,trace,cache,frame_only,catalog=std::move(catalog)]() mutable {
            CounterModel model;
            HANDLE process=nullptr;
            try {
                // Samples are immutable while published. Copy for replacement
                // only on the loader worker; UI-only pin flags are not read.
                if(!catalog)throw std::runtime_error("Counter catalogue unavailable");
                model.series.reserve(catalog->series.size());
                const bool reloadFrames=std::filesystem::exists(cache/"frame-stats.cystats");
                for(const auto& source:catalog->series){if(cancel_.load())throw std::runtime_error("Counter import cancelled");
                    if(source.frame_stats&&reloadFrames)continue; // replaced from the immutable cache below
                    CounterSeries c;c.id=source.id;c.name=source.name;c.unit=source.unit;c.reason=source.reason;c.group=source.group;c.floating=source.floating;c.reset_per_frame=source.reset_per_frame;c.meta=source.meta;c.availability=source.availability;c.frame_stats=source.frame_stats;c.frame_stat_id=source.frame_stat_id;
                    if(frame_only||source.frame_stats){c.samples=source.samples;c.frame_samples=source.frame_samples;}
                    model.series.push_back(std::move(c));}
                if(frame_only){if(!model.ReadFrameStats(cache/"frame-stats.cystats",false,model.error,&cancel_))throw std::runtime_error(model.error);model.Index(&cancel_);return model;}
                auto dir=cache/"counter_values";std::filesystem::create_directories(dir);
                auto cmds=cache/"counter-export.txt";std::ofstream out(cmds);
                size_t queries=0;
                for(const auto& c:model.series)if(c.reason.empty()&&!c.frame_stats) {
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
                if(!model.ReadFrameStats(cache/"frame-stats.cystats",false,model.error,&cancel_))throw std::runtime_error(model.error);
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
