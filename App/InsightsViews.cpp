// Dear ImGui implementation of the native Timing Insights layout.
// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "UiMetrics.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
namespace cyinsights {
namespace {
ImU32 CauseColour(FrameCause cause,int opacity=255) {
    ImU32 colour;
    switch(cause){case FrameCause::Game:colour=cyui::categorical(1);break;
        case FrameCause::Render:colour=cyui::categorical(0);break;
        case FrameCause::Gpu:colour=cyui::palette().success;break;
        case FrameCause::Mixed:colour=cyui::categorical(3);break;
        case FrameCause::Wait:colour=cyui::palette().warning;break;
        default:colour=cyui::palette().textDim;break;}
    return cyui::alpha(colour,float(opacity)/255);
}
void PanelToggle(const char* icon,const char* label,bool& open) {
    cyui::ScopedID id(label);
    if(cyui::iconButton(icon,label,open))open=!open;
    ImGui::SameLine(0,cyui::dp(2));
}
} // namespace
void Application::Toolbar() {
    const auto* viewport=WorkspaceViewport();
    if(!hosted_){ImGui::SetNextWindowPos(viewport->WorkPos);ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x,NavigationHeight()));}
    cyui::ScopedVar padding(ImGuiStyleVar_WindowPadding,cyui::dp(8,4));
    if(hosted_)ImGui::BeginChild("##InsightsNavigation",ImVec2(0,NavigationHeight()),ImGuiChildFlags_AlwaysUseWindowPadding,ImGuiWindowFlags_NoScrollbar);
    else BeginView("##InsightsNavigation",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar);
    const char* modes[]={"Session & capture","Timing Insights"};int mode=session_view_?0:1;
    if(cyui::segmented("##workspace_mode",modes,2,&mode,cyui::dp(290)))session_view_=mode==0;
    ImGui::SameLine(0,cyui::dp(12));
    const bool updating=native_.store&&(model_task_.valid()||loaded_start_!=view_start_||loaded_end_!=view_end_);
    if(AnalysisBusy()||updating){cyui::spinner("##analysis",6);ImGui::SameLine();}
    cyui::textDim("%s",AnalysisBusy()?"Analyzing capture...":updating?"Updating visible interval...":native_.store?"Ready to explore":native_.events.empty()?"Open a capture or connect a device":"Native timing ready");
    ImGui::Separator();
    cyui::textDim("Tracks");ImGui::SameLine();
    PanelToggle(cyui::icon::grid,"Frames - show / hide (View menu)",show_frames_);
    PanelToggle(cyui::icon::clock,"Timing - show / hide (View menu)",show_timing_);
    PanelToggle(cyui::icon::equalizer,"Counters - show / hide (View menu)",show_counters_);
    {bool old=show_frame_stats_;PanelToggle(cyui::icon::ruler,"Frame stats - show / hide (View menu)",show_frame_stats_);if(show_frame_stats_&&!old)frame_stats_dock_pending_=true;}
    cyui::toolbarSeparator();
    cyui::textDim("Analyze");ImGui::SameLine();
    PanelToggle(cyui::icon::list,"Timers - show / hide (View menu)",show_timers_);
    PanelToggle(cyui::icon::back,"Callers - show / hide (View menu)",show_callers_);
    PanelToggle(cyui::icon::forward,"Callees - show / hide (View menu)",show_callees_);
    PanelToggle(cyui::icon::sort,"Distributions / function histogram (scope context menu)",show_distribution_);
    PanelToggle(cyui::icon::shuffle,"Tasks - show / hide (View menu)",show_tasks_);
    {bool old=show_activities_;PanelToggle(cyui::icon::globe,"Network, storage & memory (View menu)",show_activities_);if(show_activities_&&!old)FocusActivities();}
    PanelToggle(cyui::icon::viewAll,"Compare captures (Analysis menu)",show_comparison_);
    PanelToggle(cyui::icon::code,"Detailed GPU frame (View menu)",show_gpu_);
    cyui::toolbarSeparator();
    {bool old=show_ai_;PanelToggle(cyui::icon::lightbulb,"AI Inspector / standalone tools (View menu)",show_ai_);if(show_ai_&&!old)FocusAi();}
    if(cyui::ghostButton("Panels"))ImGui::OpenPopup("##panel_picker");
    if(ImGui::BeginPopup("##panel_picker")){PanelMenu();ImGui::EndPopup();}
    if(hosted_)ImGui::EndChild();else ImGui::End();
}
void Application::FitTiming() {
    view_start_ = timing_model_.start;
    view_end_ = timing_model_.end;
    if(native_.events.empty()&&!native_.store){FitCapture();return;}
    if (!cpu_frames_.empty()) {
        // Start with a readable span of consecutive ticks, rather than a whole
        // capture compressed into sub-pixel bars. The overview retains all ticks.
        size_t index = cpu_frames_.size() / 2;
        size_t first = index > 1 ? index - 1 : 0, last = std::min(cpu_frames_.size() - 1, first + 2);
        view_start_ = cpu_frames_[first].first;
        view_end_ = cpu_frames_[last].second;
        double padding = std::max(.00001, (view_end_ - view_start_) * .03);
        view_start_ -= padding;
        view_end_ += padding;
    }
}
void Application::StepFrame(int direction) {
    if (cpu_frames_.empty())
        return;
    double anchor =
        range_start_ == range_end_ ? (view_start_ + view_end_) / 2 : (range_start_ + range_end_) / 2;
    auto after = std::upper_bound(cpu_frames_.begin(), cpu_frames_.end(), anchor,
                                  [](double t, const auto &frame) { return t < frame.first; });
    auto index = static_cast<int64_t>(after - cpu_frames_.begin()) - 1;
    index = std::clamp(index + direction, int64_t{0}, static_cast<int64_t>(cpu_frames_.size() - 1));
    auto i = static_cast<size_t>(index);
    range_start_ = cpu_frames_[i].first;
    range_end_ = cpu_frames_[i].second;
    view_start_ = cpu_frames_[i == 0 ? 0 : i - 1].first;
    view_end_ = cpu_frames_[std::min(i + 1, cpu_frames_.size() - 1)].second;
    auto padding = std::max(.00001, (view_end_ - view_start_) * .03);
    view_start_ -= padding;
    view_end_ += padding;
}
bool Application::FocusSlowFrame() {
    if(cpu_frames_.empty())return false;
    auto next=std::upper_bound(cpu_frames_.begin(),cpu_frames_.end(),(view_start_+view_end_)/2,
        [](double t,const auto& frame){return t<frame.first;});
    size_t first=static_cast<size_t>(next-cpu_frames_.begin()),chosen=cpu_frames_.size();
    for(size_t step=0;step<cpu_frames_.size();++step) {
        size_t i=(first+step)%cpu_frames_.size();auto f=cpu_frames_[i];if(f.second-f.first<=1./frame_target_fps_)continue;
        if(chosen==cpu_frames_.size())chosen=i;
        auto cause=Cause(i);if(cause==FrameCause::Game||cause==FrameCause::Render||cause==FrameCause::Gpu||cause==FrameCause::Mixed){chosen=i;break;}
    }
    if(chosen==cpu_frames_.size())return false;
    range_start_=cpu_frames_[chosen].first;range_end_=cpu_frames_[chosen].second;
    view_start_=cpu_frames_[chosen?chosen-1:0].first;view_end_=cpu_frames_[std::min(chosen+1,cpu_frames_.size()-1)].second;
    auto padding=std::max(.00001,(view_end_-view_start_)*.03);view_start_-=padding;view_end_+=padding;
    return true;
}
void Application::FocusTimer(uint32_t timer) {
    double anchor =
        range_start_ == range_end_ ? (view_start_ + view_end_) / 2 : (range_start_ + range_end_) / 2;
    size_t nearest = native_.events.size();
    double distance = INFINITY;
    for (size_t i = 0; i < native_.events.size(); ++i) {
        const auto &event = native_.events[i];
        if (timing_model_.timers[i] != timer || event.open_ended || event.end <= event.start)
            continue;
        double delta = anchor < event.start ? event.start - anchor
                       : anchor > event.end ? anchor - event.end
                                            : 0;
        if (delta < distance) {
            nearest = i;
            distance = delta;
        }
    }
    if (nearest == native_.events.size())
    {
        if(native_.store&&timer<timing_model_.names.size()) {
            auto event=native_.store->Nearest(timing_model_.timer_ids[timer],timing_model_.timer_has_id[timer],timing_model_.names[timer],anchor);
            if(event){auto padding=std::max(.000001,(event->end-event->start)*.08);view_start_=event->start-padding;view_end_=event->end+padding;range_start_=event->start;range_end_=event->end;}
        }
        return;
    }
    const auto &event = native_.events[nearest];
    selected_cpu_ = static_cast<int>(nearest);
    selected_event_ = -1;
    selected_timer_ = static_cast<int>(timer);
    range_start_ = event.start;
    range_end_ = event.end;
    auto padding = std::max(.000001, (event.end - event.start) * .08);
    view_start_ = event.start - padding;
    view_end_ = event.end + padding;
    show_timing_ = true;
    track_filter_ = 0;
    for (auto &track : timing_model_.tracks)
        if (std::find(track.events.begin(), track.events.end(), nearest) != track.events.end()) {
            track.visible = true;
            track.collapsed = false;
            track.focus_depth = event.depth;
            break;
        }
    if (depth_levels_ > 0)
        depth_levels_ = std::clamp(std::max(depth_levels_, event.depth + 1), 1, 4097);
    ImGui::SetWindowFocus(WindowName("Timing").c_str());
}
void Application::UpdateTiming() {
    if (native_.events.empty())
        return;
    double a = range_start_ == range_end_ ? view_start_ : std::min(range_start_, range_end_),
           b = range_start_ == range_end_ ? view_end_ : std::max(range_start_, range_end_);
    if (!native_.store&&(a != statistics_start_ || b != statistics_end_)) {
        timing_model_.Aggregate(native_.events, a, b);
        statistics_start_ = a;
        statistics_end_ = b;
        ++statistics_version_;
        statistics_timer_ = -2;
    }
    if (selected_timer_ != statistics_timer_) {
        callers_.clear();
        callees_.clear();
        if (selected_timer_ >= 0&&!window_aggregated_) {
            callers_ = timing_model_.Relations(static_cast<uint32_t>(selected_timer_), true);
            callees_ = timing_model_.Relations(static_cast<uint32_t>(selected_timer_), false);
        }
        statistics_timer_ = selected_timer_;
    }
}
void Application::BeginTimingDrag(DragSurface surface,int button,float width,bool zoom) {
    if(drag_surface_!=DragSurface::None)return;
    const auto& io=ImGui::GetIO();
    drag_surface_=surface;drag_button_=button;drag_width_=std::max(1.f,width);
    drag_zoom_=zoom;drag_moved_=false;
    overview_drag_start_=view_start_;overview_drag_end_=view_end_;
    overview_drag_x_=io.MousePos.x;overview_drag_y_=io.MousePos.y;
    drag_scroll_start_=tracks_scroll_;drag_context_name_.clear();
}
void Application::AdvanceTimingDrag() {
    drag_released_surface_=DragSurface::None;
    if(drag_surface_==DragSurface::None)return;
    if(drag_surface_==DragSurface::Measurement)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    else if(drag_button_==ImGuiMouseButton_Right)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    const auto& io=ImGui::GetIO();
    const double dx=io.MousePos.x-overview_drag_x_,dy=io.MousePos.y-overview_drag_y_;
    drag_moved_|=dx*dx+dy*dy>=9*UiScale()*UiScale();
    if(drag_moved_&&(ImGui::IsMouseDown(drag_button_)||ImGui::IsMouseReleased(drag_button_))) {
        if(drag_surface_==DragSurface::Measurement)
            range_end_=overview_drag_start_+std::clamp(double(io.MousePos.x-measure_drag_origin_)/drag_width_,0.,1.)*(overview_drag_end_-overview_drag_start_);
        else {
            auto next=NavigateTimingWindow(overview_drag_start_,overview_drag_end_,timing_model_.start,timing_model_.end,
                -dx/drag_width_*(overview_drag_end_-overview_drag_start_),drag_zoom_?dy*.012:0);
            view_start_=next.first;view_end_=next.second;
        }
    }
    if(!ImGui::IsMouseDown(drag_button_)){drag_released_surface_=drag_surface_;drag_surface_=DragSurface::None;}
}
void Application::MeasurementRuler(float width) {
    const auto p=ImGui::GetCursorScreenPos();const float scale=UiScale(),height=48*scale;
    measure_x_=p.x;measure_y_=p.y;measure_width_=width;measure_height_=height;
    const double span=std::max(.000001,view_end_-view_start_);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,ImVec2(p.x+width,p.y+height),cyui::palette().bg1);
    ImGui::InvisibleButton("measurement_ruler",ImVec2(width,height));
    const bool hovered=ImGui::IsItemHovered();
    if(hovered)ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if(hovered&&ImGui::IsMouseClicked(0)&&drag_surface_==DragSurface::None) {
        BeginTimingDrag(DragSurface::Measurement,0,width,false);measure_drag_origin_=p.x;
        range_start_=range_end_=view_start_+std::clamp(double(ImGui::GetIO().MousePos.x-p.x)/width,0.,1.)*span;
    }
    // Tick density follows screen pixels, independently of trace size. The
    // measurement ruler shares the tracks' exact time-to-pixel projection.
    const double desired=span*110*scale/std::max(1.f,width),power=std::pow(10.,std::floor(std::log10(desired)));
    const double ratio=desired/power,major=power*(ratio<=1?1:ratio<=2?2:ratio<=5?5:10),minor=major/5;
    const double first=std::ceil(view_start_/minor);
    const int ticks=std::min(8192,int(std::ceil(span/minor))+2);
    const int precision=std::clamp(int(std::ceil(-std::log10(major))),3,9);
    draw->PushClipRect(p,ImVec2(p.x+width,p.y+height),true);
    if(range_start_!=range_end_) {
        const float a=p.x+width*float((std::min(range_start_,range_end_)-view_start_)/span),
                    b=p.x+width*float((std::max(range_start_,range_end_)-view_start_)/span);
        draw->AddRectFilled(ImVec2(a,p.y),ImVec2(b,p.y+height),cyui::palette().selection);
        for(float x:{a,b}){draw->AddLine(ImVec2(x,p.y),ImVec2(x,p.y+height),cyui::palette().accent,2*scale);draw->AddTriangleFilled(ImVec2(x-4*scale,p.y),ImVec2(x+4*scale,p.y),ImVec2(x,p.y+5*scale),cyui::palette().accent);}
    }
    for(int i=0;i<ticks;++i) {
        const double index=first+i,t=index*minor;if(t>view_end_)break;
        const float x=p.x+width*float((t-view_start_)/span);
        const bool large=std::abs(std::remainder(index,5.))<.1;
        draw->AddLine(ImVec2(x,p.y),ImVec2(x,p.y+(large?10:5)*scale),large?cyui::palette().textDim:cyui::palette().gridStrong);
        if(large){char text[96];std::snprintf(text,sizeof(text),"%.*f s",precision,t);draw->AddText(ImVec2(x+3*scale,p.y+11*scale),cyui::palette().text,text);}
    }
    char label[256];
    if(range_start_!=range_end_)std::snprintf(label,sizeof(label),"Range: %.6f - %.6f s  |  %.3f ms",std::min(range_start_,range_end_),std::max(range_start_,range_end_),std::abs(range_end_-range_start_)*1000);
    else std::snprintf(label,sizeof(label),"Measure interval: drag here with the left mouse button");
    draw->AddText(ImVec2(p.x+7*scale,p.y+31*scale),cyui::palette().accent,label);
    draw->AddLine(ImVec2(p.x,p.y+height-1),ImVec2(p.x+width,p.y+height-1),cyui::palette().lineStrong);
    draw->PopClipRect();
    if(hovered&&drag_surface_!=DragSurface::Measurement)ImGui::SetTooltip("Drag to measure a time range. The view stays fixed; statistics and histograms use this selection. Escape clears the range.");
}
void Application::FrameGlyphs(float width) {
    if(frame_glyph_start_==view_start_&&frame_glyph_end_==view_end_&&frame_glyph_width_==width)return;
    BuildFrameGlyphs(cpu_frames_,view_start_,view_end_,width,frame_glyphs_);
    frame_glyph_start_=view_start_;frame_glyph_end_=view_end_;frame_glyph_width_=width;
}
FrameCause Application::Cause(size_t frame) const {
    if(!native_.store||frame>=native_.store->frame_diagnostics.size())return FrameCause::Unknown;
    return DiagnoseFrame(native_.store->frame_diagnostics[frame],cpu_frames_[frame].second-cpu_frames_[frame].first,1./frame_target_fps_);
}
void Application::FrameBackground(float x,float y,float height,FrameDomain domain) {
    if(!show_frame_causes_||!native_.store||domain==FrameDomain::Other)return;
    auto* draw=ImGui::GetWindowDrawList();
    for(const auto& glyph:frame_glyphs_) {
        if(glyph.frame>=native_.store->frame_diagnostics.size())continue;
        const auto frame=cpu_frames_[glyph.frame];
        if(!FrameCauseHighlights(native_.store->frame_diagnostics[glyph.frame],frame.second-frame.first,1./frame_target_fps_,domain))continue;
        const auto cause=domain==FrameDomain::Game?FrameCause::Game:domain==FrameDomain::Render?FrameCause::Render:FrameCause::Gpu;
        draw->AddRectFilled(ImVec2(x+glyph.left,y),ImVec2(x+glyph.right,y+height),CauseColour(cause,48));
    }
}
void Application::Frames() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    if (!BeginView("Frames", &show_frames_,ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();ImGui::PopStyleVar();return;
    }
    const auto p=ImGui::GetCursorScreenPos();const float scale=UiScale();
    const float w=std::max(1.f,ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ScrollbarSize),
                h=std::max(35.f*scale,ImGui::GetContentRegionAvail().y);
    auto* d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,ImVec2(p.x+w,p.y+h),cyui::palette().bg0);
    navigation_x_=p.x;navigation_y_=p.y;navigation_width_=w;navigation_height_=h;
    frames_start_=view_start_;frames_end_=view_end_;
    ImGui::InvisibleButton("frame_navigation",ImVec2(w,h));
    const bool hovered=ImGui::IsItemHovered();const auto& io=ImGui::GetIO();
    if(hovered&&ImGui::IsMouseClicked(0))BeginTimingDrag(DragSurface::Frames,0,w);
    if(hovered&&io.MouseWheel) {
        auto next=NavigateTimingWindow(view_start_,view_end_,timing_model_.start,timing_model_.end,0,
            -io.MouseWheel*.22314355,std::clamp(double(io.MousePos.x-p.x)/w,0.,1.));
        view_start_=next.first;view_end_=next.second;
        frames_start_=view_start_;frames_end_=view_end_;
    }
    if(cpu_frames_.empty()) {
        d->AddText(ImVec2(p.x+7,p.y+7),cyui::palette().textDim,"CPU frame durations unavailable");
        ImGui::End();ImGui::PopStyleVar();return;
    }
    FrameGlyphs(w);
    const float baseline=p.y+h-4*scale,graphH=std::max(1.f,h-38*scale);
    for(auto pair:{std::pair<double,const char*>{.0166667,"16.7ms / 60fps"},{.0333333,"33.3ms / 30fps"},
                   {.05,"50ms / 20fps"},{.0666667,"66.7ms / 15fps"}}) {
        float y=baseline-graphH*static_cast<float>(pair.first/.075);
        d->AddLine(ImVec2(p.x,y),ImVec2(p.x+w,y),cyui::palette().gridStrong);
    }
    for(const auto& glyph:frame_glyphs_) {
        const size_t i=glyph.frame;const auto f=cpu_frames_[i];const double duration=f.second-f.first;
        float x=p.x+glyph.left,x2=p.x+glyph.right,y=baseline-graphH*static_cast<float>(std::min(.075,duration)/.075);
        const auto cause=Cause(i);
        const bool attributed=cause==FrameCause::Game||cause==FrameCause::Render||cause==FrameCause::Gpu||cause==FrameCause::Mixed;
        auto colour=show_frame_causes_&&attributed?CauseColour(cause):cyui::palette().textDim;
        d->AddRectFilled(ImVec2(x,y),ImVec2(x2,baseline),colour);
        if(glyph.count==1) {
            char label[64];std::snprintf(label,sizeof(label),timing_model_.frames_native?"Frame %llu":"Tick %llu",
                static_cast<unsigned long long>(timing_model_.frames_native?timing_model_.frame_ids[i]:i));
            if(x2-x>ImGui::CalcTextSize(label).x+8*scale)d->AddText(ImVec2(x+3*scale,p.y+18*scale),cyui::palette().textDim,label);
        }
        if(hovered&&ImGui::IsMouseHoveringRect(ImVec2(x,p.y),ImVec2(x2,baseline))) {
            ImGui::BeginTooltip();
            if(glyph.count>1)ImGui::Text("%zu frames in this pixel; duration/diagnosis of tallest frame only (derived display).",glyph.count);
            if(timing_model_.frames_native)ImGui::Text("Native game frame %llu",static_cast<unsigned long long>(timing_model_.frame_ids[i]));
            else ImGui::Text("Tick candidate %zu (derived frame identity)",i);
            ImGui::Text("%.3f ms measured duration | target %.3f ms (%.0f fps)",duration*1000,1000./frame_target_fps_,frame_target_fps_);
            if(native_.store&&i<native_.store->frame_diagnostics.size()) {
                ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(CauseColour(Cause(i))),"%s (derived)",FrameCauseName(Cause(i)));
                ImGui::Separator();
                for(size_t domain=0;domain<3;++domain) {
                    auto work=native_.store->frame_diagnostics[i].work[domain];
                    const char* name=domain==0?"GameThread":domain==1?"RenderThread":"GPU";
                    if(work.available&&domain==2)ImGui::Text("GPU: %.3f ms observed scopes | wait breakdown unavailable%s",work.total*1000,work.incomplete?" | incomplete":"");
                    else if(work.available)ImGui::Text("%s: %.3f ms total | %.3f ms waits | %.3f ms excluding waits%s",name,
                        work.total*1000,work.wait*1000,work.active*1000,work.incomplete?" | incomplete":"");
                    else ImGui::TextDisabled("%s: unavailable in this interval%s",name,work.incomplete?" / unfinished scopes":"");
                }
                ImGui::Separator();
                ImGui::TextUnformatted("Clipped union of observed scopes; nested scopes counted once.");
                ImGui::TextUnformatted("Wait names recognized heuristically; uninstrumented waits may remain.");
                ImGui::TextUnformatted("Render/GPU overlap this game interval; pipelined work may belong to another frame.");
                ImGui::TextUnformatted("Missing GPU scopes/Meta counters do not establish GPU cost or exact causality.");
            }
            ImGui::EndTooltip();
            if(ImGui::IsMouseReleased(0)&&drag_released_surface_==DragSurface::Frames&&!drag_moved_&&std::abs(io.MousePos.x-overview_drag_x_)<3*scale&&std::abs(io.MousePos.y-overview_drag_y_)<3*scale) {
                range_start_=f.first;range_end_=f.second;
            }
        }
    }
    if(show_frame_causes_&&native_.store) {
        char legend[180];std::snprintf(legend,sizeof(legend),"Probable cause | %.0f fps |",frame_target_fps_);
        float x=p.x+5*scale;d->AddRectFilled(p,ImVec2(p.x+w,p.y+16*scale),cyui::alpha(cyui::palette().bg1,0.961f));
        d->AddText(ImVec2(x,p.y+scale),cyui::palette().textDim,legend);x+=ImGui::CalcTextSize(legend).x+8*scale;
        for(auto item:{std::pair<FrameCause,const char*>{FrameCause::Game,"GameThread"},{FrameCause::Render,"RenderThread"},
            {FrameCause::Gpu,"GPU"},{FrameCause::Mixed,"Mixed"},{FrameCause::Unknown,"No attributed cause = neutral"}}) {
            auto length=ImGui::CalcTextSize(item.second).x;if(x+length+10*scale>p.x+w)break;
            d->AddRectFilled(ImVec2(x,p.y+4*scale),ImVec2(x+5*scale,p.y+10*scale),CauseColour(item.first));
            d->AddText(ImVec2(x+9*scale,p.y+scale),cyui::palette().textDim,item.second);x+=length+21*scale;
        }
    }
    // Paint scale labels above the bars so a slow frame cannot obscure them.
    for(auto pair:{std::pair<double,const char*>{.0166667,"16.7ms / 60fps"},{.0333333,"33.3ms / 30fps"},
                   {.05,"50ms / 20fps"},{.0666667,"66.7ms / 15fps"}}) {
        float y=baseline-graphH*static_cast<float>(pair.first/.075);
        auto size=ImGui::CalcTextSize(pair.second);const float x=p.x+w-size.x-5*scale;
        d->AddRectFilled(ImVec2(x-2*scale,y-7*scale),ImVec2(p.x+w,y+7*scale),cyui::alpha(cyui::palette().bg2,0.824f));
        d->AddText(ImVec2(x,y-6*scale),cyui::palette().textDim,pair.second);
    }
    ImGui::End();ImGui::PopStyleVar();
}
void Application::Timing() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, cyui::vec(cyui::palette().bg0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (!BeginView("Timing", &show_timing_)) {
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }
    float scale = UiScale(), header_height = 20 * scale, row_height = 15 * scale, scope_height = 14 * scale,
          ruler_height = 25 * scale;
    const bool keyboard=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&!ImGui::GetIO().WantTextInput&&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
    auto fitSelection=[&]{if(range_start_==range_end_)FitTiming();else{double a=std::min(range_start_,range_end_),b=std::max(range_start_,range_end_),padding=std::max(.000001,(b-a)*.05);view_start_=a-padding;view_end_=b+padding;}};
    if(keyboard&&!ImGui::GetIO().KeyCtrl&&!ImGui::GetIO().KeyAlt){
        if(ImGui::IsKeyPressed(ImGuiKey_F))fitSelection();
        if(ImGui::IsKeyPressed(ImGuiKey_Home))FitCapture();
        if(ImGui::IsKeyPressed(ImGuiKey_Escape)){range_start_=range_end_=0;if(drag_surface_==DragSurface::Measurement)drag_surface_=DragSurface::None;}
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))StepFrame(-1);
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))StepFrame(1);
    }
    ImGui::SetCursorPosX(5 * scale);
    if (ImGui::BeginCombo("##track_filter",
                          track_filter_ == 0   ? "All Tracks"
                          : track_filter_ == 1 ? "CPU Tracks"
                                               : "GPU Tracks",
                          ImGuiComboFlags_WidthFitPreview)) {
        for (int i = 0; i < 3; ++i)
            if (ImGui::Selectable(i == 0   ? "All Tracks"
                                  : i == 1 ? "CPU Tracks"
                                           : "GPU Tracks",
                                  i == track_filter_))
                track_filter_ = i;
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::list,"Choose visible CPU / GPU tracks"))
        ImGui::OpenPopup("track_visibility");
    if (ImGui::BeginPopup("track_visibility")) {
        for (auto &track : timing_model_.tracks)
            ImGui::MenuItem(track.name.c_str(), nullptr, &track.visible);
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::settings,"Timeline options: depth, frame guides and cause colors"))
        ImGui::OpenPopup("timing_view_mode");
    if (ImGui::BeginPopup("timing_view_mode")) {
        bool all_depths = depth_levels_ == 0;
        if (ImGui::Checkbox("All recorded depth levels", &all_depths))
            depth_levels_ = all_depths ? 0 : 24;
        if (!all_depths) {
            int maximum = 1;
            for (const auto &track : timing_model_.tracks)
                maximum = std::max(maximum, track.RowCount(0));
            ImGui::SliderInt("Depth limit", &depth_levels_, 1, maximum);
        }
        ImGui::Checkbox(timing_model_.frames_native?"Native game frame guides":"Engine tick guides (derived)", &show_tick_guides_);
        ImGui::Checkbox("Hide inactive tracks", &hide_inactive_tracks_);
        ImGui::Checkbox("Frame cause background (derived)",&show_frame_causes_);
        ImGui::SetNextItemWidth(130*scale);
        ImGui::InputFloat("Target fps",&frame_target_fps_,0,0,"%.0f");
        if(!std::isfinite(frame_target_fps_)||frame_target_fps_<1)frame_target_fps_=60;
        frame_target_fps_=std::min(frame_target_fps_,1000.f);
        ImGui::TextWrapped("Colors mark over-budget intervals. Hover Frames for observed total, recognized waits and work excluding waits. Render/GPU association uses time overlap, not exact frame identity.");
        if (ImGui::MenuItem("Collapse all tracks"))
            for (auto &track : timing_model_.tracks)
                track.collapsed = true;
        if (ImGui::MenuItem("Expand all tracks"))
            for (auto &track : timing_model_.tracks)
                track.collapsed = false;
        if (ImGui::MenuItem("Fit capture")) {
            FitCapture();
            range_start_ = range_end_ = 0;
        }
        if (ImGui::MenuItem("Focus three frames"))
            FitTiming();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::fullScreen,"Focus three frames (Home fits the entire capture)"))
        FitTiming();
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Focus three frames. F fits the selection, Home fits the capture, Left/Right step frames, Escape clears the time selection.");
    ImGui::SameLine();ImGui::BeginDisabled(range_start_==range_end_);if(cyui::iconButton(cyui::icon::ruler,"Fit selected interval (F)"))fitSelection();ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(cpu_frames_.empty());
    if (cyui::iconButton(cyui::icon::previous,"Previous frame (Left arrow)"))
        StepFrame(-1);
    ImGui::SameLine();
    if (cyui::iconButton(cyui::icon::next,"Next frame (Right arrow)"))
        StepFrame(1);
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(175 * scale);
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F))ImGui::SetKeyboardFocusHere();
    if(cyui::searchField("##timing_search", search_, sizeof(search_), "Find a timing event (Ctrl+F)",cyui::dp(230)))timing_exact_name_.clear();
    ImGui::SameLine();
    if(cyui::iconTextButton(cyui::icon::warning,"Next slow frame",cyui::Variant::ghost))FocusSlowFrame();
    if(!timing_exact_name_.empty()){
        if(cyui::iconButton(cyui::icon::close,"Clear exact function filter (double-click an empty timeline area)"))FilterFunction({});
        ImGui::SameLine();cyui::badge("Exact function",cyui::Tone::accent);ImGui::SameLine();
        ImGui::TextWrapped("%s",timing_exact_name_.c_str());
    }
    if (timing_model_.timers.size()!=native_.events.size()||(native_.events.empty() && counters_->series.empty() && !activity_store_)) {
        ImGui::Spacing();
        ImGui::TextWrapped("  %s", AnalysisBusy() ? message_.c_str()
                                                  : load_error_.empty()?"Open a capture to display native CPU/GPU tracks.":load_error_.c_str());
        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        return;
    }
    const double span = std::max(.000001, view_end_ - view_start_);
    std::string filter_key=search_;filter_key+='\0';filter_key+=timing_exact_name_;
    const bool filtering=search_[0]||!timing_exact_name_.empty();
    if (timing_search_version_!=model_version_||timing_search_query_ != filter_key || (filtering&&timing_search_matches_.size() != native_.events.size())) {
        timing_search_version_=model_version_;
        timing_search_query_ = filter_key;
        if(!filtering)timing_search_matches_.clear();
        else {
            if(timing_filter_key_!=filter_key){timing_filter_key_=filter_key;timing_name_matches_.clear();}
            timing_timer_matches_.resize(timing_model_.names.size());
            for(size_t i=0;i<timing_timer_matches_.size();++i){const auto& name=timing_model_.names[i];auto cached=timing_name_matches_.try_emplace(name,uint8_t{0});
                if(cached.second)cached.first->second=TimingMatches(name,search_)&&(timing_exact_name_.empty()||name==timing_exact_name_)?1:0;timing_timer_matches_[i]=cached.first->second;}
            timing_search_matches_.resize(native_.events.size());
            for(size_t i=0;i<timing_search_matches_.size();++i)timing_search_matches_[i]=timing_timer_matches_[timing_model_.timers[i]]||(!timing_exact_name_.empty()&&loaded_exact_name_==timing_exact_name_&&native_.events[i].display_aggregate&&native_.events[i].exact_name_match&&search_[0]==0)?1:0;
        }
    }
    const float w = std::max(1.f, ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ScrollbarSize);
    FrameGlyphs(w);
    auto *d = ImGui::GetWindowDrawList();
    auto p = ImGui::GetCursorScreenPos();
    float time_ruler_height = ruler_height;
    if (show_tick_guides_)
        ruler_height += 16 * scale;
    std::vector<std::pair<size_t, float>> guides;
    if (show_tick_guides_) {
        // Match the pixel-bounded Frames overview; avoid one guide per source
        // frame when hours of capture are zoomed out into a few pixels.
        for(const auto& glyph:frame_glyphs_) {
            auto i=glyph.frame;
            if(cpu_frames_[i].first>=view_start_)guides.emplace_back(i,glyph.left);
        }
    }
    d->AddRectFilled(p, ImVec2(p.x + w, p.y + ruler_height), cyui::palette().bg2);
    ImGui::InvisibleButton("time_ruler", ImVec2(w, ruler_height));
    double power = std::pow(10., std::floor(std::log10(span / 8))),
           step = power * std::ceil(span / (8 * power));
    for (double t = std::ceil(view_start_ / step) * step; t < view_end_; t += step) {
        float x = p.x + w * static_cast<float>((t - view_start_) / span);
        auto label = TimingLabel(t);
        d->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + time_ruler_height), cyui::palette().gridStrong);
        d->AddText(ImVec2(x + 3 * scale, p.y + 9 * scale), cyui::palette().textDim, label.c_str());
    }
    float previous_label = -10000;
    for (const auto &guide : guides) {
        float x = p.x + guide.second;
        d->AddLine(ImVec2(x, p.y + time_ruler_height), ImVec2(x, p.y + ruler_height),
                   cyui::palette().gridStrong);
        if (x >= previous_label) {
            char label[48];
            std::snprintf(label,sizeof(label),timing_model_.frames_native?"Frame %llu":"Tick %llu",
                static_cast<unsigned long long>(timing_model_.frames_native?timing_model_.frame_ids[guide.first]:guide.first));
            d->AddText(ImVec2(x + 2 * scale, p.y + time_ruler_height), cyui::palette().textDim, label);
            previous_label = x + ImGui::CalcTextSize(label).x + 8 * scale;
        }
        if (ImGui::IsItemHovered() &&
            ImGui::IsMouseHoveringRect(ImVec2(x - 3 * scale, p.y + time_ruler_height),
                                       ImVec2(x + 3 * scale, p.y + ruler_height))) {
            if(timing_model_.frames_native)ImGui::SetTooltip("Native game frame %llu\nMeasured frame boundary; scopes on other tracks can cross this guide.",
                static_cast<unsigned long long>(timing_model_.frame_ids[guide.first]));
            else ImGui::SetTooltip("Engine tick candidate %zu\nDerived from GameThread engine scopes; native "
                              "frame identity unavailable.\nScopes on other tracks can cross this guide.",
                              guide.first);
        }
    }
    ruler_x_=p.x;ruler_y_=p.y;ruler_width_=w;
    if(ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(0)){search_[0]=0;FilterFunction({});}
    else if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
    {
        if(ImGui::GetIO().KeyShift)range_start_ = range_end_ = view_start_ + span * (ImGui::GetIO().MousePos.x - p.x) / w;
        else BeginTimingDrag(DragSurface::Ruler,0,w);
    }
    if(ImGui::IsItemHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Right))BeginTimingDrag(DragSurface::Ruler,ImGuiMouseButton_Right,w,false);
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)&&ImGui::GetIO().KeyShift)
        range_end_ = view_start_ + span * (ImGui::GetIO().MousePos.x - p.x) / w;
    if(ImGui::IsItemHovered()&&ImGui::GetIO().MouseWheel) {
        auto next=NavigateTimingWindow(view_start_,view_end_,timing_model_.start,timing_model_.end,0,-ImGui::GetIO().MouseWheel*.22314355,
            std::clamp(double(ImGui::GetIO().MousePos.x-p.x)/w,0.,1.));
        view_start_=next.first;view_end_=next.second;
    }
    MeasurementRuler(w);
    ImGui::BeginChild("native_tracks", ImVec2(0, 0), false,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    tracks_scroll_=ImGui::GetScrollY();tracks_scroll_max_=ImGui::GetScrollMaxY();
    if(ImGui::IsWindowHovered()&&ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        BeginTimingDrag(DragSurface::Tracks,ImGuiMouseButton_Right,w,false);
    if(drag_button_==ImGuiMouseButton_Right&&drag_moved_&&
       (drag_surface_==DragSurface::Tracks||drag_surface_==DragSurface::Ruler||
        drag_released_surface_==DragSurface::Tracks||drag_released_surface_==DragSurface::Ruler))
        ImGui::SetScrollY(drag_scroll_start_-(ImGui::GetIO().MousePos.y-overview_drag_y_));
    bool scope_hovered=false,blank_lane_double_click=false;
    const float counter_tracks_start=ImGui::GetCursorScreenPos().y;
    CounterTracks(w);
    const float counter_tracks_end=ImGui::GetCursorScreenPos().y;
    d = ImGui::GetWindowDrawList();
    auto clip = ImGui::GetCurrentWindow()->ClipRect;
    visible_rows_=recorded_rows_=0;
    tracks_y_=0;
    scope_probe_x_=scope_probe_y_=0;scope_probe_name_.clear();context_hist_x_=context_hist_y_=0;
    timing_blank_x_=timing_blank_y_=0;
    for (auto &track : timing_model_.tracks) {
        if (!track.visible || (track_filter_ == 1 && track.gpu) || (track_filter_ == 2 && !track.gpu))
            continue;
        const int query_rows=track.RowCount(depth_levels_);
        auto refresh = [&] {
            if (track.visible_levels != query_rows || track.visible_start != view_start_ ||
                track.visible_end != view_end_) {
                track.visible_events_valid=false;
                track.visible_start = view_start_;
                track.visible_end = view_end_;
                track.visible_levels = query_rows;
                track.visible_rows=track.VisibleRowCount(native_.events,view_start_,view_end_,depth_levels_);
                track.glyph_width = 0;
            }
        };
        refresh();
        const int rows=track.visible_rows;
        visible_rows_+=rows;recorded_rows_+=track.RowCount(0);
        if (hide_inactive_tracks_) {
            if (rows==0)
                continue;
        }
        auto pos = ImGui::GetCursorScreenPos();
        float height = track.collapsed||rows==0 ? header_height : header_height + row_height * rows + 5 * scale;
        ImGui::PushID(track.name.c_str());
        ImGui::InvisibleButton("lane", ImVec2(w, height));
        if (track.focus_depth >= 0) {
            ImGui::SetScrollFromPosY(
                pos.y - ImGui::GetWindowPos().y + header_height + track.focus_depth * row_height, .3f);
            track.focus_depth = -1;
        }
        bool laneHovered = ImGui::IsItemHovered();
        if(laneHovered&&ImGui::GetIO().MousePos.y>=pos.y+header_height) {
            // InvisibleButton owns the active mouse item on the second click,
            // so IsWindowHovered() alone cannot recognize empty lane space.
            blank_lane_double_click|=ImGui::IsMouseDoubleClicked(0);
            if(ImGui::IsMouseClicked(0)&&!ImGui::IsMouseDoubleClicked(0))BeginTimingDrag(DragSurface::Tracks,0,w,false);
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))BeginTimingDrag(DragSurface::Tracks,ImGuiMouseButton_Middle,w,false);
        }
        ImGui::PopID();
        if (pos.y + height < clip.Min.y || pos.y > clip.Max.y)
            continue;
        if(timing_blank_y_==0&&!track.collapsed&&rows>0&&pos.y+height<=clip.Max.y){
            // Use lane padding, which survives integer mouse coordinates at any DPI.
            timing_blank_x_=pos.x+w*.5f;timing_blank_y_=pos.y+height-2*scale;
        }
        d->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + height), cyui::palette().bg0);
        const auto domain=DiagnosticDomain(track.events.empty()?track.name:native_.events[track.events.front()].thread);
        FrameBackground(pos.x,pos.y+header_height,height-header_height,domain);
        if(tracks_y_==0&&rows>0&&!track.collapsed) {tracks_x_=pos.x;tracks_y_=pos.y+header_height+scope_height*.5f;}
        d->AddLine(ImVec2(pos.x, pos.y + height - 1), ImVec2(pos.x + w, pos.y + height - 1),
                   cyui::palette().lineStrong);
        for (double t = std::ceil(view_start_ / step) * step; t < view_end_; t += step) {
            float x = pos.x + w * static_cast<float>((t - view_start_) / span);
            d->AddLine(ImVec2(x, pos.y + header_height), ImVec2(x, pos.y + height),
                       cyui::palette().grid);
        }
        for (const auto &guide : guides)
            d->AddLine(ImVec2(pos.x + guide.second, pos.y + header_height),
                       ImVec2(pos.x + guide.second, pos.y + height), cyui::alpha(cyui::palette().gridStrong,0.706f));
        d->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + header_height - scale), cyui::palette().bg2);
        auto header = std::string(track.collapsed ? "> " : "v ") + track.name;
        d->AddText(ImVec2(pos.x + 4 * scale, pos.y + 2 * scale), cyui::palette().text,
                   header.c_str());
        if (laneHovered && ImGui::GetIO().MousePos.y < pos.y + header_height - scale &&
            ImGui::IsMouseClicked(0))
            track.collapsed = !track.collapsed;
        if (track.collapsed||rows==0)
            continue;
        // Only tracks intersecting the vertical viewport need event lists.
        // Their row count above remains dynamic while scrolling or panning.
        if(!track.visible_events_valid){track.visible_events.clear();track.Query(native_.events,view_start_,view_end_,query_rows,track.visible_events);track.visible_events_valid=true;
            track.gpu_detail=track.gpu?InspectGpuTimingDetail(native_.events,track.visible_events):GpuTimingDetail{};
            if(track.gpu&&!track.gpu_detail.Limited()&&track.gpu_detail_source.Limited())track.gpu_detail=track.gpu_detail_source;}
        if(track.gpu_detail.Limited()) {
            const char* warning="GPU group costs unresolved";
            const float warning_x=pos.x+w-ImGui::CalcTextSize(warning).x-8*scale;
            d->AddText(ImVec2(warning_x,pos.y+2*scale),cyui::palette().warning,warning);
            if(ImGui::IsMouseHoveringRect(ImVec2(warning_x,pos.y),ImVec2(pos.x+w,pos.y+header_height))) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("Derived diagnostic in the loaded timing interval");
                ImGui::Text("%zu / %zu nested intervals are <= 2 us; longest outer interval: %.3f ms.",track.gpu_detail.near_zero,track.gpu_detail.nested,track.gpu_detail.longest_outer*1000.);
                ImGui::TextUnformatted("Near-zero timestamp differences do not prove negligible GPU cost.");
                ImGui::TextUnformatted("Tile render passes can prevent meaningful per-group timing. Recorded values are preserved.");
                ImGui::TextUnformatted("The updated Quest plugin can add measured Vulkan render-pass boundaries in a new capture. Shared tile subpass costs remain unresolved.");
                ImGui::EndTooltip();
            }
        }
        if (track.glyph_width != w || track.glyph_selected != selected_cpu_ ||
            track.glyph_search != filter_key) {
            BuildTimingGlyphs(native_.events, track.visible_events, view_start_, view_end_, w, selected_cpu_,
                              track.glyphs, filtering ? &timing_search_matches_ : nullptr);
            track.glyph_width = w;
            track.glyph_selected = selected_cpu_;
            track.glyph_search = filter_key;
        }
        if (track.glyphs.empty())
            d->AddText(ImVec2(pos.x + 5 * scale, pos.y + header_height + 2 * scale),
                       cyui::palette().textFaint, "No timing scopes in this interval");
        if (depth_levels_ > 0 && query_rows < track.RowCount(0)) {
            char label[64];
            std::snprintf(label, sizeof(label), "%d depth levels hidden", track.RowCount(0) - query_rows);
            float label_x = pos.x + w - ImGui::CalcTextSize(label).x - 5 * scale;
            d->AddText(ImVec2(label_x, pos.y + 2 * scale), cyui::palette().textDim, label);
        }
        for (const auto &glyph : track.glyphs) {
            auto i = glyph.event;
            const auto &e = native_.events[i];
            float x = pos.x + glyph.left, x2 = pos.x + glyph.right;
            float y = pos.y + header_height + e.depth * row_height;
            if(y+scope_height<clip.Min.y||y>clip.Max.y)continue;
            if(scope_probe_name_.empty()&&glyph.count==1&&!e.display_aggregate&&e.frame_type<0&&e.start>=view_start_&&e.end<=view_end_&&x2-x>30*scale&&y>=clip.Min.y&&y+scope_height<=clip.Max.y){
                scope_probe_x_=(x+x2)*.5f;scope_probe_y_=y+scope_height*.5f;scope_probe_name_=e.name;
            }
            auto colour = timer_colours_[timing_model_.timers[i]];
            if (filtering && !glyph.matches_search)
                colour = (colour & 0x00ffffffu) | 0x55000000u;
            d->AddRectFilled(ImVec2(x, y), ImVec2(std::max(x + 1, x2), y + scope_height), colour);
            if (glyph.selected)
                d->AddRect(ImVec2(x, y), ImVec2(std::max(x + 1, x2), y + scope_height),
                           cyui::palette().accent, 0.0f, 2.0f * scale, ImDrawFlags_None);
            if (glyph.count == 1 && x2 - x > 16 * scale) {
                const char* label = e.name.c_str();std::string duration_label;
                if (x2 - x > 200 * scale && !e.open_ended) {
                    char duration[48];
                    std::snprintf(duration, sizeof(duration), " (%.2f ms)", (e.end - e.start) * 1000);
                    duration_label=e.name+duration;label=duration_label.c_str();
                }
                // Clip glyph geometry on the CPU, retaining the shared window
                // scissor. A separate GPU scissor for every label would split
                // the timeline into hundreds of draw submissions.
                const ImVec4 text_clip(x,y,x2,y+scope_height);
                d->AddText(ImGui::GetFont(),ImGui::GetFontSize(),ImVec2(x+2*scale,y-scale),
                           ScopeTextColour(colour),label,nullptr,0,&text_clip);
            }
            if (laneHovered &&
                ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(std::max(x + 1, x2), y + scope_height))) {
                scope_hovered=true;
                if (glyph.count > 1)
                    ImGui::SetTooltip(
                        "Aggregated display (derived): %zu native scopes in this pixel column\nDepth: %d\nExecution range: %s "
                        "to %s\nZoom in or double-click to separate the scopes.%s",
                        glyph.count, glyph.depth, TimingLabel(glyph.first_start).c_str(),
                        TimingLabel(glyph.last_end).c_str(),
                        glyph.has_unfinished ? "\nIncludes unfinished scopes; their duration is unavailable."
                                             : "");
                else if (e.open_ended)
                    ImGui::SetTooltip("%s\nUnfinished scope: duration unavailable", e.name.c_str());
                else
                    ImGui::SetTooltip("%s\n%s\nStart: %s / End: %s\nInclusive: %.3f ms (%s)\nExclusive "
                                      "in interval: %.3f ms "
                                      "(derived from nesting)%s%s%s",
                                      e.name.c_str(), e.thread.c_str(), TimingLabel(e.start).c_str(),
                                      TimingLabel(e.end).c_str(), (e.end - e.start) * 1000,
                                      track.gpu?"recorded timestamp interval":"measured",
                                      timing_model_.exclusive[i] * 1000,
                                      glyph.continues_left ? "\nStarts before the displayed interval." : "",
                                      glyph.continues_right ? "\nEnds after the displayed interval." : "",
                                      track.name.rfind("GPU / Meta ",0)==0?"\nMeasured hardware stage; aligned through Perfetto clock snapshots and native Sync. Unreal pass and execution-frame identity unavailable.":track.name=="GPU / Vulkan render passes (approximate alignment)"?"\nPass boundary interval measured on GPU; CPU alignment is approximate (device clock sample or engine clock relation). Not per-subpass rasterization timing.":track.gpu&&track.gpu_detail.Limited()?"\nGPU group timing is limited in this interval; a near-zero subgroup does not establish negligible GPU cost.":"");
                if (glyph.count == 1 && ImGui::IsMouseClicked(0)) {
                    selected_cpu_ = static_cast<int>(i);
                    if(e.thread=="GPU / Vulkan render passes (approximate alignment)"){show_details_=true;ImGui::SetWindowFocus(WindowName("Selection").c_str());}
                    selected_event_ = -1;
                    selected_timer_ = static_cast<int>(timing_model_.timers[i]);
                }
                if(glyph.count==1&&!e.display_aggregate&&e.frame_type<0&&ImGui::IsMouseClicked(ImGuiMouseButton_Right)){
                    drag_context_name_=e.name;
                }
                if(glyph.count==1&&!e.display_aggregate&&e.frame_type<0&&ImGui::IsMouseDoubleClicked(0))FilterFunction(e.name);
                else if (ImGui::IsMouseDoubleClicked(0)) {
                    double a = glyph.first_start, b = glyph.last_end,
                           padding = std::max(.000001, (b - a) * .03);
                    view_start_ = a - padding;
                    view_end_ = b + padding;
                    range_start_ = range_end_ = 0;
                }
            }
        }
        if (range_start_ != range_end_) {
            float a = pos.x +
                      w * static_cast<float>((std::min(range_start_, range_end_) - view_start_) / span),
                  b = pos.x +
                      w * static_cast<float>((std::max(range_start_, range_end_) - view_start_) / span);
            d->AddRectFilled(ImVec2(a, pos.y + header_height), ImVec2(b, pos.y + height),
                             cyui::palette().selection);
            d->AddLine(ImVec2(a, pos.y + header_height), ImVec2(a, pos.y + height),
                       cyui::palette().accent);
            d->AddLine(ImVec2(b, pos.y + header_height), ImVec2(b, pos.y + height),
                       cyui::palette().accent);
        }
    }
    // A right click opens the menu only after release. Held input remains a pan,
    // including when asynchronous reads replace the scopes under the cursor.
    if(drag_released_surface_==DragSurface::Tracks&&drag_button_==ImGuiMouseButton_Right&&!drag_moved_&&!drag_context_name_.empty()){
        timing_scope_context_=drag_context_name_;drag_context_name_.clear();ImGui::OpenPopup("timing_scope_actions");
    }
    if(ImGui::BeginPopup("timing_scope_actions")){
        ImGui::TextWrapped("%s",timing_scope_context_.c_str());ImGui::Separator();
        if(ImGui::MenuItem("Filter all occurrences by exact name"))FilterFunction(timing_scope_context_);
        if(ImGui::MenuItem("Histogram of this function"))OpenFunctionHistogram(timing_scope_context_);
        {auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();context_hist_x_=(a.x+b.x)*.5f;context_hist_y_=(a.y+b.y)*.5f;}
        if(ImGui::MenuItem("Clear function filter",nullptr,false,!timing_exact_name_.empty()))FilterFunction({});
        ImGui::EndPopup();
    }
    const float other_tracks_start=ImGui::GetCursorScreenPos().y;
    ActivityTracks(w);
    const float mouse_y=ImGui::GetIO().MousePos.y;
    const bool other_track_hovered=(mouse_y>=other_tracks_start&&mouse_y<ImGui::GetCursorScreenPos().y)||(mouse_y>=counter_tracks_start&&mouse_y<counter_tracks_end);
    if((blank_lane_double_click||ImGui::IsWindowHovered())&&!scope_hovered&&!other_track_hovered&&ImGui::IsMouseDoubleClicked(0)){search_[0]=0;FilterFunction({});}
    if (ImGui::IsWindowHovered()) {
        auto &io = ImGui::GetIO();
        if (io.MouseWheel && io.KeyShift)
            ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseWheel * row_height * 5);
        else if (io.MouseWheel) {
            double anchor =
                       std::clamp(static_cast<double>((io.MousePos.x - ImGui::GetWindowPos().x) / w), 0., 1.),
                   time = view_start_ + span * anchor,
                   zoomed = std::clamp(span * std::pow(.8, io.MouseWheel), .000001,
                                       std::max(.000001, std::max(timing_model_.end,counters_->end)-std::min(timing_model_.start,counters_->start)));
            view_start_ = time - zoomed * anchor;
            view_end_ = view_start_ + zoomed;
        }
        if(drag_surface_==DragSurface::None&&ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
            BeginTimingDrag(DragSurface::Tracks,ImGuiMouseButton_Middle,w,false);
    }
    // Keyboard navigation is handled once by Timing above, including focus,
    // popup and text-input guards. Repeating it here used to step two frames.
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
void Application::TimerTable(const char *id, const std::vector<TimerTotal> &rows, bool selectable) {
    auto& cache=timer_tables_[id];auto& filtered=cache.filtered;
    const bool exact_filter=std::string(id)=="timers";
    std::string query=search_;query+='\0';if(exact_filter)query+=timing_exact_name_;
    const bool changed=cache.model_version!=model_version_||cache.statistics_version!=statistics_version_||
        cache.selected_timer!=statistics_timer_||cache.query!=query;
    if(changed) {
        filtered.clear();
        for(size_t i=0;i<rows.size();++i){const auto& name=timing_model_.names[rows[i].timer];if(TimingMatches(name,search_)&&(!exact_filter||timing_exact_name_.empty()||name==timing_exact_name_))filtered.push_back(i);}
        cache.model_version=model_version_;cache.statistics_version=statistics_version_;
        cache.selected_timer=statistics_timer_;cache.query=query;
        cache.column=-1; // Sort again even if the table is temporarily clipped.
    }
    if (ImGui::BeginTable(id, 4,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                              ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 2.4f, 0);
        ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_None, 0, 1);
        ImGui::TableSetupColumn(
            "Incl ms", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending, 0, 2);
        ImGui::TableSetupColumn("Excl ms", ImGuiTableColumnFlags_None, 0, 3);
        ImGui::TableHeadersRow();
        if (auto *sorting = ImGui::TableGetSortSpecs(); sorting && sorting->SpecsCount) {
            auto spec = sorting->Specs[0];
            if(changed||sorting->SpecsDirty||cache.column!=int(spec.ColumnUserID)||cache.direction!=int(spec.SortDirection))
                std::stable_sort(filtered.begin(), filtered.end(), [&](size_t a, size_t b) {
                const auto &x = rows[a];
                const auto &y = rows[b];
                int order = 0;
                if (spec.ColumnUserID == 0)
                    order = timing_model_.names[x.timer].compare(timing_model_.names[y.timer]);
                else if (spec.ColumnUserID == 1)
                    order = x.count < y.count ? -1 : x.count > y.count ? 1 : 0;
                else {
                    double vx = spec.ColumnUserID == 2 ? x.inclusive : x.exclusive,
                           vy = spec.ColumnUserID == 2 ? y.inclusive : y.exclusive;
                    order = vx < vy ? -1 : vx > vy ? 1 : 0;
                }
                if (order == 0)
                    return x.timer < y.timer;
                return spec.SortDirection == ImGuiSortDirection_Descending ? order > 0 : order < 0;
            });
            cache.column=int(spec.ColumnUserID);cache.direction=int(spec.SortDirection);
            sorting->SpecsDirty = false;
        }
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(filtered.size()));
        while (clip.Step())
            for (int r = clip.DisplayStart; r < clip.DisplayEnd; ++r) {
                const auto &row = rows[filtered[r]];
                auto &name = timing_model_.names[row.timer];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(row.timer));
                cyui::colourChip("##timer_colour",timer_colours_[row.timer],cyui::dp(4,12));
                ImGui::SameLine(0,cyui::dp(4));
                if (ImGui::Selectable(name.c_str(), selected_timer_ == static_cast<int>(row.timer),
                                      ImGuiSelectableFlags_SpanAllColumns) &&
                    selectable)
                    selected_timer_ = static_cast<int>(row.timer);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                    FocusTimer(row.timer);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\nInclusive sum in interval; nested scopes overlap.\nExclusive is "
                                      "derived from contained immediate children.",
                                      name.c_str());
                if (ImGui::BeginPopupContextItem("timer_actions")) {
                    if (timing_model_.timer_has_id[row.timer])
                        ImGui::Text("Native TimerId: %llu",
                                    static_cast<unsigned long long>(timing_model_.timer_ids[row.timer]));
                    else
                        ImGui::TextDisabled("Native TimerId unavailable / grouped by name");
                    if (ImGui::MenuItem("Find in timing view"))
                        FocusTimer(row.timer);
                    if(ImGui::MenuItem("Filter all occurrences by exact name"))FilterFunction(name);
                    if(ImGui::MenuItem("Histogram of this function"))OpenFunctionHistogram(name);
                    ImGui::EndPopup();
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::Text("%zu", row.count);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", row.inclusive * 1000);
                ImGui::TableNextColumn();
                if(std::isfinite(row.exclusive))ImGui::Text("%.3f", row.exclusive * 1000);
                else ImGui::TextDisabled("N/A");
            }
        ImGui::EndTable();
    }
}
void Application::Statistics() {
    if (!BeginView("Timers", &show_timers_)) {
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(-1);
    cyui::searchField("##timer_search", search_, sizeof(search_),"Search timers",-1);
    ImGui::TextDisabled("Flat / selected interval");
    if(native_.store&&(statistics_start_!=(range_start_==range_end_?view_start_:std::min(range_start_,range_end_))||
                      statistics_end_!=(range_start_==range_end_?view_end_:std::max(range_start_,range_end_))))
        ImGui::TextDisabled("Updating interval statistics...");
    if(native_.store&&!native_.store->relations_valid)ImGui::TextWrapped("Exclusive time unavailable: source order cannot establish nesting across the entire capture.");
    if (native_.events.empty())
        ImGui::TextWrapped("Select a native capture to populate timers.");
    else
        TimerTable("timers", timing_model_.totals, true);
    ImGui::End();
}
void Application::Relations(bool callers) {
    bool &visible = callers ? show_callers_ : show_callees_;
    if (!BeginView(callers ? "Callers" : "Callees", &visible)) {
        ImGui::End();
        return;
    }
    if (selected_timer_ < 0)
        ImGui::TextWrapped("Select a timer or scope to inspect its immediate %s.",
                           callers ? "callers" : "callees");
    else {
        ImGui::TextWrapped("%s", timing_model_.names[static_cast<size_t>(selected_timer_)].c_str());
        if(window_aggregated_)ImGui::TextWrapped("Call relations unavailable in this aggregated overview. Zoom in to individual native scopes.");
        ImGui::TextDisabled("Derived from depth and containment");
        TimerTable(callers ? "callers" : "callees", callers ? callers_ : callees_, true);
    }
    ImGui::End();
}
void Application::LoadStatus() {
    if(native_setup_.Busy()){pending_shown_=true;return;}
    bool busy=native_import_pending_||!pending_capture_.empty()||native_.Busy()||model_task_.valid();
    if(!busy&&load_error_.empty())return;
    const auto* viewport=WorkspaceViewport();
    ImGui::SetNextWindowViewport(viewport->ID);
    // Keep the timeline usable while a replacement visible interval is queried.
    bool initial=!native_.store||!std::isfinite(loaded_start_);
    // Interval reads are reported in the toolbar. No appearing window may take
    // focus or mouse capture away from an ongoing navigation gesture.
    if(!initial&&load_error_.empty())return;
    ImGui::SetNextWindowPos(initial||!load_error_.empty()?ImVec2(viewport->WorkPos.x+viewport->WorkSize.x*.5f,viewport->WorkPos.y+viewport->WorkSize.y*.45f):ImVec2(viewport->WorkPos.x+12*UiScale(),viewport->WorkPos.y+viewport->WorkSize.y-125*UiScale()),ImGuiCond_Always,initial||!load_error_.empty()?ImVec2(.5f,.5f):ImVec2(0,0));
    ImGui::SetNextWindowSize(ImVec2(560*UiScale(),0),ImGuiCond_Always);
    BeginView(load_error_.empty()?"Loading capture":"Capture could not be opened",nullptr,
        ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoFocusOnAppearing);
    if(!load_error_.empty()) {
        cyui::heading("Capture could not be opened",2);ImGui::TextWrapped("%s",load_error_.c_str());
        ImGui::TextWrapped("Trace: %s",trace_.u8string().c_str());ImGui::TextWrapped("Analyzer: %s",exe_);
        ImGui::TextWrapped("Log: %s",native_.LogPath().u8string().c_str());
        if(ImGui::Button("Copy details")){auto details=load_error_+"\n"+trace_.u8string()+"\n"+native_.LogPath().u8string();ImGui::SetClipboardText(details.c_str());}
        ImGui::SameLine();if(ImGui::Button("Close"))load_error_.clear();
        if(ImGui::Button("Add Unreal engine root and retry")) {
            auto root=ChooseFolder();if(!root.empty())AddEngineRoot(root);
        }
        if(!engine_registration_error_.empty())ImGui::TextWrapped("%s",engine_registration_error_.c_str());
    } else {
        const char* phase=!pending_capture_.empty()?"Preparing capture":native_.Busy()?native_.Importing()?"Building the timing index on disk":native_.DirectTiming()?"Decoding native Unreal timings":"Analyzing Unreal trace / exporting timings":"Loading visible interval / calculating statistics";
        ImGui::TextWrapped("%s",phase);ImGui::Text("Elapsed: %.1f s",ImGui::GetTime()-load_started_);
        const auto& progress=native_.Progress();uint64_t total=progress.total.load(),bytes=progress.bytes.load();
        float fraction=native_.Importing()&&total?static_cast<float>(double(bytes)/double(total)):-static_cast<float>(ImGui::GetTime());
        cyui::progressBar(fraction,ImVec2(-1,cyui::dp(18)),native_.Importing()&&total?nullptr:"Working...",cyui::Tone::accent);
        if(native_.Importing())ImGui::Text("%llu events indexed | %.1f / %.1f MiB",static_cast<unsigned long long>(progress.events.load()),double(bytes)/(1024*1024),double(total)/(1024*1024));
        else if(native_.Busy())ImGui::Text("%s: %.1f MiB",native_.DirectTiming()?"Compact binary timings":"Intermediate CSV export",double(native_.ExportBytes())/(1024*1024));
        ImGui::TextWrapped("%s",pending_capture_.empty()?trace_.filename().u8string().c_str():pending_capture_.filename().u8string().c_str());
        if(ImGui::Button("Cancel")){pending_capture_.clear();native_.Cancel();model_cancel_.store(true);counter_reader_.Cancel();cancelled_load_=true;}
        pending_shown_=true;
    }
    ImGui::End();
}
void Application::Messages() {
    if (!BeginView("Messages", &show_messages_)) {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("Application messages / native Unreal log provider pending");
    if (has_diff_) {
        ImGui::Text("GPU comparison: draws %+d / dispatches %+d", diff_.draw_delta, diff_.dispatch_delta);
        if (comparison_timed_)
            ImGui::Text("GPU duration delta: %+.4f ms", diff_.gpu_delta_ms);
        else
            ImGui::TextDisabled("GPU duration delta unavailable");
    }
    for (auto i = messages_.rbegin(); i != messages_.rend(); ++i)
        ImGui::TextWrapped("%s", i->c_str());
    ImGui::End();
}
} // namespace cyinsights
