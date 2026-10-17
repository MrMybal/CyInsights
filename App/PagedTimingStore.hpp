// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingModel.hpp"
#include "TimingComparison.hpp"
#include "FrameDiagnostics.hpp"
#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <iosfwd>
#include <functional>
namespace cyinsights {
struct NativeReadProgress;
struct StoredTimer {
    std::string name;
    uint64_t id=0;
    bool has_id=false;
    int frame_type=-1;
    uint64_t frame_index=0;
};
struct StoredTrack {
    std::string name;
    uint64_t id=0;
    bool has_id=false;
    int max_depth=0;
    bool gpu=false;
};
struct StoredTotal { uint64_t count=0,unfinished=0;double inclusive=0,exclusive=0; };
struct StoredScope {double start=0,end=0;uint32_t timer=0;int depth=0;bool unfinished=false;uint32_t track=0;};
struct StoredRelation {uint32_t parent=0,child=0;bool gpu=false;uint64_t count=0,unfinished=0;double inclusive=0;};
struct ScopeVisitMetrics {uint64_t block_candidates=0,blocks_read=0,cache_hits=0,records_scanned=0,samples=0;};
struct TimingWindow {
    std::vector<TimingEvent> events;
    std::vector<StoredTotal> totals;
    std::vector<StoredTotal> cpu_totals,gpu_totals;
    std::vector<GpuTimingDetail> gpu_details;
    bool aggregated=false;
    double start=0,end=0,stats_start=0,stats_end=0;
    uint64_t block_candidates=0,blocks_read=0,cache_hits=0,records_scanned=0;
    unsigned workers=1;
};
// Source-owned disposable cache. Every native event is kept on disk; RAM is
// bounded by metadata, ingestion buffers and the display's pixel density.
class PagedTimingStore {
public:
    static std::shared_ptr<PagedTimingStore> Build(const std::filesystem::path& csv,
        const std::filesystem::path& binary,NativeReadProgress& progress,std::string& error,bool binary_source=false);
    TimingWindow Window(double a,double b,double stats_a,double stats_b,float pixels,
                         const std::atomic<bool>* cancel=nullptr,unsigned workers=0,const std::string& exact_name={}) const;
    void ApplyTotals(const TimingWindow& window,TimingModel& model) const;
    void ClearReadCache() const; // Diagnostics: reset reusable blocks, retain source/index.
    std::vector<uint8_t> FunctionMask(const std::string& exact_name) const;
    std::vector<StoredRelation> Relations(double a,double b,const std::atomic<bool>* cancel=nullptr) const;
    void VisitScopes(size_t track,double a,double b,bool roots,const std::function<void(const StoredScope&)>& sink,
        const std::atomic<bool>* cancel=nullptr,const std::vector<uint8_t>* timer_matches=nullptr,ScopeVisitMetrics* metrics=nullptr) const;
    std::vector<ComparisonValue> Summary(const TimingWindow& window,const TimingModel& model) const;
    std::optional<TimingEvent> Nearest(uint64_t timer_id,bool has_id,const std::string& name,double anchor) const;
    std::vector<StoredTrack> tracks;
    std::vector<StoredTimer> timers;
    std::vector<std::pair<double,double>> frames;
    std::vector<uint64_t> frame_ids;
    std::vector<FrameDiagnostic> frame_diagnostics;
    double frame_preparation_ms=0;
    bool frames_native=false,relations_valid=true;
    uint64_t count=0;
    double start=0,end=1;
private:
    struct Record {double start=0,end=0;uint32_t timer=0;int32_t parent=-1;uint32_t unfinished=0;};
    struct Block {uint64_t offset=0;uint32_t count=0,track=0;int depth=0;double start=0,end=0;bool unfinished=false;};
    struct Lane {std::vector<uint32_t> blocks;std::vector<double> prefix_end;};
    struct Cache;
    struct SummaryGroup {ComparisonGroup identity;std::vector<size_t> timers;};
    std::shared_ptr<const std::vector<Record>> Records(uint32_t block,std::ifstream& input,TimingWindow& metrics) const;
    std::filesystem::path file_;
    std::vector<Block> blocks_;
    std::vector<std::vector<uint32_t>> timer_blocks_;
    std::vector<Lane> lanes_;
    std::shared_ptr<Cache> cache_;
    std::vector<SummaryGroup> summary_groups_;
    std::vector<uint8_t> domains_;
};
}
