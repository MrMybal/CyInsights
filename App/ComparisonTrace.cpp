// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ComparisonTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <thread>
namespace cyinsights {
namespace {
fs::path MatchingAnalyzer(const fs::path& selected,const cygi::Json& metadata) {
    auto version=metadata["engine_version"].AsString();
    if(version.size()<3||version[0]!='5'||version[1]!='.'||version[2]<'5'||version[2]>'8'||
       (version.size()>3&&version[3]!='.'))return selected;
    auto root=selected.parent_path().parent_path().parent_path().parent_path().parent_path();
    auto candidate=root/("UE_"+version.substr(0,3))/"Engine/Binaries/Win64/UnrealInsights.exe";
    return fs::is_regular_file(candidate)?candidate:selected;
}
}
ComparisonTrace::~ComparisonTrace(){Cancel();if(task_.valid())task_.wait();}
bool ComparisonTrace::Start(const fs::path& file,const fs::path& analyzer,const fs::path& cache,std::string& error) {
    if(Busy()){error="Comparison analysis already running";return false;}
    cancel_.store(false);
    try {
        task_=std::async(std::launch::async,[this,file,analyzer,cache] {
            Result result;
            try {
                auto data=std::make_shared<ComparisonDataset>();data->file=file;
                fs::path trace=file;
                if(file.extension()==L".cycapture") {
                    Capture capture;std::string problem;
                    if(!Inspect(file,capture,problem)||!Extract(capture,cache/"capture",problem))throw std::runtime_error(problem);
                    trace.clear();size_t sources=0;
                    for(const auto& source:capture.manifest["sources"].Items())if(source["id"].AsString()=="unreal.trace"&&source.Has("path")) {
                        trace=cache/"capture"/fs::u8path(source["path"].AsString());++sources;
                    }
                    if(sources!=1)throw std::runtime_error("Capture B requires exactly one native trace source");
                    data->metadata=capture.manifest["metadata"];
                } else if(file.extension()!=L".utrace")throw std::runtime_error("Choose a .utrace or .cycapture for B");
                if(cancel_.load())throw std::runtime_error("Comparison cancelled");
                data->analyzer=MatchingAnalyzer(analyzer,data->metadata);
                NativeTrace native;std::string message;
                if(!native.Start(data->analyzer,trace,cache/"analysis",message,true))throw std::runtime_error(message);
                while(native.Busy()) {
                    if(cancel_.load())throw std::runtime_error("Comparison cancelled; original capture preserved");
                    native.Poll(message);
                    if(native.Busy())std::this_thread::sleep_for(std::chrono::milliseconds(25));
                }
                if(!native.store||!native.store->count)throw std::runtime_error("Capture B has no usable native timing: "+message);
                data->store=native.store;
                auto window=data->store->Window(data->store->start,data->store->end,data->store->start,data->store->end,2000,&cancel_);
                data->events=std::move(window.events);data->timing.Build(data->events,false,&cancel_);
                data->timing.Aggregate(data->events,data->store->start,data->store->end);data->store->ApplyTotals(window,data->timing);
                data->timing.frames=data->store->frames;data->timing.frame_ids=data->store->frame_ids;data->timing.frames_native=data->store->frames_native;
                data->start=data->timing.start;data->end=data->timing.end;
                data->summary=data->store->Summary(window,data->timing);
                if(cancel_.load())throw std::runtime_error("Comparison cancelled");
                result.data=std::move(data);
            }catch(const std::exception& e){result.error=e.what();}
            return result;
        });return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool ComparisonTrace::Range(std::shared_ptr<ComparisonDataset> data,double start,double end,std::string& error) {
    if(Busy()){error="Comparison analysis already running";return false;}
    if(!data||!std::isfinite(start)||!std::isfinite(end)||start< data->timing.start||end>data->timing.end||start>=end) {
        error="B interval must be finite, inside its capture and have positive duration";return false;
    }
    cancel_.store(false);
    try {
        task_=std::async(std::launch::async,[this,data,start,end] {
            Result result;const double old_start=data->start,old_end=data->end;
            try {
                auto summary=data->summary;
                if(data->store){auto window=data->store->Window(start,end,start,end,2000,&cancel_);summary=data->store->Summary(window,data->timing);}
                else {data->timing.Aggregate(data->events,start,end);summary=data->index.Summarize(data->events,data->timing);}
                if(cancel_.load())throw std::runtime_error("Comparison interval cancelled");
                data->summary=std::move(summary);data->start=start;data->end=end;result.data=data;
            }catch(const std::exception& e) {
                if(!data->store)data->timing.Aggregate(data->events,old_start,old_end);result.error=e.what();
            }
            return result;
        });return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool ComparisonTrace::Poll(std::shared_ptr<ComparisonDataset>& data,std::string& message) {
    if(!task_.valid()||task_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return false;
    try {
        auto result=task_.get();
        if(result.error.empty()){data=std::move(result.data);message="Comparison B ready: "+std::to_string(data->events.size())+" native timing events";}
        else message=result.error;
    }catch(const std::exception& e){message=e.what();}
    return true;
}
}
