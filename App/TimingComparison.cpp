// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TimingComparison.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <limits>
#include <set>
#include <stdexcept>
namespace cyinsights {
bool ComparisonKey::operator<(const ComparisonKey& other) const {
    return gpu!=other.gpu?gpu<other.gpu:name<other.name;
}
const char* ComparisonRow::Relation() const {
    if(!defined_a)return "Only B";
    if(!defined_b)return "Only A";
    if(a.group.identity_missing||b.group.identity_missing||a.group.native_ids.size()!=1||b.group.native_ids.size()!=1)
        return "Ambiguous name group";
    return "Name candidate";
}
void TimingComparisonIndex::Build(const std::vector<TimingEvent>& events,const TimingModel& model,
                                  const std::atomic<bool>* cancel) {
    groups_.clear();event_groups_.assign(events.size(),std::numeric_limits<size_t>::max());
    std::map<ComparisonKey,size_t> keys;
    std::vector<std::set<uint32_t>> timers;
    std::vector<std::set<uint64_t>> ids;
    for(const auto& track:model.tracks)for(auto i:track.events) {
        if(i%4096==0&&cancel&&cancel->load())throw std::runtime_error("Analysis cancelled");
        if(events.at(i).frame_type>=0)continue;
        ComparisonKey key{events.at(i).name,track.gpu};
        auto found=keys.emplace(key,groups_.size());
        if(found.second){groups_.push_back({key,{},{},false});timers.emplace_back();ids.emplace_back();}
        size_t group=found.first->second;event_groups_[i]=group;
        timers[group].insert(model.timers.at(i));
        if(events[i].has_timer_id)ids[group].insert(events[i].timer_id);else groups_[group].identity_missing=true;
    }
    for(size_t i=0;i<groups_.size();++i) {
        groups_[i].timers.assign(timers[i].begin(),timers[i].end());
        groups_[i].native_ids.assign(ids[i].begin(),ids[i].end());
    }
}
std::vector<ComparisonValue> TimingComparisonIndex::Summarize(const std::vector<TimingEvent>& events,const TimingModel& model) const {
    if(events.size()!=event_groups_.size()||model.clipped.size()!=events.size()||
       !std::isfinite(model.aggregate_start)||!std::isfinite(model.aggregate_end))throw std::runtime_error("Comparison index or interval does not match timing data");
    std::vector<ComparisonValue> values;
    for(const auto& group:groups_)values.push_back({group,0,0,0,0,std::max(0.,model.aggregate_end-model.aggregate_start)});
    if(model.aggregate_end<=model.aggregate_start)return values;
    std::vector<size_t> active;
    for(const auto& track:model.tracks) {
        active.clear();track.Query(events,model.aggregate_start,model.aggregate_end,4097,active);
        for(auto i:active) {
            if(events[i].frame_type>=0)continue;
            auto& value=values.at(event_groups_[i]);const auto& event=events[i];
            if(event.open_ended) {
                if(event.start<model.aggregate_end&&event.end>model.aggregate_start)++value.unfinished;
            } else if(model.clipped[i]>0) {
                ++value.count;value.inclusive+=model.clipped[i];value.exclusive+=model.exclusive.at(i);
            }
        }
    }
    return values;
}
std::vector<ComparisonRow> CompareTiming(const std::vector<ComparisonValue>& a,const std::vector<ComparisonValue>& b) {
    std::map<ComparisonKey,ComparisonRow> rows;
    for(const auto& value:a){auto& row=rows[value.group.key];row.key=value.group.key;row.a=value;row.defined_a=true;}
    for(const auto& value:b){auto& row=rows[value.group.key];row.key=value.group.key;row.b=value;row.defined_b=true;}
    std::vector<ComparisonRow> output;
    for(auto& entry:rows)if(entry.second.a.count||entry.second.b.count||entry.second.a.unfinished||entry.second.b.unfinished)
        output.push_back(std::move(entry.second));
    return output;
}
std::optional<double> ComparisonCost(const ComparisonValue& value,ComparisonMode mode,bool exclusive) {
    if(!value.count)return {};
    double cost=exclusive?value.exclusive:value.inclusive;
    if(mode==ComparisonMode::PerSecond){if(value.interval<=0)return {};cost/=value.interval;}
    if(mode==ComparisonMode::PerOccurrence)cost/=static_cast<double>(value.count);
    return std::isfinite(cost)?std::optional<double>{cost}:std::nullopt;
}
std::optional<double> ComparisonDelta(const ComparisonRow& row,ComparisonMode mode,bool percent,bool exclusive) {
    auto a=ComparisonCost(row.a,mode,exclusive),b=ComparisonCost(row.b,mode,exclusive);
    if(!a||!b||!row.defined_a||!row.defined_b)return {};
    if(percent&&*a<=0)return {};
    double delta=percent?((*b-*a)/ *a)*100:*b-*a;
    return std::isfinite(delta)?std::optional<double>{delta}:std::nullopt;
}
}
