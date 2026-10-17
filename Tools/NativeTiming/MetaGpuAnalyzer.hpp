// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "MetaGpuPerfetto.hpp"
#include "FrameStatsAnalyzer.hpp"
#include "MetaGpuRecords.hpp"
#include <regex>
#include <iomanip>
class MetaGpuModule final:public TraceServices::IModule {
    class Analyzer final:public UE::Trace::IAnalyzer {
        MetaGpuModule& owner;
    public:
        explicit Analyzer(MetaGpuModule& x):owner(x){}
        void OnAnalysisBegin(const FOnAnalysisContext& c)override{c.InterfaceBuilder.RouteEvent(1,"CyInsights","Sync");}
        bool OnEvent(uint16,EStyle,const FOnEventContext& c)override{
            const auto ticks=c.EventData.GetValue<uint64>("CpuTicks");const auto seconds=c.EventTime.AsSeconds(ticks);
            FString session;c.EventData.GetString("SessionId",session);
            if(ticks&&std::isfinite(seconds))owner.anchors.try_emplace(Utf8(*session),ticks,seconds);return true;
        }
    };
    using Record=cymeta::Record;
    struct Track{FILE* file=nullptr;std::string name;uint64_t count=0;};
    uint64_t anchorTicks=0;double anchorSeconds=0;
    std::map<std::string,std::pair<uint64_t,double>> anchors;
    std::filesystem::path directory;
    std::map<std::string,uint32_t> timers;
    std::map<std::string,Track> tracks;
public:
    std::vector<std::string> names;uint64_t recorded=0,counterSamples=0,unavailable=0;std::string error;
    explicit MetaGpuModule(const std::filesystem::path& dir):directory(dir){IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName,this);}
    ~MetaGpuModule(){IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName,this);for(auto& p:tracks)if(p.second.file)fclose(p.second.file);}
    void GetModuleInfo(TraceServices::FModuleInfo& i)override{i.Name=TEXT("CyInsights_MetaGpu");i.DisplayName=TEXT("Meta GPU hardware stages");}
    void OnAnalysisBegin(TraceServices::IAnalysisSession& s)override{s.AddAnalyzer(new Analyzer(*this));}
    double Seconds(uint64_t mono)const {return anchorSeconds+(mono>=anchorTicks?double(mono-anchorTicks):-double(anchorTicks-mono))*1e-9;}
    void Read(const std::filesystem::path& trace,FrameStatsModule& stats){
        try{ReadSource(trace,stats);}catch(const std::exception& e){
            if(recorded||counterSamples)throw; // A write failure must not publish a partial cache.
            error=std::string("Meta GPU unavailable: ")+e.what();fprintf(stdout,"%s\n",error.c_str());
        }
    }
    void ReadSource(const std::filesystem::path& trace,FrameStatsModule& stats){
        const auto file=trace.parent_path()/L"meta-gpu.pftrace";if(!std::filesystem::exists(file))return;
        std::string infoText;{std::ifstream info(trace.parent_path()/L"meta-gpu.info.json");infoText.assign((std::istreambuf_iterator<char>(info)),{});if(infoText.find("android.monotonic")==std::string::npos){error="Meta GPU alignment unavailable: missing Android clock provenance";fprintf(stdout,"%s\n",error.c_str());return;}}
        std::smatch frequency;double cpuFrequency=0;
        if(std::regex_search(infoText,frequency,std::regex("\\\"unreal_cpu_frequency_hz\\\"\\s*:\\s*([0-9.eE+-]+)")))cpuFrequency=std::stod(frequency[1]);
        std::smatch session;if(std::regex_search(infoText,session,std::regex("\\\"unreal_session_id\\\"\\s*:\\s*\\\"([^\\\"]+)\\\""))){auto match=anchors.find(session[1]);if(match!=anchors.end()){if(cymeta::MonoAnchorFromCycles(match->second.first,cpuFrequency,anchorTicks))anchorSeconds=match->second.second;}}
        if(!anchorTicks){error="Meta GPU timeline unavailable: no Android CyInsights.Sync clock anchor or cycle frequency";fprintf(stdout,"%s\n",error.c_str());return;}
        std::set<int32_t> gamePids;std::smatch ids;if(std::regex_search(infoText,ids,std::regex("\\\"process_ids\\\"\\s*:\\s*\\[([^\\]]*)\\]"))){const auto body=ids[1].str();const auto pattern=std::regex("[0-9]+");for(auto it=std::sregex_iterator(body.begin(),body.end(),pattern);it!=std::sregex_iterator();++it)gamePids.insert(std::stoi(it->str()));}
        std::map<std::string,uint32_t> metrics;cymeta::Decoder decoder;
        decoder.Read(file); // Validate the entire auxiliary source before appending any cache records.
        auto counts=decoder.Read(file,[&](const cymeta::Stage& s){const double a=Seconds(s.mono_ns),b=a+double(s.duration_ns)*1e-9;if(a<0||!std::isfinite(b)||b<a){++unavailable;return;}
            const auto process=s.pid?(gamePids.count(s.pid)?"Game":"Other process "+std::to_string(s.pid)):"Process unknown";
            const auto key=process+std::string(" / ")+s.queue+" / "+cymeta::StageLane(s);auto inserted=tracks.try_emplace(key);auto& t=inserted.first->second;
            if(inserted.second){t.name="GPU / Meta "+key+" (measured hardware stages)";auto path=directory/("meta-stage-"+std::to_string(tracks.size())+".raw");t.file=_wfopen(path.c_str(),L"w+b");if(!t.file)throw std::runtime_error("Meta GPU spool unavailable");setvbuf(t.file,nullptr,_IOFBF,1024*1024);}
            auto timer=timers.try_emplace("Meta / "+s.name,uint32_t(names.size()));if(timer.second)names.push_back(timer.first->first);
            cyvp::WriteValue(t.file,Record{a,b,timer.first->second});++t.count;++recorded;
        },[&](const cymeta::Counter& c){const auto seconds=Seconds(c.mono_ns);if(seconds<0||!std::isfinite(seconds)){++unavailable;return;}
            auto metric=metrics.try_emplace(c.name,0);if(metric.second)metric.first->second=stats.AddMetric("Meta GPU system",c.name,"raw");stats.AddSample(metric.first->second,seconds,c.value);++counterSamples;
        });unavailable+=counts.unavailable;
        fprintf(stdout,"Meta GPU: %llu stages, %llu counter samples, %llu unavailable; Android MONOTONIC / native Sync alignment\n",recorded,counterSamples,unavailable);
        std::ofstream diagnostic(directory/"meta-gpu-analysis.json");diagnostic<<"{\"sync_ticks\":\""<<anchorTicks<<"\",\"native_seconds\":"<<std::setprecision(17)<<anchorSeconds<<",\"hardware_stages\":\""<<recorded<<"\",\"counter_samples\":\""<<counterSamples<<"\",\"unavailable\":\""<<unavailable<<"\"}";
    }
    uint32_t TrackCount()const{return uint32_t(tracks.size());}
    void Export(FILE* out,uint32_t timerBase){uint64_t lane=0;for(auto& pair:tracks){auto& t=pair.second;cymeta::ExportTrack(out,t.file,t.name,++lane,t.count,timerBase);}}
};
