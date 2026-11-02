// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "CounterModel.hpp"
#include "ActivityStore.hpp"
#include <cyui/modules/Plots.h>
#include <array>
#include <limits>

namespace cyinsights {
// A presentation adapter, not an aggregation engine. CounterModel retains exact
// int64 values and availability. At most five points per indexed pixel bucket;
// all four endpoints/extrema survive and missing source frames break the line.
struct CounterPlotData {
    double low=0,high=0,normal=1;
    size_t minimum=0,maximum=0;
    bool available=false;
    std::vector<cyui::Point> line,dots;
};
inline CounterPlotData PrepareCounterPlot(const CounterSeries& source,const std::vector<CounterBucket>& buckets) {
    CounterPlotData data;
    for(const auto& bucket:buckets){
        const auto low=source.samples[bucket.minimum].Value(source.floating),high=source.samples[bucket.maximum].Value(source.floating);
        if(!data.available||(source.floating?low<source.samples[data.minimum].real:source.samples[bucket.minimum].integer<source.samples[data.minimum].integer))data.minimum=bucket.minimum;
        if(!data.available||(source.floating?high>source.samples[data.maximum].real:source.samples[bucket.maximum].integer>source.samples[data.maximum].integer))data.maximum=bucket.maximum;
        data.low=data.available?std::min(data.low,low):low;data.high=data.available?std::max(data.high,high):high;data.available=true;
    }
    data.normal=std::max({std::abs(data.low),std::abs(data.high),1.});
    data.line.reserve(buckets.size()*5);data.dots.reserve(buckets.size());
    const CounterBucket* previous=nullptr;
    for(const auto& bucket:buckets){
        bool continuous=previous&&previous->last+1==bucket.first;
        if(continuous&&source.frame_stats){
            const auto& a=source.frame_samples[previous->last];const auto& b=source.frame_samples[bucket.first];
            continuous=(a.flags&cyfs::FrameBounds)&&(b.flags&cyfs::FrameBounds)&&a.game_frame!=UINT64_MAX&&b.game_frame==a.game_frame+1;
        }
        if(previous&&!continuous)data.line.push_back({source.samples[bucket.first].time,std::numeric_limits<double>::quiet_NaN()});
        std::array<size_t,4> indices{bucket.first,bucket.minimum,bucket.maximum,bucket.last};std::sort(indices.begin(),indices.end());
        size_t last=SIZE_MAX;
        for(auto index:indices)if(index!=last){const auto& sample=source.samples[index];data.line.push_back({sample.time,sample.Value(source.floating)/data.normal});last=index;}
        const auto& sample=source.samples[bucket.last];data.dots.push_back({sample.time,sample.Value(source.floating)/data.normal});previous=&bucket;
    }
    return data;
}
inline std::vector<cyui::Point> PrepareActivityPlot(const ActivityLaneWindow& lane,const ActivityWindow& window,bool memory) {
    std::vector<cyui::Point> points;if(lane.bins.empty()||!(window.end>window.start))return points;
    double high=1,low=0;
    for(const auto& bin:lane.bins)if(bin.count){high=std::max(high,memory?bin.maximum:double(bin.count));if(memory)low=std::min(low,bin.minimum);}
    const double step=(window.end-window.start)/lane.bins.size();points.reserve(lane.bins.size());
    for(size_t i=0;i<lane.bins.size();++i){const auto& bin=lane.bins[i];
        const double value=bin.count?(memory?(bin.maximum-low)/(high-low):double(bin.count)/high):0;
        points.push_back({window.start+(i+.5)*step,value});
    }
    return points;
}
}
