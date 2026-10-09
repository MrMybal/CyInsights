// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "Capture.hpp"
#include "TimingComparison.hpp"
#include "PagedTimingStore.hpp"
#include <atomic>
#include <future>
#include <memory>
namespace cyinsights {
struct ComparisonDataset {
    std::vector<TimingEvent> events;
    TimingModel timing;
    TimingComparisonIndex index;
    std::vector<ComparisonValue> summary;
    std::shared_ptr<PagedTimingStore> store;
    fs::path file,analyzer;
    cygi::Json metadata;
    double start=0,end=0;
};
class ComparisonTrace {
public:
    ~ComparisonTrace();
    bool Start(const fs::path& file,const fs::path& analyzer,const fs::path& cache,std::string& error);
    // The UI must not read this dataset while Range is busy.
    bool Range(std::shared_ptr<ComparisonDataset> data,double start,double end,std::string& error);
    bool Poll(std::shared_ptr<ComparisonDataset>& data,std::string& message);
    bool Busy() const {return task_.valid();}
    void Cancel(){cancel_.store(true);}
private:
    struct Result {std::shared_ptr<ComparisonDataset> data;std::string error;};
    std::future<Result> task_;
    std::atomic<bool> cancel_{false};
};
}
