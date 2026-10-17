// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "QuestPassClock.h"
#include "VulkanPassRecords.hpp"
#include "VulkanPassDetailFormat.hpp"
#include "Features/IModularFeatures.h"
#include "Trace/Analyzer.h"
#include "TraceServices/Model/AnalysisSession.h"
#include <map>
// Supplemental events stay on disk while native providers analyze large traces.
// Clock anchors are the small, per-frame native GPU clock relation. Never put
// asynchronous query receipt timestamps on the execution timeline.
class VulkanPassModule final:public TraceServices::IModule {
    using Raw=cyvp::Raw;
    class Analyzer final:public UE::Trace::IAnalyzer {
        VulkanPassModule& owner;
    public:
        explicit Analyzer(VulkanPassModule& x):owner(x){}
        void OnAnalysisBegin(const FOnAnalysisContext& c)override {
            c.InterfaceBuilder.RouteEvent(1,"CyInsightsVulkan","Pass");
            c.InterfaceBuilder.RouteEvent(2,"GpuProfiler","Frame");
            c.InterfaceBuilder.RouteEvent(3,"CyInsightsVulkan","Clock");
            c.InterfaceBuilder.RouteEvent(4,"CyInsightsVulkan","Contents");
        }
        bool OnEvent(uint16 route,EStyle,const FOnEventContext& c)override {
            try {
                if(route==2){owner.anchors.push_back({c.EventData.GetValue<uint64>("TimestampBase"),c.EventData.GetValue<uint64>("CalibrationBias"),c.EventTime.AsSeconds(0)});return true;}
                if(route==3){const auto gpu=c.EventData.GetValue<uint64>("GpuUs");owner.deviceAnchors.push_back({gpu,0,c.EventTime.AsSeconds(c.EventData.GetValue<uint64>("CpuCycles"))-double(gpu)*1e-6});return true;}
                if(route==4){const auto& e=c.EventData;cyvpd::Raw raw{};raw.begin=e.GetValue<uint64>("BeginUs");raw.end=e.GetValue<uint64>("EndUs");auto& r=raw.detail;
                    r.pass=e.GetValue<uint64>("PassId");r.submission=e.GetValue<uint64>("Submission");r.ordinal=e.GetValue<uint32>("Ordinal");r.width=e.GetValue<uint32>("Width");r.height=e.GetValue<uint32>("Height");r.subpass=e.GetValue<uint32>("SubpassIndex");r.subpasses=e.GetValue<uint32>("Subpasses");r.flags=e.GetValue<uint32>("Flags");
                    r.direct=e.GetValue<uint64>("DirectDraws");r.indexed=e.GetValue<uint64>("IndexedDraws");r.indirect=e.GetValue<uint64>("IndirectCalls");r.indirect_commands=e.GetValue<uint64>("IndirectCommands");r.vertices=e.GetValue<uint64>("VerticesRequested");r.indices=e.GetValue<uint64>("IndicesRequested");r.instances=e.GetValue<uint64>("InstancesRequested");
                    cyvp::WriteValue(owner.details,raw);return true;
                }
                const auto& e=c.EventData;
                const auto name="Vulkan pass "+std::to_string(e.GetValue<uint32>("Ordinal"))+" ["+std::to_string(e.GetValue<uint32>("Width"))+"x"+std::to_string(e.GetValue<uint32>("Height"))+", "+std::to_string(e.GetValue<uint32>("Subpasses"))+" subpasses]";
                auto [it,added]=owner.ids.emplace(name,static_cast<uint32_t>(owner.names.size()));
                if(added)owner.names.push_back(name);
                Raw record{e.GetValue<uint64>("BeginUs"),e.GetValue<uint64>("EndUs"),it->second,0};cyvp::WriteValue(owner.spool,record);++owner.recorded;
            }catch(const std::exception& e){owner.error=e.what();return false;}
            return true;
        }
    };
    FILE* spool=nullptr;FILE* details=nullptr;std::filesystem::path detailPath;
    std::map<std::string,uint32_t> ids;
    std::vector<cyinsights_quest::ClockAnchor> anchors;
    std::vector<cyinsights_quest::ClockAnchor> deviceAnchors;
public:
    std::vector<std::string> names;
    uint64_t recorded=0,aligned=0,unavailable=0;
    std::string error;
    explicit VulkanPassModule(const std::wstring& path) {
        spool=_wfopen(path.c_str(),L"w+b");if(!spool)throw std::runtime_error("Vulkan pass cache write failed");setvbuf(spool,nullptr,_IOFBF,1024*1024);
        detailPath=std::filesystem::path(path).parent_path()/L"vulkan-pass-details.cyvp";
        details=_wfopen((detailPath.wstring()+L".raw").c_str(),L"w+b");if(!details){fclose(spool);spool=nullptr;throw std::runtime_error("Vulkan content spool unavailable");}setvbuf(details,nullptr,_IOFBF,1024*1024);
        IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName,this);
    }
    ~VulkanPassModule(){IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName,this);if(spool)fclose(spool);if(details)fclose(details);}
    void GetModuleInfo(TraceServices::FModuleInfo& info)override{info.Name=TEXT("CyInsights_VulkanPasses");info.DisplayName=TEXT("CyInsights Vulkan passes");}
    void OnAnalysisBegin(TraceServices::IAnalysisSession& session)override{session.AddAnalyzer(new Analyzer(*this));}
    void Export(FILE* out,uint32_t timerBase) {
        if(!error.empty())throw std::runtime_error(error);
        if(!deviceAnchors.empty())anchors=std::move(deviceAnchors);
        std::stable_sort(anchors.begin(),anchors.end(),[](const auto& a,const auto& b){return a.gpu_us<b.gpu_us;});
        FILE* detailOut=_wfopen(detailPath.c_str(),L"wb");if(!detailOut)throw std::runtime_error("Vulkan content export unavailable");
        try{cyvp::WriteValue(detailOut,cyvpd::magic);if(fflush(details)||_fseeki64(details,0,SEEK_SET))throw std::runtime_error("Vulkan content seek failed");cyvpd::Raw raw{};uint64_t total=0;
            while(fread(&raw,sizeof(raw),1,details)==1){double a=0,b=0;if(cyinsights_quest::AlignPass(raw.begin,raw.end,anchors,a,b)){raw.detail.begin=a;raw.detail.end=b;cyvp::WriteValue(detailOut,raw.detail);++total;}}
            if(ferror(details))throw std::runtime_error("Vulkan content read failed");cyvp::WriteValue(detailOut,cyvpd::footer);cyvp::WriteValue(detailOut,total);
            const auto result=fclose(detailOut);detailOut=nullptr;if(result)throw std::runtime_error("Vulkan content close failed");
        }catch(...){if(detailOut)fclose(detailOut);throw;}
        auto counts=cyvp::ExportTrack(out,spool,std::move(anchors),timerBase);aligned=counts.aligned;unavailable=counts.unavailable;
        if(aligned+unavailable!=recorded)throw std::runtime_error("Incomplete Vulkan pass spool");
        fprintf(stdout,"Vulkan passes: %llu recorded, %llu aligned, %llu unavailable (no usable engine GPU clock relation)\n",recorded,aligned,unavailable);
    }
};
