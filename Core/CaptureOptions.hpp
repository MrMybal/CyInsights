// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "../Plugins/CyInsights/Source/CyInsights/Public/CyInsightsCapturePolicy.h"
#include <CyGPUInspectorCore/Json.hpp>
#include <cmath>
namespace cyinsights {
using CaptureOptions=cycapture::Options;
inline cygi::Json CaptureOptionsJson(const CaptureOptions& o){
    auto j=cygi::Json::Object();j["version"]=1;
#define CY_BOOL(name) j[#name]=o.name
    CY_BOOL(cpu);CY_BOOL(gpu);CY_BOOL(frame_stats);CY_BOOL(object_counts);CY_BOOL(memory_stats);CY_BOOL(stat_groups_enabled);
    CY_BOOL(tasks);CY_BOOL(network);CY_BOOL(file_io);CY_BOOL(loadtime);CY_BOOL(logs);CY_BOOL(memtags);CY_BOOL(quest_metrics);
    CY_BOOL(quest_gpu_completion);CY_BOOL(gpu_passes);CY_BOOL(gpu_details);CY_BOOL(gpu_bindings);CY_BOOL(gpu_barriers);CY_BOOL(gpu_per_command);CY_BOOL(gpu_readbacks);
#undef CY_BOOL
    j["stat_groups"]=o.stat_groups;j["quest_hz"]=o.quest_hz;j["gpu_frames"]=o.gpu_frames;return j;
}
inline bool ReadCaptureOptions(const cygi::Json& j,CaptureOptions& o,std::string& error){
    if(!j.IsObject()||j["version"].AsNumber()!=1){error="Unsupported capture-options version/object";return false;}
    auto result=o;
    for(const auto& pair:j.Members()){
        const auto& key=pair.first;const auto& v=pair.second;if(key=="version")continue;
#define CY_BOOL(name) if(key==#name){if(v.GetType()!=cygi::Json::Type::boolean){error="Expected boolean: "+key;return false;}result.name=v.AsBool();continue;}
        CY_BOOL(cpu);CY_BOOL(gpu);CY_BOOL(frame_stats);CY_BOOL(object_counts);CY_BOOL(memory_stats);CY_BOOL(stat_groups_enabled);
        CY_BOOL(tasks);CY_BOOL(network);CY_BOOL(file_io);CY_BOOL(loadtime);CY_BOOL(logs);CY_BOOL(memtags);CY_BOOL(quest_metrics);
        CY_BOOL(quest_gpu_completion);CY_BOOL(gpu_passes);CY_BOOL(gpu_details);CY_BOOL(gpu_bindings);CY_BOOL(gpu_barriers);CY_BOOL(gpu_per_command);CY_BOOL(gpu_readbacks);
#undef CY_BOOL
        if(key=="stat_groups"){if(v.GetType()!=cygi::Json::Type::string){error="Expected stat group string";return false;}result.stat_groups=v.AsString();continue;}
        if(key=="quest_hz"||key=="gpu_frames"){
            const auto n=v.AsNumber();if(v.GetType()!=cygi::Json::Type::number||!std::isfinite(n)||n<(key=="quest_hz"?0:1)||n>120||n!=std::floor(n)){error="Invalid integer: "+key;return false;}
            if(key=="quest_hz")result.quest_hz=static_cast<int>(n);else result.gpu_frames=static_cast<int>(n);continue;
        }
        error="Unknown capture option: "+key;return false;
    }
    error=result.Validate();if(!error.empty())return false;o=std::move(result);return true;
}
}
