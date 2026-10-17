// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>
#include "../Tools/NativeTiming/FrameStatsFormat.hpp"

namespace cyinsights {
struct CounterSample {
    double time = 0, real = 0;
    int64_t integer = 0;
    bool valid = true;
    double Value(bool floating) const { return floating ? real : static_cast<double>(integer); }
};
struct CounterSeries {
    uint32_t id = 0;
    std::string name, unit, reason, group;
    bool floating = false, reset_per_frame = false, meta = false, availability = false, pinned = false;
    int companion = -1;
    std::vector<CounterSample> samples;
    bool frame_stats=false;
    uint32_t frame_stat_id=0;
    // Parallel only for plugin snapshots; ordinary counters incur no per-sample
    // frame metadata allocation. Values retain exact int64 representation.
    std::vector<cyfs::Sample> frame_samples;
    size_t Lower(double time) const;
    std::string Text(const CounterSample&) const;
};
struct CounterStats {
    size_t count = 0, unavailable = 0;
    double minimum = 0, maximum = 0, mean = 0;
    size_t first = 0, last = 0;
    size_t minimum_sample = 0, maximum_sample = 0;
};
struct CounterBucket {
    size_t first = 0, last = 0, minimum = 0, maximum = 0;
    int pixel = 0;
};
class CounterModel {
public:
    std::vector<CounterSeries> series;
    std::string error;
    double start = 0, end = 0;
    static bool ReadCatalog(const std::filesystem::path&, CounterModel&, std::string& error);
    bool ReadFrameStats(const std::filesystem::path&,bool catalog_only,std::string& error,const std::atomic<bool>* cancel=nullptr);
    // Last available observation recorded INSIDE this frame. Never carry a
    // previous frame's gauge or a delayed GPU result across a missing frame.
    const CounterSample* InFrame(size_t counter,double begin,double end,size_t* index=nullptr) const;
    // Fixed numeric filenames avoid path/name collisions from trace metadata.
    static bool CanExport(const std::string& name);
    void LoadValues(const std::filesystem::path& directory,const std::atomic<bool>* cancel=nullptr);
    // Rebuild after changing samples. Pinning and display metadata do not
    // invalidate this index; LoadValues builds it off the UI thread.
    void Index(const std::atomic<bool>* cancel=nullptr);
    bool Available(size_t counter, double time) const;
    const CounterSample* At(size_t counter, double time) const;
    CounterStats Stats(size_t counter, double a, double b) const;
    std::vector<CounterBucket> Plot(size_t counter, double a, double b, int width) const;
private:
    static constexpr size_t range_block_samples=64;
    struct RangeIndex {size_t samples=0,base=0;std::vector<uint8_t> valid;std::vector<CounterStats> tree;};
    std::vector<RangeIndex> ranges_;
    bool Indexed(size_t counter) const;
    bool SampleAvailable(size_t counter,size_t sample) const;
    CounterStats Combine(size_t counter,const CounterStats& a,const CounterStats& b) const;
    CounterStats Range(size_t counter,size_t begin,size_t end) const;
};
}
