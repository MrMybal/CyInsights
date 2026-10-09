// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingEvent.hpp"
#include <cstddef>
#include <vector>
#include <utility>
namespace cyinsights {
std::pair<double,double> NavigateTimingWindow(double start,double end,double capture_start,
    double capture_end,double horizontal_delta,double zoom_delta,double anchor=.5);
struct FrameGlyph {size_t frame=0,count=1;float left=0,right=0;};
void BuildFrameGlyphs(const std::vector<std::pair<double,double>>& frames,double start,double end,
                      float width,std::vector<FrameGlyph>& output);
struct TimingGlyph {
    size_t event = 0;
    size_t count = 1;
    int depth = 0;
    double first_start = 0, last_end = 0;
    float left = 0, right = 0;
    bool continues_left = false, continues_right = false;
    bool selected = false, has_unfinished = false;
    bool matches_search = true;
};
// Input indices are ordered by depth and start time (TimingTrack::Query).
// All input scopes overlapping the viewport are represented. Subpixel scopes
// in the same column are combined explicitly instead of silently discarded.
void BuildTimingGlyphs(const std::vector<TimingEvent> &source, const std::vector<size_t> &indices,
                       double start, double end, float width, int selected, std::vector<TimingGlyph> &output,
                       const std::vector<uint8_t> *event_matches = nullptr);
} // namespace cyinsights
