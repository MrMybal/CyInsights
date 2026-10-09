// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <atomic>

namespace cyinsights {
struct CounterSample {
    double time = 0, real = 0;
    int64_t integer = 0;
    bool valid = true;
    double Value(bool floating) const { return floating ? real : static_cast<double>(integer); }
};
struct CounterSeries {
    uint32_t id = 0;
    std::string name, unit, reason;
    bool floating = false, reset_per_frame = false, meta = false, availability = false, pinned = false;
    int companion = -1;
    std::vector<CounterSample> samples;
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
    // Fixed numeric filenames avoid path/name collisions from trace metadata.
    static bool CanExport(const std::string& name);
    void LoadValues(const std::filesystem::path& directory,const std::atomic<bool>* cancel=nullptr);
    void Index();
    bool Available(size_t counter, double time) const;
    const CounterSample* At(size_t counter, double time) const;
    CounterStats Stats(size_t counter, double a, double b) const;
    std::vector<CounterBucket> Plot(size_t counter, double a, double b, int width) const;
};
}
