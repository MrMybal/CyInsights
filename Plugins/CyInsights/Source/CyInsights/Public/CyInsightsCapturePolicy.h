// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <cctype>

// Shared by the standalone client and the plugin. No engine types or Epic code.
namespace cycapture {
struct Options {
    bool cpu=true,gpu=true,frame_stats=true,object_counts=true,memory_stats=false;
    bool stat_groups_enabled=false;
    std::string stat_groups="Game,UObjects,GPU";
    bool tasks=false,network=false,file_io=false,loadtime=false,logs=false,memtags=false;
    bool quest_metrics=true;
    int quest_hz=0; // 0: observe once per game frame; positive: optional throttle.
    bool gpu_passes=false; // Extra per-pass names; native GPU groups are baseline.
    bool quest_gpu_completion=true; // Quest Vulkan: sample completed work during recording.
    bool gpu_details=false,gpu_bindings=false,gpu_barriers=false,gpu_per_command=false,gpu_readbacks=false;
    int gpu_frames=1;
    int GpuEventLevel() const {return gpu?(gpu_passes?3:2):0;}
    std::string Validate() const {
        if(gpu_frames<1||gpu_frames>8)return "GPU frame count must be 1..8";
        if(quest_hz<0||quest_hz>120)return "Quest metric rate must be 0 (per frame) or 1..120 Hz";
        if(gpu_passes&&!gpu)return "Detailed GPU passes require native GPU scopes";
        if(stat_groups.size()>512)return "Stat group selection exceeds 512 characters";
        if(stat_groups_enabled&&!frame_stats)return "Additional stat groups require frame statistics";
        if(stat_groups_enabled&&stat_groups.empty())return "Choose stat groups or *";
        if(stat_groups!="*")for(unsigned char c:stat_groups)
            if(!std::isalnum(c)&&c!='_'&&c!=','&&c!=' '&&c!='-')return "Invalid stat group selection";
        return {};
    }
};
}
