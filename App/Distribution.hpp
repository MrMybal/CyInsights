// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <array>
#include <atomic>
#include <functional>
#include <vector>
#include <cstdint>
namespace cyinsights {
struct DistributionSample {double value=0,start=0,end=0;uint64_t index=0;uint32_t timer=0;bool valid=true;};
struct Distribution {
    static constexpr size_t bin_count=64,page_size=128;
    std::array<uint64_t,bin_count> bins{};
    uint64_t count=0,unavailable=0,matched=0;
    double min=0,max=0,mean=0;
    std::array<double,4> percentiles{}; // nearest rank P50/P90/P95/P99
    std::array<std::pair<double,double>,4> percentile_bounds{};
    bool exact_percentiles=false,logarithmic_values=false;
    int selected_bin=-1;uint64_t page=0;
    std::vector<DistributionSample> examples;
    size_t Bin(double value) const;
    std::pair<double,double> Bounds(size_t bin) const;
    double bin_origin=0,bin_factor=0; // Prepared once, avoids logs of bounds per sample.
};
using SampleConsumer=std::function<void(const DistributionSample&)>;
using SampleVisitor=std::function<void(const SampleConsumer&)>;
// All samples contribute. Scope quantiles have explicit histogram bounds;
// frame samples can supply exact quantiles without retaining native scopes.
Distribution BuildDistribution(const SampleVisitor& visit,bool exact_percentiles,
    int selected_bin=-1,uint64_t page=0,const std::atomic<bool>* cancel=nullptr,bool logarithmic_values=false);
Distribution SelectDistributionSamples(const SampleVisitor& visit,const Distribution& summary,
    int selected_bin,uint64_t page,const std::atomic<bool>* cancel=nullptr);
}
