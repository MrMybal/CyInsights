// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "UiMetrics.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <imgui.h>
#include <imgui_internal.h>

namespace cyinsights {
namespace {
bool Match(std::string value,std::string query) {
    for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for(auto& c:query)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value.find(query)!=std::string::npos;
}
ImU32 Colour(size_t i) {
    return cyui::categorical(static_cast<int>(i));
}
int Compare(const CounterSeries& a,const CounterSample& x,const CounterSeries& b,const CounterSample& y) {
    if(!a.floating&&!b.floating)return x.integer<y.integer?-1:x.integer>y.integer?1:0;
    if(a.floating&&b.floating)return x.real<y.real?-1:x.real>y.real?1:0;
    // Preserve integer ordering beyond double precision, including mixed types.
    const int64_t integer=a.floating?y.integer:x.integer;
    const double real=a.floating?x.real:y.real;
    int order=0;
    if(real>=9223372036854775808.)order=-1;
    else if(real<-9223372036854775808.)order=1;
    else {
        const auto whole=static_cast<int64_t>(real);
        order=integer<whole?-1:integer>whole?1:real>static_cast<double>(whole)?-1:real<static_cast<double>(whole)?1:0;
    }
    return a.floating?-order:order;
}
}
void Application::FitCapture() {
    bool have=false;double a=0,b=1;
    if(native_.store||!native_.events.empty()){a=timing_model_.start;b=timing_model_.end;have=true;}
    if(std::any_of(counters_->series.begin(),counters_->series.end(),[](const CounterSeries& c){return !c.samples.empty();})) {
        a=have?std::min(a,counters_->start):counters_->start;b=have?std::max(b,counters_->end):counters_->end;have=true;
    }
    if(activity_store_&&activity_store_->count){a=have?std::min(a,activity_store_->start):activity_store_->start;b=have?std::max(b,activity_store_->end):activity_store_->end;have=true;}
    view_start_=a;view_end_=std::max(b,a+.000001);range_start_=range_end_=0;
}
void Application::UpdateCounters() {
    double a=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);
    double b=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
    if(counter_stats_start_==a&&counter_stats_end_==b&&counter_stats_.size()==counters_->series.size())return;
    counter_stats_start_=a;counter_stats_end_=b;counter_stats_.clear();
    ++counter_stats_version_;
    for(size_t i=0;i<counters_->series.size();++i)counter_stats_.push_back(counters_->Stats(i,a,b));
}
void Application::FrameStatsView() {
    frame_stat_rows_=0;
    if(frame_stats_dock_pending_){
        auto* timing=ImGui::FindWindowByName(WindowName("Timing").c_str());
        if(timing&&timing->DockId){
            auto* node=ImGui::DockBuilderGetNode(timing->DockId);
            if(node&&node->IsLeafNode()){
                auto* root=node;while(root->ParentNode)root=root->ParentNode;
                ImGuiID bottom=0,top=0;ImGui::DockBuilderSplitNode(timing->DockId,ImGuiDir_Down,.35f,&bottom,&top);
                ImGui::DockBuilderDockWindow(WindowName("Frame stats").c_str(),bottom);ImGui::DockBuilderFinish(root->ID);
            }
            frame_stats_dock_pending_=false;
        }
    }
    if(!BeginView("Frame stats",&show_frame_stats_)){ImGui::End();return;}
    if(!frame_stat_samples_requested_&&!counter_reader_.Busy()&&!native_.Busy()&&std::any_of(counters_->series.begin(),counters_->series.end(),[](const auto& c){return c.frame_stats;})){
        frame_stat_samples_requested_=counter_reader_.Start(fs::u8path(exe_),trace_,cache_,counters_,counter_message_,true);
    }
    if(!counter_samples_requested_&&!native_.Busy()&&!counters_->series.empty()){
        ImGui::TextWrapped("Record CyInsights.RecordStats=1 with the plugin. Existing native counters can also be inspected by frame.");
        if(ImGui::Button(frame_stat_samples_requested_?"Load native counters too":"Load frame statistics"))counter_samples_requested_=counter_reader_.Start(fs::u8path(exe_),trace_,cache_,counters_,counter_message_);
    }
    if(counter_reader_.Busy())ImGui::TextDisabled("Loading samples in background...");
    if(!counter_message_.empty())ImGui::TextWrapped("%s",counter_message_.c_str());
    if(frame_stat_catalog_revision_!=counter_catalog_revision_||frame_stat_catalog_size_!=counters_->series.size()){
        frame_stat_catalog_revision_=counter_catalog_revision_;frame_stat_catalog_size_=counters_->series.size();
        frame_stat_groups_={"All","Unit","GPU","UObjects","Memory","Game"};
        for(const auto& c:counters_->series)if(!c.group.empty()&&std::find(frame_stat_groups_.begin(),frame_stat_groups_.end(),c.group)==frame_stat_groups_.end())frame_stat_groups_.push_back(c.group);
        frame_stat_filter_key_.clear();
    }
    const auto& groups=frame_stat_groups_;
    frame_stats_group_=std::clamp(frame_stats_group_,0,static_cast<int>(groups.size())-1);
    ImGui::SetNextItemWidth(140*UiScale());
    if(ImGui::BeginCombo("Group",groups[frame_stats_group_].c_str())){
        for(size_t i=0;i<groups.size();++i)if(ImGui::Selectable(groups[i].c_str(),frame_stats_group_==static_cast<int>(i)))frame_stats_group_=static_cast<int>(i);
        ImGui::EndCombo();
    }
    ImGui::SameLine();ImGui::SetNextItemWidth(240*UiScale());cyui::searchField("##frame_stat_search",frame_stats_search_,sizeof(frame_stats_search_),"Search statistics");
    const auto filter=groups[frame_stats_group_]+"/"+frame_stats_search_;
    if(frame_stat_filter_key_!=filter){frame_stat_filter_key_=filter;frame_stat_metrics_.clear();
        for(size_t i=0;i<counters_->series.size();++i){const auto& c=counters_->series[i];if(!c.availability&&(frame_stats_group_==0||c.group==groups[frame_stats_group_])&&Match(c.name,frame_stats_search_))frame_stat_metrics_.push_back(i);}
    }
    const auto& metrics=frame_stat_metrics_;
    ImGui::SetNextItemWidth(-1);
    if(selected_counter_<0){for(size_t i=0;i<counters_->series.size();++i)if(counters_->series[i].frame_stats&&counters_->series[i].name.find("Observed frame span")!=std::string::npos){selected_counter_=static_cast<int>(i);counters_->series[i].pinned=true;break;}}
    const bool selected=selected_counter_>=0&&static_cast<size_t>(selected_counter_)<counters_->series.size();
    if(ImGui::BeginCombo("##frame_stat_metric",selected?counters_->series[selected_counter_].name.c_str():"Select a statistic")){
        ImGuiListClipper clip;clip.Begin(static_cast<int>(metrics.size()));
        while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row){auto i=metrics[row];ImGui::PushID(static_cast<int>(i));if(ImGui::Selectable(counters_->series[i].name.c_str(),selected_counter_==static_cast<int>(i)))selected_counter_=static_cast<int>(i);ImGui::PopID();}
        ImGui::EndCombo();
    }
    for(const char* group:{"Unit","GPU","UObjects","Memory"}){
        std::string label=std::string(group)+" curves";
        if(ImGui::SmallButton(label.c_str()))for(auto& c:counters_->series)if(c.group==group&&!c.availability)c.pinned=true;
        ImGui::SameLine();
    }
    if(ImGui::SmallButton("Clear stat curves"))for(auto& c:counters_->series)if(!c.group.empty())c.pinned=false;
    if(selected_counter_<0||static_cast<size_t>(selected_counter_)>=counters_->series.size()){ImGui::TextDisabled("Choose a metric to see its values for each frame.");ImGui::End();return;}
    const size_t metric=static_cast<size_t>(selected_counter_);auto& c=counters_->series[metric];
    ImGui::Checkbox("Curve in timeline",&c.pinned);ImGui::SameLine();ImGui::TextDisabled("Missing frame = unavailable; no previous-value carry");
    ImGui::TextWrapped(c.frame_stats?"Snapshot values retain source game/stats frame IDs. Derived totals and unknown GPU execution-frame identity are shown separately.":"Native counter: last available observation inside each frame. Frame association is derived from time, not an engine snapshot ID.");
    if(cpu_frames_.empty()){ImGui::TextDisabled("No frame intervals available in this trace.");ImGui::End();return;}
    const double a=range_start_==range_end_?view_start_:std::min(range_start_,range_end_),b=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
    const auto& frames=cpu_frames_;
    auto first=std::lower_bound(frames.begin(),frames.end(),a,[](const auto& frame,double t){return frame.second<=t;});
    auto last=std::lower_bound(first,frames.end(),b,[](const auto& frame,double t){return frame.first<t;});
    frame_stat_rows_=static_cast<uint64_t>(last-first);
    ImGui::TextDisabled("%zu frames in %s interval",static_cast<size_t>(last-first),range_start_==range_end_?"visible":"selected");
    if(ImGui::BeginTable("frame_values",8,ImGuiTableFlags_Resizable|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX)){
        const char* headings[]={"Trace frame","Start (s)","Value","Unit","Source game frame","Source stats frame","Observed (s)","Provenance"};
        for(int column=0;column<8;++column)ImGui::TableSetupColumn(headings[column],column==7?ImGuiTableColumnFlags_WidthStretch:ImGuiTableColumnFlags_WidthFixed,column==7?1.f:(column==3?60.f:column==4||column==5?145.f:100.f)*UiScale());
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        ImGuiListClipper clip;clip.Begin(static_cast<int>(last-first));
        while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row){
            const auto index=static_cast<size_t>((first-frames.begin())+row);const auto& frame=frames[index];size_t sample=0;
            const auto* value=counters_->InFrame(metric,frame.first,frame.second,&sample);
            const auto* source=value&&sample<c.frame_samples.size()?&c.frame_samples[sample]:nullptr;
            ImGui::PushID(row);ImGui::TableNextRow();ImGui::TableNextColumn();
            const auto id=index<timing_model_.frame_ids.size()?timing_model_.frame_ids[index]:index;
            auto label=std::to_string(id)+(timing_model_.frames_native?"":" (derived)");
            if(ImGui::Selectable(label.c_str(),range_start_==frame.first&&range_end_==frame.second,ImGuiSelectableFlags_SpanAllColumns)){range_start_=frame.first;range_end_=frame.second;}
            if(ImGui::IsItemHovered()&&ImGui::IsMouseDoubleClicked(0)){view_start_=frame.first;view_end_=frame.second;}
            ImGui::TableNextColumn();ImGui::Text("%.6f",frame.first);
            ImGui::TableNextColumn();ImGui::TextUnformatted(value?c.Text(*value).c_str():"Unavailable");
            ImGui::TableNextColumn();ImGui::TextUnformatted(c.unit.empty()?"N/A":c.unit.c_str());
            ImGui::TableNextColumn();if(source&&(source->flags&cyfs::FrameBounds)&&source->game_frame!=UINT64_MAX)ImGui::Text("%llu",source->game_frame);else ImGui::TextDisabled("N/A");
            ImGui::TableNextColumn();if(source&&source->stats_frame>=0)ImGui::Text("%lld",source->stats_frame);else ImGui::TextDisabled("N/A");
            ImGui::TableNextColumn();if(value)ImGui::Text("%.6f",source?source->observed:value->time);else ImGui::TextDisabled("N/A");
            ImGui::TableNextColumn();
            if(!value)ImGui::TextDisabled("Unavailable");
            else if(source){
                auto provenance=std::string(source->flags&cyfs::Aggregate?"Derived aggregate":"Measured observation");
                if(!(source->flags&cyfs::FrameBounds))provenance+="; source bounds unknown";
                if(source->flags&cyfs::Delayed)provenance+="; delayed";
                if(source->flags&cyfs::ExecutionUnknown)provenance+="; execution frame unknown";
                ImGui::TextUnformatted(provenance.c_str());
            }else ImGui::TextUnformatted("Measured value; derived frame association");
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
void Application::Counters() {
    if(!BeginView("Counters",&show_counters_)){ImGui::End();return;}
    if(!counter_samples_requested_&&!counters_->series.empty()&&!native_.Busy()) {
        ImGui::TextWrapped("%zu counters defined. Samples load on request so they do not delay the timing view.",counters_->series.size());
        if(ImGui::Button("Load counter samples")) {
            counter_samples_requested_=counter_reader_.Start(fs::u8path(exe_),trace_,cache_,counters_,counter_message_);
        }
    }
    if(counter_reader_.Busy()){ImGui::Text("Loading counter samples...");ImGui::SameLine();if(ImGui::SmallButton("Cancel counters"))counter_reader_.Cancel();}
    ImGui::SetNextItemWidth(-1);
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F))ImGui::SetKeyboardFocusHere();
    cyui::searchField("##counter_filter",counter_search_,sizeof(counter_search_),"Search counters (Ctrl+F)");
    ImGui::Checkbox("Quest / Meta only",&meta_counters_only_);ImGui::SameLine();
    ImGui::Checkbox("Availability",&show_availability_);
    ImGui::Checkbox("Hide counters without values",&hide_empty_counters_);
    if(!counter_message_.empty())ImGui::TextWrapped("%s",counter_message_.c_str());
    if(counters_->series.empty()) {
        ImGui::TextWrapped(counter_reader_.Busy()?"Loading native counter values...":"No native counters available. Record the counters channel to include them.");
        ImGui::End();return;
    }
    UpdateCounters();
    ImGui::TextDisabled(range_start_==range_end_?"Statistics in visible interval":"Statistics in selected interval");
    ImGui::TextDisabled("Mean: derived sample average");
    std::string filterKey=std::string(counter_search_)+"/"+std::to_string(meta_counters_only_)+std::to_string(show_availability_)+std::to_string(hide_empty_counters_)+"/"+std::to_string(counters_->series.size())+"/"+std::to_string(counter_catalog_revision_);
    if(filterKey!=counter_filter_key_){counter_filter_key_=std::move(filterKey);counter_order_key_.clear();counter_visible_.clear();
    for(size_t i=0;i<counters_->series.size();++i) {
        auto& c=counters_->series[i];
        if((!meta_counters_only_||c.meta)&&(!c.availability||show_availability_)&&
           (!hide_empty_counters_||!c.samples.empty())&&Match(c.name,counter_search_))counter_visible_.push_back(i);
    }}auto& visible=counter_visible_;
    // Selected metric details stay visible above the scrolling table.
    if(selected_counter_>=0&&static_cast<size_t>(selected_counter_)<counters_->series.size()) {
        auto& c=counters_->series[static_cast<size_t>(selected_counter_)];
        ImGui::TextWrapped("%s",c.name.c_str());
        ImGui::Checkbox("Show in timeline",&c.pinned);
        if(!c.reason.empty())ImGui::TextWrapped("Unavailable: %s",c.reason.c_str());
        if(c.meta)ImGui::TextWrapped("Runtime observation on the CPU clock. GPU execution frame and measurement interval unavailable.");
        if(c.reset_per_frame)ImGui::TextDisabled("Native counter resets every frame");
    }
    if(ImGui::BeginTable("native_counters",6,ImGuiTableFlags_Resizable|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_ScrollX|ImGuiTableFlags_Sortable)) {
        ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_WidthStretch,3,0);
        ImGui::TableSetupColumn("Samples",ImGuiTableColumnFlags_WidthFixed,60*UiScale(),1);
        ImGui::TableSetupColumn("Min",ImGuiTableColumnFlags_WidthFixed,75*UiScale(),2);
        ImGui::TableSetupColumn("Max",ImGuiTableColumnFlags_WidthFixed,75*UiScale(),3);
        ImGui::TableSetupColumn("Mean",ImGuiTableColumnFlags_WidthFixed,75*UiScale(),4);
        ImGui::TableSetupColumn("Unit",ImGuiTableColumnFlags_WidthFixed,85*UiScale(),5);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        if(auto sorting=ImGui::TableGetSortSpecs();sorting&&sorting->SpecsCount) {
            auto spec=sorting->Specs[0];
            auto orderKey=std::to_string(spec.ColumnUserID)+"/"+std::to_string(spec.SortDirection)+"/"+std::to_string(spec.ColumnUserID>=1&&spec.ColumnUserID<=4?counter_stats_version_:0);
            if(orderKey!=counter_order_key_){counter_order_key_=std::move(orderKey);
            std::stable_sort(visible.begin(),visible.end(),[&](size_t a,size_t b){
                int order=0;const auto& x=counter_stats_[a];const auto& y=counter_stats_[b];
                if(spec.ColumnUserID==0)order=counters_->series[a].name.compare(counters_->series[b].name);
                else if(spec.ColumnUserID==5)order=counters_->series[a].unit.compare(counters_->series[b].unit);
                else if((spec.ColumnUserID==2||spec.ColumnUserID==3)&&x.count&&y.count) {
                    const auto& ca=counters_->series[a];const auto& cb=counters_->series[b];
                    order=Compare(ca,ca.samples[spec.ColumnUserID==2?x.minimum_sample:x.maximum_sample],
                                  cb,cb.samples[spec.ColumnUserID==2?y.minimum_sample:y.maximum_sample]);
                }
                else if(spec.ColumnUserID>=2&&spec.ColumnUserID<=4&&(!x.count||!y.count))order=x.count?1:y.count?-1:0;
                else {double vx=spec.ColumnUserID==1?double(x.count):spec.ColumnUserID==2?x.minimum:spec.ColumnUserID==3?x.maximum:x.mean;
                      double vy=spec.ColumnUserID==1?double(y.count):spec.ColumnUserID==2?y.minimum:spec.ColumnUserID==3?y.maximum:y.mean;
                      order=vx<vy?-1:vx>vy?1:0;}
                if(!order)return counters_->series[a].id<counters_->series[b].id;
                return spec.SortDirection==ImGuiSortDirection_Descending?order>0:order<0;
            });}sorting->SpecsDirty=false;
        }
        ImGuiListClipper clip;clip.Begin(static_cast<int>(visible.size()));
        while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
            size_t i=visible[static_cast<size_t>(row)];auto& c=counters_->series[i];const auto& stats=counter_stats_[i];
            ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();
            std::string label=std::string(c.pinned?"+ ":"")+c.name;
            if(ImGui::Selectable(label.c_str(),selected_counter_==static_cast<int>(i),ImGuiSelectableFlags_SpanAllColumns))selected_counter_=static_cast<int>(i);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s: %u / %s\nUnit: %s\nInvalid or unavailable value samples in interval: %zu\nDouble-click to toggle its timeline track.%s%s",
                    c.name.c_str(),c.frame_stats?"Frame-stat metric ID":"Native CounterId",c.frame_stats?c.frame_stat_id:c.id,c.floating?"Double":"Int64",c.unit.empty()?"unavailable":c.unit.c_str(),stats.unavailable,
                    c.reason.empty()?"":"\n",c.reason.c_str());
                if(ImGui::IsMouseDoubleClicked(0))c.pinned=!c.pinned;
            }
            ImGui::TableNextColumn();ImGui::Text("%zu",stats.count);
            ImGui::TableNextColumn();ImGui::TextUnformatted(stats.count?c.Text(c.samples[stats.minimum_sample]).c_str():"N/A");
            ImGui::TableNextColumn();ImGui::TextUnformatted(stats.count?c.Text(c.samples[stats.maximum_sample]).c_str():"N/A");
            ImGui::TableNextColumn();if(stats.count)ImGui::Text("%.6g",stats.mean);else ImGui::TextDisabled("N/A");
            ImGui::TableNextColumn();ImGui::TextUnformatted(c.unit.empty()?"N/A":c.unit.c_str());
            ImGui::PopID();
        }ImGui::EndTable();
    }
    ImGui::End();
}
void Application::CounterTracks(float width) {
    auto* draw=ImGui::GetWindowDrawList();const double span=std::max(.000001,view_end_-view_start_);
    const float scale=UiScale(),header=20*scale,height=98*scale;
    auto clip=ImGui::GetCurrentWindow()->ClipRect;
    if(counter_plots_.size()!=counters_->series.size())counter_plots_.resize(counters_->series.size());
    for(size_t i=0;i<counters_->series.size();++i) {
        auto& c=counters_->series[i];if(!c.pinned)continue;
        ImGui::PushID(static_cast<int>(i));const auto pos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("counter_lane",ImVec2(width,height));const bool hover=ImGui::IsItemHovered();
        if(pos.y+height<clip.Min.y||pos.y>clip.Max.y){ImGui::PopID();continue;}
        draw->AddRectFilled(pos,ImVec2(pos.x+width,pos.y+header),cyui::palette().bg2);
        std::string label=c.name+(c.meta?"  [observation]":"");
        draw->PushClipRect(pos,ImVec2(pos.x+width,pos.y+header),true);
        draw->AddText(ImVec2(pos.x+4*scale,pos.y+2*scale),Colour(i),label.c_str());draw->PopClipRect();
        auto& cache=counter_plots_[i];int pixels=std::clamp(static_cast<int>(width),1,16384);
        if(cache.a!=view_start_||cache.b!=view_end_||cache.width!=pixels) {
            cache.a=view_start_;cache.b=view_end_;cache.width=pixels;
            cache.buckets=counters_->Plot(i,view_start_,view_end_,pixels);
        }
        double low=0,high=0;bool any=false;size_t low_sample=0,high_sample=0;
        for(auto& bucket:cache.buckets) {
            double a=c.samples[bucket.minimum].Value(c.floating),b=c.samples[bucket.maximum].Value(c.floating);
            if(!any||(c.floating?c.samples[bucket.minimum].real<c.samples[low_sample].real:c.samples[bucket.minimum].integer<c.samples[low_sample].integer))low_sample=bucket.minimum;
            if(!any||(c.floating?c.samples[bucket.maximum].real>c.samples[high_sample].real:c.samples[bucket.maximum].integer>c.samples[high_sample].integer))high_sample=bucket.maximum;
            low=any?std::min(low,a):a;high=any?std::max(high,b):b;any=true;
        }
        const double normal=std::max({std::abs(low),std::abs(high),1.});
        auto y=[&](double value){double range=high/normal-low/normal;
            double fraction=range>0?(value/normal-low/normal)/range:.5;
            return pos.y+height-8*scale-static_cast<float>(fraction)*(height-header-18*scale);};
        draw->PushClipRect(ImVec2(pos.x,pos.y+header),ImVec2(pos.x+width,pos.y+height),true);
        for(int tick=0;tick<9;++tick) {
            const float x=pos.x+width*static_cast<float>(tick)/8;
            draw->AddLine(ImVec2(x,pos.y+header),ImVec2(x,pos.y+height),cyui::palette().grid);
        }
        if(any) {
            auto bounds="Min "+c.Text(c.samples[low_sample])+" / Max "+c.Text(c.samples[high_sample])+" "+(c.unit.empty()?"(unit unavailable)":c.unit);
            draw->AddText(ImVec2(pos.x+5*scale,pos.y+header+2*scale),cyui::palette().textDim,bounds.c_str());
        }
        const CounterBucket* previous=nullptr;
        for(const auto& bucket:cache.buckets) {
            float x=pos.x+static_cast<float>(bucket.pixel);
            if(previous&&previous->last+1==bucket.first){
                bool continuous=true;
                if(c.frame_stats){const auto& before=c.frame_samples[previous->last];const auto& after=c.frame_samples[bucket.first];
                    continuous=(before.flags&cyfs::FrameBounds)&&(after.flags&cyfs::FrameBounds)&&before.game_frame!=UINT64_MAX&&after.game_frame==before.game_frame+1;
                }
                if(continuous)draw->AddLine(ImVec2(pos.x+static_cast<float>(previous->pixel),y(c.samples[previous->last].Value(c.floating))),ImVec2(x,y(c.samples[bucket.first].Value(c.floating))),Colour(i),1.5f*scale);
            }
            draw->AddLine(ImVec2(x,y(c.samples[bucket.minimum].Value(c.floating))),ImVec2(x,y(c.samples[bucket.maximum].Value(c.floating))),Colour(i));
            draw->AddCircleFilled(ImVec2(x,y(c.samples[bucket.last].Value(c.floating))),1.4f*scale,Colour(i),6);
            previous=&bucket;
        }
        if(!any)draw->AddText(ImVec2(pos.x+5*scale,pos.y+header+8*scale),cyui::palette().textFaint,"No available observations in this interval");
        if(c.companion>=0) {
            const auto& flag=counters_->series[static_cast<size_t>(c.companion)];
            for(size_t j=flag.Lower(view_start_);j<flag.samples.size()&&flag.samples[j].time<=view_end_;++j) {
                if(flag.samples[j].Value(flag.floating)==1&&flag.samples[j].valid)continue;
                float x=pos.x+width*static_cast<float>((flag.samples[j].time-view_start_)/span);
                draw->AddLine(ImVec2(x,pos.y+height-7*scale),ImVec2(x,pos.y+height-2*scale),cyui::palette().error,2*scale);
            }
        }
        if(range_start_!=range_end_) {
            float a=pos.x+width*static_cast<float>((std::min(range_start_,range_end_)-view_start_)/span);
            float b=pos.x+width*static_cast<float>((std::max(range_start_,range_end_)-view_start_)/span);
            draw->AddRectFilled(ImVec2(a,pos.y+header),ImVec2(b,pos.y+height),cyui::palette().selection);
        }
        draw->PopClipRect();
        draw->AddLine(ImVec2(pos.x,pos.y+height-1),ImVec2(pos.x+width,pos.y+height-1),cyui::palette().lineStrong);
        if(hover) {
            double t=view_start_+span*std::clamp(double((ImGui::GetIO().MousePos.x-pos.x)/width),0.,1.);
            const auto* sample=counters_->At(i,t);
            if(c.frame_stats){
                sample=nullptr;const auto frame=std::lower_bound(cpu_frames_.begin(),cpu_frames_.end(),t,[](const auto& interval,double time){return interval.second<=time;});
                if(frame!=cpu_frames_.end()&&frame->first<=t)sample=counters_->InFrame(i,frame->first,frame->second);
            }
            if(sample&&c.frame_stats){
                const auto& source=c.frame_samples[static_cast<size_t>(sample-c.samples.data())];
                ImGui::SetTooltip("%s\nValue: %s / unit: %s\nSource interval: %.9f - %.9f s\nObserved: %.9f s\nGame frame: %llu / stats frame: %lld\n%s%s%s",
                    c.name.c_str(),c.Text(*sample).c_str(),c.unit.c_str(),source.begin,source.end,source.observed,source.game_frame,source.stats_frame,
                    source.flags&cyfs::Aggregate?"Derived aggregate of recorded stats":"Measured observation",
                    source.flags&cyfs::Delayed?"\nResult received after its source frame":"",
                    source.flags&cyfs::ExecutionUnknown?"\nExecution-frame identity unavailable for this scalar":"");
            }
            else if(sample)ImGui::SetTooltip("%s\nValue: %s / unit: %s\nLast observation: %.9f s (native CPU clock)\nCursor: %.9f s%s",
                c.name.c_str(),c.Text(*sample).c_str(),c.unit.empty()?"unavailable":c.unit.c_str(),sample->time,t,
                c.meta?"\nGPU execution frame and runtime measurement interval unavailable.\nPoints are observations; dense columns retain min/max (derived display).":"\nPoints are native counter values; dense columns retain min/max (derived display).");
            else ImGui::SetTooltip("%s\nUnavailable at %.9f s",c.name.c_str(),t);
            if(ImGui::IsMouseClicked(0))selected_counter_=static_cast<int>(i);
        }
        ImGui::PopID();
    }
}
}
