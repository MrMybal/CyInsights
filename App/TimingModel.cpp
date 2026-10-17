// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TimingModel.hpp"
#include "AnalysisWorkers.hpp"
#include "FrameDiagnostics.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>
#include <stdexcept>
namespace cyinsights {
int TimingTrackRank(const TimingTrack& t) {
    if(t.gpu)return 0;
    if(t.name=="GameThread"||t.name.rfind("GameThread [#",0)==0)return 1;
    if(TimingMatches(t.name,"RenderThread")||TimingMatches(t.name,"RenderingThread"))return 2;
    if(TimingMatches(t.name,"RHI"))return 3;
    return 4;
}
int TimingTrack::RowCount(int limit) const {
    int count = static_cast<int>(depths.size());
    return std::max(1, limit > 0 ? std::min(count, limit) : count);
}
int TimingTrack::VisibleRowCount(const std::vector<TimingEvent>& source,double start,double end,int limit) const {
    if(end<start)return 0;
    for(int level=std::min(RowCount(limit),static_cast<int>(depths.size()))-1;level>=0;--level) {
        const auto& depth=depths[static_cast<size_t>(level)];
        auto first=std::lower_bound(depth.prefix_end.begin(),depth.prefix_end.end(),start);
        for(size_t pos=static_cast<size_t>(first-depth.prefix_end.begin());pos<depth.events.size();++pos) {
            const auto& event=source[depth.events[pos]];if(event.start>end)break;
            if(event.end>=start)return level+1;
        }
    }
    return 0;
}
void TimingTrack::Query(const std::vector<TimingEvent> &source, double start, double end, int levels,
                        std::vector<size_t> &result) const {
    if (end < start || levels <= 0)
        return;
    for (size_t level = 0; level < depths.size() && level < static_cast<size_t>(levels); ++level) {
        const auto &depth = depths[level];
        auto first = std::lower_bound(depth.prefix_end.begin(), depth.prefix_end.end(), start);
        for (size_t pos = static_cast<size_t>(first - depth.prefix_end.begin()); pos < depth.events.size();
             ++pos) {
            auto i = depth.events[pos];
            const auto &event = source[i];
            if (event.start > end)
                break;
            if (event.end >= start)
                result.push_back(i);
        }
    }
}
bool TimingMatches(const std::string &value, const char *query) {
    std::string a = value, b = query;
    auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    std::transform(a.begin(), a.end(), a.begin(), lower);
    std::transform(b.begin(), b.end(), b.begin(), lower);
    return a.find(b) != std::string::npos;
}
std::string TimingLabel(double seconds) {
    char label[64];
    if (std::abs(seconds) >= 60)
        std::snprintf(label, sizeof(label), "%dm %.4fs", static_cast<int>(seconds / 60),
                      std::fmod(seconds, 60));
    else if (std::abs(seconds) >= 1)
        std::snprintf(label, sizeof(label), "%.4fs", seconds);
    else
        std::snprintf(label, sizeof(label), "%.3fms", seconds * 1000);
    return label;
}
uint32_t TimerColour(const std::string &name) {
    // Stable pastel palette with dark labels, as in native timing tracks.
    constexpr uint32_t palette[] = {0xffd288dc, 0xffcec974, 0xff94d7dc, 0xffdad2a9, 0xffd59770, 0xff80d1a7,
                                    0xffa7bded, 0xffcfa3b9, 0xff83c7e8, 0xffc5e692, 0xffdb8aad, 0xffaad2bf};
    uint32_t hash = 2166136261u;
    for (unsigned char c : name) {
        hash ^= c;
        hash *= 16777619u;
    }
    return palette[hash % 12];
}
void TimingModel::Build(const std::vector<TimingEvent> &events, bool aggregate_all,
                        const std::atomic<bool>* cancel) {
    auto check_cancel=[&]{if(cancel&&cancel->load())throw std::runtime_error("Analysis cancelled");};
    check_cancel();
    tracks.clear();
    names.clear();
    timer_ids.clear();
    timer_has_id.clear();
    totals.clear();
    frames.clear();
    frame_ids.clear();frames_native=false;
    std::vector<std::pair<std::pair<double,double>,uint64_t>> native_frames;
    timers.resize(events.size());
    parents.assign(events.size(), -1);
    clipped.clear();
    exclusive.clear();
    active_events_.clear();
    std::unordered_map<std::string, size_t> threads;
    std::unordered_map<std::string, uint32_t> ids;
    std::unordered_map<uint64_t, size_t> native_threads;
    std::unordered_map<uint64_t, uint32_t> native_timers;
    std::map<std::pair<int,uint64_t>,uint32_t> frame_timers;
    start = events.empty() ? 0 : events.front().start;
    end = start;
    for (size_t i = 0; i < events.size(); ++i) {
        if(i%4096==0)check_cancel();
        const auto &e = events[i];
        start = std::min(start, e.start);
        end = std::max(end, e.end);
        size_t track_index;
        bool new_track;
        if (e.has_thread_id) {
            auto t = native_threads.try_emplace(e.thread_id, tracks.size());
            track_index = t.first->second;
            new_track = t.second;
        } else {
            auto t = threads.try_emplace(e.thread, tracks.size());
            track_index = t.first->second;
            new_track = t.second;
        }
        if (new_track) {
            TimingTrack track;
            track.name = e.thread;
            track.id = e.thread_id;
            track.has_id = e.has_thread_id;
            track.gpu = DiagnosticDomain(e.thread)==FrameDomain::Gpu;
            tracks.push_back(std::move(track));
        }
        tracks[track_index].events.push_back(i);
        uint32_t timer_index;
        bool new_timer;
        if(e.frame_type>=0) {
            auto n=frame_timers.try_emplace(std::make_pair(e.frame_type,e.frame_index),static_cast<uint32_t>(names.size()));
            timer_index=n.first->second;new_timer=n.second;
        } else if (e.has_timer_id) {
            auto n = native_timers.try_emplace(e.timer_id, static_cast<uint32_t>(names.size()));
            timer_index = n.first->second;
            new_timer = n.second;
        } else {
            auto n = ids.try_emplace(e.name, static_cast<uint32_t>(names.size()));
            timer_index = n.first->second;
            new_timer = n.second;
        }
        if (new_timer) {
            names.push_back(e.name);
            timer_ids.push_back(e.timer_id);
            timer_has_id.push_back(e.has_timer_id);
        }
        timers[i] = timer_index;
        if(e.frame_type==0&&!e.open_ended&&e.end>e.start)native_frames.push_back({{e.start,e.end},e.frame_index});
        if (!e.open_ended && e.thread == "GameThread" &&
            (e.name == "FEngineLoop::Tick" || e.name == "EngineLoopTick"))
            frames.emplace_back(e.start, e.end);
    }
    if (end <= start)
        end = start + .001;
    std::sort(frames.begin(), frames.end());
    if(!native_frames.empty()) {
        std::sort(native_frames.begin(),native_frames.end());frames.clear();frames_native=true;
        for(const auto& frame:native_frames){frames.push_back(frame.first);frame_ids.push_back(frame.second);}
    }
    ParallelAnalysis(tracks.size(),events.size()>=32768?AnalysisWorkerCount():1,[&](size_t track_index) {
        auto& t=tracks[track_index];
        check_cancel();
        std::stable_sort(t.events.begin(), t.events.end(), [&](size_t a, size_t b) {
            const auto &x = events[a];
            const auto &y = events[b];
            if (x.start != y.start)
                return x.start < y.start;
            if (x.depth != y.depth)
                return x.depth < y.depth;
            return x.end > y.end;
        });
        for (auto i : t.events) {
            if(i%4096==0)check_cancel();
            auto depth = static_cast<size_t>(events[i].depth);
            if (t.depths.size() <= depth)
                t.depths.resize(depth + 1);
            auto &lane = t.depths[depth];
            lane.events.push_back(i);
            lane.prefix_end.push_back(
                lane.prefix_end.empty() ? events[i].end : std::max(lane.prefix_end.back(), events[i].end));
        }
        std::vector<size_t> stack;
        for (auto i : t.events) {
            if(i%4096==0)check_cancel();
            const auto &e = events[i];
            while (!stack.empty() &&
                   (events[stack.back()].end <= e.start || events[stack.back()].depth >= e.depth))
                stack.pop_back();
            if (!stack.empty()) {
                auto p = stack.back();
                const auto &parent = events[p];
                if (parent.depth + 1 == e.depth && parent.start <= e.start && parent.end >= e.end &&
                    !parent.open_ended && !e.open_ended)
                    parents[i] = static_cast<int>(p);
            }
            stack.push_back(i);
        }
    },cancel);
    std::stable_sort(tracks.begin(), tracks.end(),
                     [&](const auto &a, const auto &b) { return TimingTrackRank(a) < TimingTrackRank(b); });
    std::unordered_map<std::string, size_t> name_counts;
    for (const auto &track : tracks)
        ++name_counts[track.name];
    for (auto &track : tracks)
        if (name_counts[track.name] > 1)
            track.name += track.has_id ? " [#" + std::to_string(track.id) + "]" : " [identity unavailable]";
    check_cancel();
    if(aggregate_all)Aggregate(events, start, end);
}
void TimingModel::Aggregate(const std::vector<TimingEvent> &events, double a, double b) {
    if (b < a)
        std::swap(a, b);
    aggregate_start = a;
    aggregate_end = b;
    for (auto i : active_events_) {
        clipped[i] = exclusive[i] = 0;
    }
    active_events_.clear();
    clipped.resize(events.size(), 0);
    exclusive.resize(events.size(), 0);
    totals.assign(names.size(), {});
    for (const auto &track : tracks)
        track.Query(events, a, b, 4097, active_events_);
    active_events_.erase(std::remove_if(active_events_.begin(), active_events_.end(),
                                        [&](size_t i) {
                                            const auto &e = events[i];
                                            return e.frame_type>=0 || e.display_aggregate || e.open_ended || e.end <= a || e.start >= b ||
                                                   e.end <= e.start;
                                        }),
                         active_events_.end());
    for (auto i : active_events_) {
        const auto &e = events[i];
        clipped[i] = e.open_ended ? 0 : std::max(0., std::min(e.end, b) - std::max(e.start, a));
        exclusive[i] = clipped[i];
    }
    for (auto i : active_events_)
        if (parents[i] >= 0)
            exclusive[static_cast<size_t>(parents[i])] -= clipped[i];
    for (auto i : active_events_) {
        exclusive[i] = std::max(0., exclusive[i]);
        if (clipped[i] <= 0)
            continue;
        auto &total = totals[timers[i]];
        total.timer = timers[i];
        ++total.count;
        total.inclusive += clipped[i];
        total.exclusive += exclusive[i];
    }
    totals.erase(std::remove_if(totals.begin(), totals.end(), [](const auto &row) { return row.count == 0; }),
                 totals.end());
    std::sort(totals.begin(), totals.end(),
              [](const auto &a, const auto &b) { return a.inclusive > b.inclusive; });
}
std::vector<TimerTotal> TimingModel::Relations(uint32_t timer, bool callers) const {
    std::vector<TimerTotal> rows(names.size());
    for (auto i : active_events_) {
        if (clipped[i] <= 0 || parents[i] < 0)
            continue;
        auto p = static_cast<size_t>(parents[i]);
        if (callers ? timers[i] != timer : timers[p] != timer)
            continue;
        auto key = callers ? timers[p] : timers[i];
        auto &row = rows[key];
        row.timer = key;
        ++row.count;
        row.inclusive += clipped[i];
        row.exclusive += exclusive[i];
    }
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](const auto &r) { return r.count == 0; }),
               rows.end());
    std::sort(rows.begin(), rows.end(),
              [](const auto &a, const auto &b) { return a.inclusive > b.inclusive; });
    return rows;
}
} // namespace cyinsights
