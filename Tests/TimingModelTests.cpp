// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TimingModel.hpp"
#include "AnalysisWorkers.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>

using namespace cyinsights;
namespace {
int checks = 0, failures = 0;
void Check(bool value, const char *name) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << name << "\n";
    }
}
bool Near(double a, double b) { return std::abs(a - b) < 1e-9; }
TimingEvent Event(const char *thread, const char *timer, double start, double end, int depth, uint64_t tid,
                  uint64_t timer_id) {
    TimingEvent e{thread, timer, start, end, depth};
    e.thread_id = tid;
    e.timer_id = timer_id;
    e.has_thread_id = e.has_timer_id = true;
    return e;
}
} // namespace
int main() {
    std::vector<TimingEvent> gpu_groups={Event("GPU","MobileSceneRender",0,.02,0,9,1)};
    for(int i=0;i<5;++i)gpu_groups.push_back(Event("GPU","Group",.019+i*1e-6,.019+i*1e-6+1e-6,1,9,2));
    std::vector<size_t> group_indices={0,1,2,3,4,5};
    auto gpu_detail=InspectGpuTimingDetail(gpu_groups,group_indices);
    Check(gpu_detail.Limited()&&gpu_detail.nested==5&&gpu_detail.near_zero==5,"Collapsed Quest intervals reported as limited group timing, preserving values");
    gpu_groups[1].end=gpu_groups[1].start+.0002;gpu_groups[2].end=gpu_groups[2].start+.0002;
    Check(!InspectGpuTimingDetail(gpu_groups,group_indices).Limited(),"Resolved subgroup timings do not trigger collapsed-interval diagnostic");
    for(auto& group:gpu_groups)group.display_aggregate=true;
    Check(!InspectGpuTimingDetail(gpu_groups,group_indices).Limited(),"Derived display aggregates are not evidence of near-zero native intervals");
    GpuTimingDetail invalid;auto incomplete=Event("GPU","Pending",0,.02,0,9,3);incomplete.open_ended=true;invalid.Add(incomplete);
    Check(invalid.longest_outer==0&&invalid.nested==0,"Unresolved GPU queries excluded from detail diagnostic");
    TimingModel ordered;
    ordered.Build({Event("GameThread","Tick",0,.02,0,1,1),Event("GpuWorkerThread","CPU work",0,.01,0,2,2),
        Event("RenderThread","Render",0,.01,0,3,3),Event("GPU1","Pass",0,.02,0,4,4),
        Event("GPU2","Async pass",0,.02,0,5,5),Event("RHIThread","Submit",0,.01,0,6,6)});
    Check(ordered.tracks[0].gpu&&ordered.tracks[1].gpu&&ordered.tracks[2].name=="GameThread"&&
        ordered.tracks[3].name=="RenderThread"&&ordered.tracks[4].name=="RHIThread"&&!ordered.tracks[5].gpu,
        "All GPU queues precede GameThread; CPU GPU workers remain ordinary CPU tracks");
    std::vector<TimingEvent> events = {Event("Worker", "Outer", 0, 10, 0, 1, 10),
                                       Event("Worker", "Work", 2, 6, 1, 1, 20),
                                       Event("Worker", "Nested", 3, 4, 2, 1, 30),
                                       Event("Worker", "Work", 1, 9, 0, 2, 21),
                                       Event("Worker", "Unfinished", 2, 10, 1, 1, 40),
                                       Event("Worker", "No immediate parent", 6, 7, 3, 1, 50)};
    events[4].open_ended = true;
    TimingModel model;
    model.Build(events);
    Check(model.tracks.size() == 2, "Same-name threads with different IDs must remain separate");
    Check(model.tracks[0].name != model.tracks[1].name, "Duplicate track labels must show distinct identity");
    Check(model.timers[1] != model.timers[3], "Same-name timers with different IDs must remain separate");
    Check(model.parents[1] == 0 && model.parents[2] == 1,
          "Contained immediate children must have their real parent");
    Check(model.parents[3] == -1, "Cross-thread events must never acquire a parent");
    Check(model.parents[4] == -1 && model.parents[5] == -1,
          "Incomplete scopes and depth gaps must not create speculative parent edges");
    model.Aggregate(events, 3, 5);
    Check(Near(model.clipped[0], 2) && Near(model.clipped[1], 2) && Near(model.clipped[2], 1),
          "Inclusive durations must be clipped to the interval");
    Check(Near(model.exclusive[0], 0) && Near(model.exclusive[1], 1),
          "Exclusive durations must subtract immediate children exactly once");
    Check(Near(model.clipped[4], 0), "Unfinished scopes must not report a measured duration");
    auto callers = model.Relations(model.timers[1], true);
    Check(callers.size() == 1 && callers[0].timer == model.timers[0] && Near(callers[0].inclusive, 2),
          "Caller costs must be scoped to the selected child in the interval");
    auto callees = model.Relations(model.timers[0], false);
    Check(callees.size() == 1 && callees[0].timer == model.timers[1],
          "Callee list must contain only known immediate children");
    model.Aggregate(events, 9.5, 10);
    Check(Near(model.clipped[1], 0) && Near(model.exclusive[1], 0),
          "Interval changes must clear stale costs");
    model.Aggregate(events, 5, 3);
    Check(Near(model.clipped[1], 2) && Near(model.aggregate_start, 3),
          "Reverse drag selection must normalize bounds");
    model.Aggregate(events, 4, 4);
    Check(model.totals.empty(), "Empty interval must not retain statistics");

    // Exercise interval lookup against a brute-force reference across many
    // nested windows, including a long-lived outer scope and open-ended scope.
    std::vector<TimingEvent> many{Event("GameThread", "Outer", 0, 100, 0, 7, 1)};
    for (int i = 0; i < 500; ++i) {
        double t = i * .2;
        many.push_back(Event("GameThread", "Work", t, t + .15, 1, 7, 2));
        many.push_back(Event("GameThread", "Child", t + .03, t + .07, 2, 7, 3));
    }
    many.push_back(Event("GameThread", "Unfinished", 0, 100, 3, 7, 4));
    many.back().open_ended = true;
    TimingModel indexed;
    indexed.Build(many);
    std::mt19937 random(42);
    bool lookup_ok = true, totals_ok = true;
    for (int test = 0; test < 100; ++test) {
        double a = static_cast<double>(random() % 98000) / 1000, b = a + .3;
        int levels = test % 4 + 1;
        std::vector<size_t> expected, actual;
        for (size_t i = 0; i < many.size(); ++i)
            if (many[i].start <= b && many[i].end >= a && many[i].depth < levels)
                expected.push_back(i);
        indexed.tracks[0].Query(many, a, b, levels, actual);
        std::sort(actual.begin(), actual.end());
        lookup_ok &= actual == expected;
        indexed.Aggregate(many, a, b);
        double inclusive = 0;
        for (const auto &e : many)
            if (!e.open_ended)
                inclusive += std::max(0., std::min(e.end, b) - std::max(e.start, a));
        double measured = 0;
        for (const auto &row : indexed.totals)
            measured += row.inclusive;
        totals_ok &= Near(measured, inclusive);
    }
    Check(lookup_ok, "Per-depth interval index must match brute-force overlap lookup");
    Check(totals_ok, "Indexed statistics must match brute-force clipped sums");

    // Engine ticks are navigation markers. Scopes from rendering/workers can
    // begin before and finish after the selected tick without being split.
    std::vector<TimingEvent> crossing = {Event("GameThread", "FEngineLoop::Tick", 0, .01, 0, 1, 1),
                                         Event("GameThread", "FEngineLoop::Tick", .01, .02, 0, 1, 1),
                                         Event("GameThread", "FEngineLoop::Tick", .02, .03, 0, 1, 1),
                                         Event("RenderThread", "Render across ticks", .005, .025, 0, 2, 2),
                                         Event("RenderThread", "Nested across ticks", .009, .021, 1, 2, 3),
                                         Event("Worker", "Async task", .002, .028, 0, 3, 4),
                                         Event("Worker", "Later depth", .026, .029, 3, 3, 5)};
    TimingModel continuous;
    continuous.Build(crossing);
    auto render = std::find_if(continuous.tracks.begin(), continuous.tracks.end(),
                               [](const auto &t) { return t.name == "RenderThread"; });
    auto worker = std::find_if(continuous.tracks.begin(), continuous.tracks.end(),
                               [](const auto &t) { return t.name == "Worker"; });
    std::vector<size_t> visible;
    render->Query(crossing, .01, .02, render->RowCount(0), visible);
    Check(visible == std::vector<size_t>({3, 4}),
          "Render scopes crossing a selected tick must remain intact");
    std::vector<TimingGlyph> glyphs;
    BuildTimingGlyphs(crossing, visible, .01, .02, 100, 4, glyphs);
    Check(glyphs.size() == 2 && Near(glyphs[0].left, 0) && Near(glyphs[0].right, 100),
          "Cross-frame scope geometry must cover the entire viewport");
    Check(glyphs[0].continues_left && glyphs[0].continues_right && glyphs[1].selected,
          "Clipped bars must retain continuation and selected-scope information");
    Check(Near(crossing[3].start, .005) && Near(crossing[3].end, .025),
          "Drawing must preserve original execution timestamps");
    visible.clear();
    worker->Query(crossing, .01, .02, worker->RowCount(0), visible);
    Check(visible == std::vector<size_t>({5}),
          "Asynchronous tasks from other threads must overlap the same time axis");
    int stable_rows = worker->RowCount(0);
    visible.clear();
    worker->Query(crossing, .04, .05, stable_rows, visible);
    Check(visible.empty() && stable_rows == 4 && worker->RowCount(0) == stable_rows,
          "Recorded hierarchy is retained even when inactive");
    Check(worker->VisibleRowCount(crossing,.04,.05,0)==0,"Inactive tracks need no empty scope rows");
    Check(worker->VisibleRowCount(crossing,.026,.027,0)==4,"Visible nested scopes retain their original depth rows");
    std::vector<TimingEvent> dynamic{Event("Worker","Root",0,10,0,1,1),Event("Worker","Deep",1,2,8,1,2)};
    TimingModel dynamic_model;dynamic_model.Build(dynamic);
    const auto& dynamic_track=dynamic_model.tracks.front();
    Check(dynamic_track.VisibleRowCount(dynamic,1.2,1.3,0)==9&&dynamic_track.VisibleRowCount(dynamic,4,5,0)==1,
        "Track height shrinks and expands with the visible interval without truncating deep scopes");
    Check(dynamic_track.VisibleRowCount(dynamic,1.2,1.3,3)==1,"Explicit depth limits still apply to visible height");
    std::vector<FrameGlyph> frame_glyphs;
    BuildFrameGlyphs({{0,1},{1,2},{2,3},{3,4}},1.5,2.5,100,frame_glyphs);
    Check(frame_glyphs.size()==2&&frame_glyphs[0].frame==1&&Near(frame_glyphs[0].left,0)&&Near(frame_glyphs[0].right,50)&&Near(frame_glyphs[1].left,50)&&Near(frame_glyphs[1].right,100),
        "Frame bars share the visible timing axis and retain crossing edge frames");
    BuildFrameGlyphs({{0,1},{1,2},{2,3},{3,4}},2,3,100,frame_glyphs);
    Check(frame_glyphs.size()==1&&frame_glyphs.back().frame==2&&Near(frame_glyphs.back().left,0)&&Near(frame_glyphs.back().right,100),
        "Frame positions follow the changed visible interval");
    std::vector<std::pair<double,double>> dense_frames;
    for(int i=0;i<1000;++i)dense_frames.emplace_back(i*.001,(i+1)*.001);
    BuildFrameGlyphs(dense_frames,0,1,10,frame_glyphs);size_t frame_count=0;
    for(const auto& glyph:frame_glyphs)frame_count+=glyph.count;
    Check(frame_count==1000&&frame_glyphs.size()<=11,"Wide frame views retain counts with bounded pixel aggregation");
    Check(worker->RowCount(2) == 2 && worker->RowCount(0) == 4,
          "Depth limits must be explicit; all-depth mode restores the complete track");
    continuous.Aggregate(crossing, .01, .02);
    Check(Near(continuous.clipped[3], .01) && Near(continuous.exclusive[3], 0),
          "Frame selection must clip statistics while preserving cross-frame nesting");
    TimingModel no_ticks;
    no_ticks.Build({Event("Worker", "Task without frame provider", 2, 5, 0, 7, 2)});
    visible.clear();
    no_ticks.tracks[0].Query({Event("Worker", "Task without frame provider", 2, 5, 0, 7, 2)}, 3, 4, 1,
                             visible);
    Check(no_ticks.frames.empty() && visible.size() == 1,
          "Continuous timing must work without engine tick candidates");

    // Dense overview columns represent all their source scopes and disclose
    // aggregation. Zooming onto a member restores its individual geometry.
    std::vector<TimingEvent> dense;
    std::vector<size_t> dense_indices;
    for (int i = 0; i < 100; ++i) {
        dense.push_back(Event("Worker", "Tiny", 1 + i * .00001, 1 + i * .00001 + .000005, 0, 7, 2));
        dense_indices.push_back(static_cast<size_t>(i));
    }
    dense.push_back(Event("Worker", "Nested tiny", 1.00073, 1.000735, 1, 7, 3));
    dense_indices.push_back(100);
    BuildTimingGlyphs(dense, dense_indices, 0, 10, 100, 73, glyphs);
    Check(glyphs.size() == 2 && glyphs[0].count == 100 && glyphs[1].count == 1,
          "Dense columns must group scopes only within their own depth row");
    size_t represented = 0;
    for (const auto &glyph : glyphs)
        represented += glyph.count;
    Check(represented == dense.size() && glyphs[0].selected,
          "Subpixel aggregation must conserve scope count and selected identity");
    std::vector<uint8_t> matches(dense.size(), 0);
    matches[73] = 1;
    BuildTimingGlyphs(dense, dense_indices, 0, 10, 100, -1, glyphs, &matches);
    Check(glyphs[0].matches_search && !glyphs[1].matches_search,
          "Search must highlight a grouped member even when its representative does not match");
    BuildTimingGlyphs(dense, dense_indices, 1.000729, 1.000736, 100, 73, glyphs);
    Check(glyphs.size() == 2 && glyphs[0].count == 1 && glyphs[0].event == 73 && glyphs[0].selected,
          "Zooming must resolve an aggregated member into its real scope");
    auto point = std::vector<TimingEvent>{Event("Worker", "Zero duration", 1, 1, 0, 7, 4)};
    BuildTimingGlyphs(point, {0}, 0, 2, 100, -1, glyphs);
    Check(glyphs.size() == 1 && Near(glyphs[0].right - glyphs[0].left, 1),
          "Zero-duration native records must remain visible as a point");
    BuildTimingGlyphs(point, {0}, 0, 2, 0, -1, glyphs);
    Check(glyphs.empty(), "An empty drawing area must not produce invalid glyphs");

    indexed.Build({});
    Check(indexed.tracks.empty() && indexed.totals.empty() && indexed.names.empty(),
          "Rebuilding with an empty capture must clear old indices");
    auto frame=Event("Game Frames","Game Frame 7",1,2,1,1ull<<24,7);
    frame.frame_type=0;frame.frame_index=7;frame.has_timer_id=false;
    auto rendering=frame;rendering.thread="Rendering Frames";rendering.thread_id++;rendering.frame_type=1;rendering.name="Rendering Frame 7";
    std::vector<TimingEvent> framed{frame,rendering,Event("Worker","Work",1,2,0,1,7),Event("GameThread","FEngineLoop::Tick",1.1,1.8,0,2,8)};
    TimingModel frame_model;frame_model.Build(framed);
    Check(frame_model.timers[0]!=frame_model.timers[2]&&frame_model.timers[0]!=frame_model.timers[1],"Frame indices must not collide with native timer IDs or other frame types");
    Check(frame_model.frames_native&&frame_model.frames.size()==1&&frame_model.frame_ids[0]==7&&Near(frame_model.frames[0].first,1),"Measured native game frames take precedence over derived tick candidates");
    Check(frame_model.totals.size()==2&&frame_model.clipped[0]==0&&frame_model.clipped[1]==0,"Frame rows remain visible but must not inflate timer costs");
    frame_model.Build({});Check(!frame_model.frames_native&&frame_model.frame_ids.empty(),"Native frame identity resets with the capture");
    std::vector<TimingEvent> parallel_fixture;
    for(uint64_t track=1;track<=4;++track)for(int i=0;i<5000;++i) {
        const double begin=i*.01;
        parallel_fixture.push_back(Event("Worker","Parent",begin,begin+.009,0,track,track*2));
        parallel_fixture.push_back(Event("Worker","Child",begin+.001,begin+.006,1,track,track*2+1));
    }
    TimingModel serial_model,parallel_model;SetAnalysisWorkerCount(1);serial_model.Build(parallel_fixture);
    SetAnalysisWorkerCount(8);parallel_model.Build(parallel_fixture);SetAnalysisWorkerCount(0);
    Check(serial_model.parents==parallel_model.parents&&serial_model.timers==parallel_model.timers&&serial_model.names==parallel_model.names&&serial_model.timer_ids==parallel_model.timer_ids&&serial_model.exclusive==parallel_model.exclusive&&serial_model.clipped==parallel_model.clipped,"Parallel track construction retains exact timer indices, parent relations and costs");
    bool same_tracks=serial_model.tracks.size()==parallel_model.tracks.size();
    if(same_tracks)for(size_t i=0;i<serial_model.tracks.size();++i) {
        const auto& a=serial_model.tracks[i];const auto& b=parallel_model.tracks[i];
        same_tracks&=a.id==b.id&&a.name==b.name&&a.events==b.events&&a.depths.size()==b.depths.size();
        if(a.depths.size()==b.depths.size())for(size_t d=0;d<a.depths.size();++d)same_tracks&=a.depths[d].events==b.depths[d].events&&a.depths[d].prefix_end==b.depths[d].prefix_end;
    }
    Check(same_tracks,"Parallel track construction retains stable ordering and interval indices");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
