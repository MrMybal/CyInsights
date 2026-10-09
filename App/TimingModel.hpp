// Native timing views rebuilt around the Unreal Insights workflow.
// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingEvent.hpp"
#include "TimingViewport.hpp"
#include <cstdint>
#include <utility>
#include <vector>
#include <atomic>
namespace cyinsights {
struct TimingDepth {
    std::vector<size_t> events;
    std::vector<double> prefix_end;
};
struct TimingTrack {
    std::string name;
    std::vector<size_t> events;
    // A long-lived outer scope must not force a scan of every deeper scope.
    std::vector<TimingDepth> depths;
    uint64_t id = 0;
    bool has_id = false;
    bool gpu = false, visible = true, collapsed = false;
    int focus_depth = -1;
    std::vector<size_t> visible_events;
    double visible_start = 0, visible_end = 0;
    int visible_levels = -1;
    int visible_rows = 0;
    std::vector<TimingGlyph> glyphs;
    float glyph_width = 0;
    int glyph_selected = -2;
    std::string glyph_search;
    int RowCount(int limit) const;
    int VisibleRowCount(const std::vector<TimingEvent>& source,double start,double end,int limit) const;
    void Query(const std::vector<TimingEvent> &source, double start, double end, int levels,
               std::vector<size_t> &result) const;
};
struct TimerTotal {
    uint32_t timer = 0;
    size_t count = 0;
    double inclusive = 0, exclusive = 0;
};
class TimingModel {
  public:
    void Build(const std::vector<TimingEvent> &events, bool aggregate_all = true,
               const std::atomic<bool>* cancel = nullptr);
    void Aggregate(const std::vector<TimingEvent> &events, double start, double end);
    std::vector<TimerTotal> Relations(uint32_t timer, bool callers) const;
    std::vector<TimingTrack> tracks;
    std::vector<std::string> names;
    std::vector<uint64_t> timer_ids;
    std::vector<bool> timer_has_id;
    std::vector<uint32_t> timers;
    std::vector<int> parents;
    std::vector<TimerTotal> totals;
    std::vector<std::pair<double, double>> frames;
    std::vector<uint64_t> frame_ids;
    bool frames_native=false;
    std::vector<double> clipped, exclusive;
    double start = 0, end = 1, aggregate_start = 0, aggregate_end = 0;

  private:
    std::vector<size_t> active_events_;
};
std::string TimingLabel(double seconds);
uint32_t TimerColour(const std::string &name);
bool TimingMatches(const std::string &value, const char *query);
} // namespace cyinsights
