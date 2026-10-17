// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingEvent.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
namespace cyinsights {
struct GpuTimingDetail {
    size_t nested=0,near_zero=0;
    double longest_outer=0;
    // A diagnostic heuristic, not a correction to recorded durations.
    bool Limited() const {return nested>=4&&near_zero>=nested-nested/5&&longest_outer>=.001;}
    void AddInterval(double start,double end,int depth,bool unfinished,int frame_type) {
        if(unfinished||frame_type>=0)return;
        const auto duration=end-start;
        if(!std::isfinite(duration)||duration<0)return;
        if(depth==0)longest_outer=std::max(longest_outer,duration);
        else {++nested;if(duration<=2e-6+1e-10)++near_zero;}
    }
    void Merge(const GpuTimingDetail& other){nested+=other.nested;near_zero+=other.near_zero;longest_outer=std::max(longest_outer,other.longest_outer);}
    void Add(const TimingEvent& event) {
        if(!event.display_aggregate)AddInterval(event.start,event.end,event.depth,event.open_ended,event.frame_type);
    }
};
inline GpuTimingDetail InspectGpuTimingDetail(const std::vector<TimingEvent>& source,const std::vector<size_t>& indices) {
    GpuTimingDetail result;for(auto index:indices)result.Add(source[index]);return result;
}
}
