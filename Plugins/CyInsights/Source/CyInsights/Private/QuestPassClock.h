// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
namespace cyinsights_quest {
inline bool TimestampSpan(uint64_t begin,uint64_t end,uint32_t bits,uint64_t& span) {
    if(!bits||bits>64)return false;
    const auto mask=bits==64?UINT64_MAX:(uint64_t(1)<<bits)-1;
    span=(end-begin)&mask;
    // More than half a hardware counter period is ambiguous, not a valid cost.
    return span<=(mask>>1);
}
struct ClockAnchor { uint64_t gpu_us,bias; double origin; };
// Engine alignment is reused, not promoted to hardware calibration. Keep the
// engine's unsigned wrap-around arithmetic for its signed calibration bias.
inline bool AlignPass(uint64_t begin,uint64_t end,const std::vector<ClockAnchor>& anchors,double& a,double& b) {
    if(end<begin||anchors.empty())return false;
    auto next=std::lower_bound(anchors.begin(),anchors.end(),begin,[](const auto& x,uint64_t t){return x.gpu_us<t;});
    auto best=next;
    if(next==anchors.end())best=std::prev(next);
    else if(next!=anchors.begin()&&begin-std::prev(next)->gpu_us<=next->gpu_us-begin)best=std::prev(next);
    // Do not extend a stale clock relation indefinitely across discontinuities.
    const auto distance=begin>best->gpu_us?begin-best->gpu_us:best->gpu_us-begin;
    if(distance>1000000)return false;
    a=double(begin+best->bias)*1e-6+best->origin;
    b=a+double(end-begin)*1e-6;
    return std::isfinite(a)&&std::isfinite(b)&&a>=0&&b>=a;
}
}
