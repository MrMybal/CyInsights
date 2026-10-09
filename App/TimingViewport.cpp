// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "TimingViewport.hpp"
#include <algorithm>
#include <cmath>
namespace cyinsights {
void BuildFrameGlyphs(const std::vector<std::pair<double,double>>& frames,double start,double end,
                      float width,std::vector<FrameGlyph>& output) {
    output.clear();
    if(!std::isfinite(start)||!std::isfinite(end)||end<=start||!std::isfinite(width)||width<=0)return;
    auto first=std::lower_bound(frames.begin(),frames.end(),start,
        [](const auto& frame,double value){return frame.first<value;});
    // Frame guides are chronological intervals. Retain the frame crossing the left edge.
    if(first!=frames.begin())--first;
    int last_column=-1;bool previous_small=false;
    for(auto it=first;it!=frames.end()&&it->first<end;++it) {
        if(it->second<=start)continue;
        float left=static_cast<float>(std::clamp((it->first-start)/(end-start),0.,1.)*width);
        float right=static_cast<float>(std::clamp((it->second-start)/(end-start),0.,1.)*width);
        const bool small=right-left<1;int column=static_cast<int>(std::min(left,std::max(0.f,width-1)));
        const auto index=static_cast<size_t>(it-frames.begin());
        if(small&&previous_small&&column==last_column&&!output.empty()) {
            auto& group=output.back();++group.count;
            auto representative=frames[group.frame];
            if(it->second-it->first>representative.second-representative.first)group.frame=index;
            group.right=std::max(group.right,std::min(width,left+1));
        }else output.push_back({index,1,left,std::min(width,std::max(left+1,right))});
        previous_small=small;last_column=column;
    }
}
std::pair<double,double> NavigateTimingWindow(double start,double end,double capture_start,
    double capture_end,double horizontal_delta,double zoom_delta,double anchor) {
    if(!std::isfinite(start)||!std::isfinite(end)||!std::isfinite(capture_start)||
       !std::isfinite(capture_end)||!std::isfinite(horizontal_delta)||!std::isfinite(zoom_delta)||capture_end<=capture_start)return {start,end};
    anchor=std::clamp(anchor,0.,1.);
    double span=std::clamp((end-start)*std::exp(std::clamp(zoom_delta,-50.,50.)),1e-9,capture_end-capture_start);
    double time=start+(end-start)*anchor+horizontal_delta;
    double first=std::clamp(time-span*anchor,capture_start,capture_end-span);
    return {first,first+span};
}
void BuildTimingGlyphs(const std::vector<TimingEvent> &source, const std::vector<size_t> &indices,
                       double start, double end, float width, int selected, std::vector<TimingGlyph> &output,
                       const std::vector<uint8_t> *event_matches) {
    output.clear();
    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(width) || end <= start || width <= 0)
        return;
    const double span = end - start;
    if (!std::isfinite(span))
        return;
    int last_column = -1;
    bool previous_subpixel = false;
    for (auto i : indices) {
        const auto &event = source[i];
        bool matches = event_matches == nullptr || (*event_matches)[i] != 0;
        if (event.end < start || event.start > end)
            continue;
        float left = static_cast<float>(std::clamp((event.start - start) / span, 0., 1.) * width);
        float right = static_cast<float>(std::clamp((event.end - start) / span, 0., 1.) * width);
        bool subpixel = right - left < 1.f;
        int column = static_cast<int>(std::floor(std::min(left, std::max(0.f, width - 1))));
        if (subpixel && previous_subpixel && !output.empty() && output.back().depth == event.depth &&
            column == last_column) {
            auto &glyph = output.back();
            glyph.count+=static_cast<size_t>(event.represented);
            glyph.first_start = std::min(glyph.first_start, event.start);
            glyph.last_end = std::max(glyph.last_end, event.end);
            glyph.selected |= static_cast<int>(i) == selected;
            glyph.has_unfinished |= event.open_ended;
            glyph.matches_search |= matches;
            glyph.continues_left |= event.start < start;
            glyph.continues_right |= event.end > end;
        } else {
            TimingGlyph glyph;
            glyph.event = i;
            glyph.count=static_cast<size_t>(event.represented);
            glyph.depth = event.depth;
            glyph.first_start = event.start;
            glyph.last_end = event.end;
            glyph.left = subpixel ? static_cast<float>(column) : left;
            glyph.right = subpixel ? std::min(width, glyph.left + 1) : right;
            glyph.continues_left = event.start < start;
            glyph.continues_right = event.end > end;
            glyph.selected = static_cast<int>(i) == selected;
            glyph.has_unfinished = event.open_ended;
            glyph.matches_search = matches;
            output.push_back(glyph);
        }
        previous_subpixel = subpixel;
        last_column = column;
    }
}
} // namespace cyinsights
