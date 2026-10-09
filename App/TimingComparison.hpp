// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingModel.hpp"
#include <optional>
#include <string>
#include <vector>
namespace cyinsights {
struct ComparisonKey {
    std::string name;
    bool gpu=false;
    bool operator<(const ComparisonKey& other) const;
};
struct ComparisonGroup {
    ComparisonKey key;
    std::vector<uint32_t> timers;
    std::vector<uint64_t> native_ids;
    bool identity_missing=false;
};
struct ComparisonValue {
    ComparisonGroup group;
    size_t count=0, unfinished=0;
    double inclusive=0, exclusive=0, interval=0;
};
struct ComparisonRow {
    ComparisonKey key;
    ComparisonValue a,b;
    bool defined_a=false,defined_b=false;
    const char* Relation() const;
};
enum class ComparisonMode { Total, PerSecond, PerOccurrence };
class TimingComparisonIndex {
public:
    void Build(const std::vector<TimingEvent>& events,const TimingModel& model,
               const std::atomic<bool>* cancel=nullptr);
    std::vector<ComparisonValue> Summarize(const std::vector<TimingEvent>& events,const TimingModel& model) const;
private:
    std::vector<ComparisonGroup> groups_;
    std::vector<size_t> event_groups_;
};
std::vector<ComparisonRow> CompareTiming(const std::vector<ComparisonValue>& a,const std::vector<ComparisonValue>& b);
std::optional<double> ComparisonCost(const ComparisonValue& value,ComparisonMode mode,bool exclusive=false);
std::optional<double> ComparisonDelta(const ComparisonRow& row,ComparisonMode mode,bool percent=false,bool exclusive=false);
}
