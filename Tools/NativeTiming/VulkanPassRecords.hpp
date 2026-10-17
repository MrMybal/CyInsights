// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "Format.hpp"
#include "QuestPassClock.h"
#include <cstdio>
#include <stdexcept>
namespace cyvp {
inline constexpr char track_name[]="GPU / Vulkan render passes (approximate alignment)";
struct Raw{uint64_t begin,end;uint32_t timer,reserved;};
struct Counts{uint64_t aligned=0,unavailable=0;};
template<class T>void WriteValue(FILE* out,const T& v){if(fwrite(&v,sizeof(v),1,out)!=1)throw std::runtime_error("Vulkan pass cache write failed");}
inline Counts ExportTrack(FILE* out,FILE* spool,std::vector<cyinsights_quest::ClockAnchor> anchors,uint32_t timerBase) {
    if(fflush(spool)||_fseeki64(spool,0,SEEK_SET))throw std::runtime_error("Vulkan pass cache seek failed");
    std::stable_sort(anchors.begin(),anchors.end(),[](const auto& a,const auto& b){return a.gpu_us<b.gpu_us;});
    WriteValue(out,uint64_t((1ull<<63)|0x43595650));WriteValue(out,uint32_t(sizeof(track_name)-1));
    if(fwrite(track_name,1,sizeof(track_name)-1,out)!=sizeof(track_name)-1)throw std::runtime_error("Vulkan track name write failed");
    const auto countAt=_ftelli64(out);WriteValue(out,uint64_t(0));Counts counts;Raw r{};
    while(fread(&r,sizeof(r),1,spool)==1) {
        double a=0,b=0;if(!cyinsights_quest::AlignPass(r.begin,r.end,anchors,a,b)){++counts.unavailable;continue;}
        WriteValue(out,cynt::Event{a,b,timerBase+r.timer,0,0,0});++counts.aligned;
    }
    if(ferror(spool))throw std::runtime_error("Vulkan pass cache read failed");
    const auto end=_ftelli64(out);if(countAt<0||end<0||_fseeki64(out,countAt,SEEK_SET))throw std::runtime_error("Vulkan pass output seek failed");
    WriteValue(out,counts.aligned);if(_fseeki64(out,end,SEEK_SET))throw std::runtime_error("Vulkan pass output seek failed");
    return counts;
}
}
