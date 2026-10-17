// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "FrameStatsFormat.hpp"
#include "Features/IModularFeatures.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include <map>
#include <set>

// Register alongside installed providers. No Epic provider implementation is
// copied, rebuilt or redistributed. Events retain source bounds and observation
// time separately, even when stats/GPU results arrive several frames later.
class FrameStatsModule final:public TraceServices::IModule {
    class Analyzer final:public UE::Trace::IAnalyzer {
        FrameStatsModule& owner;
        std::map<uint32,std::string> specs;
    public:
        explicit Analyzer(FrameStatsModule& module):owner(module){}
        void OnAnalysisBegin(const FOnAnalysisContext& c)override {
            c.InterfaceBuilder.RouteEvent(1,"CyInsightsStats","Spec");
            c.InterfaceBuilder.RouteEvent(2,"CyInsightsStats","Sample");
        }
        bool OnEvent(uint16 route,EStyle,const FOnEventContext& c)override {
            try {
                auto id=c.EventData.GetValue<uint32>("Id");
                if(route==1){
                    FString group,name,unit;
                    c.EventData.GetString("Group",group);c.EventData.GetString("Name",name);c.EventData.GetString("Unit",unit);
                    auto signature=Utf8(*group)+"/"+Utf8(*name)+"/"+Utf8(*unit)+std::to_string(c.EventData.GetValue<bool>("Floating"));
                    auto existing=specs.find(id);
                    if(existing!=specs.end()){if(existing->second!=signature)throw std::runtime_error("Conflicting frame-stat identity");return true;}
                    specs[id]=signature;const uint8_t tag=cyfs::Spec;Value(owner.out,tag);Value(owner.out,id);
                    owner.ids.insert(id);
                    const uint8_t floating=c.EventData.GetValue<bool>("Floating");Value(owner.out,floating);
                    String(owner.out,Utf8(*group));String(owner.out,Utf8(*name));String(owner.out,Utf8(*unit));
                }else{
                    cyfs::Sample s{};s.metric=id;s.flags=c.EventData.GetValue<uint32>("Flags");
                    s.observed=c.EventTime.AsSeconds(c.EventData.GetValue<uint64>("ObservedCycles"));
                    if(s.flags&cyfs::FrameBounds){s.begin=c.EventTime.AsSeconds(c.EventData.GetValue<uint64>("BeginCycles"));s.end=c.EventTime.AsSeconds(c.EventData.GetValue<uint64>("EndCycles"));}
                    else s.begin=s.end=s.observed;
                    s.game_frame=c.EventData.GetValue<uint64>("GameFrame");s.stats_frame=c.EventData.GetValue<int64>("StatsFrame");
                    s.integer=c.EventData.GetValue<int64>("Integer");s.real=c.EventData.GetValue<double>("Real");
                    const uint8_t tag=cyfs::Value;Value(owner.out,tag);Value(owner.out,s);++owner.samples;
                }
            }catch(const std::exception& e){owner.error=e.what();return false;}
            return true;
        }
    };
    FILE* out=nullptr;
    uint64_t samples=0;
    std::set<uint32_t> ids;
public:
    std::string error;
    uint32_t AddMetric(const std::string& group,const std::string& name,const std::string& unit) {
        uint32_t id=0x80000000;while(ids.count(id)){if(id==UINT32_MAX)throw std::runtime_error("Frame statistic ids exhausted");++id;}ids.insert(id);
        Value(out,uint8_t(cyfs::Spec));Value(out,id);Value(out,uint8_t(1));String(out,group);String(out,name);String(out,unit);return id;
    }
    void AddSample(uint32_t id,double seconds,double value) {
        cyfs::Sample s{};s.metric=id;s.flags=cyfs::Valid|cyfs::ExecutionUnknown;s.begin=s.end=s.observed=seconds;s.stats_frame=-1;s.real=value;
        Value(out,uint8_t(cyfs::Value));Value(out,s);++samples;
    }
    explicit FrameStatsModule(const std::wstring& file) {
        out=_wfopen(file.c_str(),L"wb");if(!out)throw std::runtime_error("Frame stats cache write failed");
        setvbuf(out,nullptr,_IOFBF,1024*1024);Write(out,cyfs::magic,8);
        IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName,this);
    }
    ~FrameStatsModule(){IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName,this);if(out)fclose(out);}
    void GetModuleInfo(TraceServices::FModuleInfo& info)override{info.Name=TEXT("CyInsights_FrameStats");info.DisplayName=TEXT("CyInsights frame statistics");}
    void OnAnalysisBegin(TraceServices::IAnalysisSession& session)override{session.AddAnalyzer(new Analyzer(*this));}
    void Finish() {
        if(!error.empty())throw std::runtime_error(error);
        const uint8_t tag=cyfs::End;Value(out,tag);Value(out,samples);
        auto file=out;out=nullptr;if(fclose(file))throw std::runtime_error("Frame stats cache close failed");
    }
};

