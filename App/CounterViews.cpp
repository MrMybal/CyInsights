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
    static const ImU32 palette[]={IM_COL32(104,201,213,255),IM_COL32(209,163,224,255),IM_COL32(223,195,111,255),IM_COL32(136,200,142,255)};
    return palette[i%4];
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
    if(std::any_of(counters_.series.begin(),counters_.series.end(),[](const CounterSeries& c){return !c.samples.empty();})) {
        a=have?std::min(a,counters_.start):counters_.start;b=have?std::max(b,counters_.end):counters_.end;have=true;
    }
    if(activity_store_&&activity_store_->count){a=have?std::min(a,activity_store_->start):activity_store_->start;b=have?std::max(b,activity_store_->end):activity_store_->end;have=true;}
    view_start_=a;view_end_=std::max(b,a+.000001);range_start_=range_end_=0;
}
void Application::UpdateCounters() {
    double a=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);
    double b=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
    if(counter_stats_start_==a&&counter_stats_end_==b&&counter_stats_.size()==counters_.series.size())return;
    counter_stats_start_=a;counter_stats_end_=b;counter_stats_.clear();
    for(size_t i=0;i<counters_.series.size();++i)counter_stats_.push_back(counters_.Stats(i,a,b));
}
void Application::Counters() {
    if(!ImGui::Begin("Counters",&show_counters_)){ImGui::End();return;}
    if(!counter_samples_requested_&&!counters_.series.empty()&&!native_.Busy()) {
        ImGui::TextWrapped("%zu counters defined. Samples load on request so they do not delay the timing view.",counters_.series.size());
        if(ImGui::Button("Load counter samples")) {
            counter_samples_requested_=counter_reader_.Start(fs::u8path(exe_),trace_,cache_,counters_,counter_message_);
        }
    }
    if(counter_reader_.Busy()){ImGui::Text("Loading counter samples...");ImGui::SameLine();if(ImGui::SmallButton("Cancel counters"))counter_reader_.Cancel();}
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##counter_filter","Search counters",counter_search_,sizeof(counter_search_));
    ImGui::Checkbox("Quest / Meta only",&meta_counters_only_);ImGui::SameLine();
    ImGui::Checkbox("Availability",&show_availability_);
    ImGui::Checkbox("Hide counters without values",&hide_empty_counters_);
    if(!counter_message_.empty())ImGui::TextWrapped("%s",counter_message_.c_str());
    if(counters_.series.empty()) {
        ImGui::TextWrapped(counter_reader_.Busy()?"Loading native counter values...":"No native counters available. Record the counters channel to include them.");
        ImGui::End();return;
    }
    UpdateCounters();
    ImGui::TextDisabled(range_start_==range_end_?"Statistics in visible interval":"Statistics in selected interval");
    ImGui::TextDisabled("Mean: derived sample average");
    std::vector<size_t> visible;
    for(size_t i=0;i<counters_.series.size();++i) {
        auto& c=counters_.series[i];
        if((!meta_counters_only_||c.meta)&&(!c.availability||show_availability_)&&
           (!hide_empty_counters_||!c.samples.empty())&&Match(c.name,counter_search_))visible.push_back(i);
    }
    // Selected metric details stay visible above the scrolling table.
    if(selected_counter_>=0&&static_cast<size_t>(selected_counter_)<counters_.series.size()) {
        auto& c=counters_.series[static_cast<size_t>(selected_counter_)];
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
            std::stable_sort(visible.begin(),visible.end(),[&](size_t a,size_t b){
                int order=0;const auto& x=counter_stats_[a];const auto& y=counter_stats_[b];
                if(spec.ColumnUserID==0)order=counters_.series[a].name.compare(counters_.series[b].name);
                else if(spec.ColumnUserID==5)order=counters_.series[a].unit.compare(counters_.series[b].unit);
                else if((spec.ColumnUserID==2||spec.ColumnUserID==3)&&x.count&&y.count) {
                    const auto& ca=counters_.series[a];const auto& cb=counters_.series[b];
                    order=Compare(ca,ca.samples[spec.ColumnUserID==2?x.minimum_sample:x.maximum_sample],
                                  cb,cb.samples[spec.ColumnUserID==2?y.minimum_sample:y.maximum_sample]);
                }
                else if(spec.ColumnUserID>=2&&spec.ColumnUserID<=4&&(!x.count||!y.count))order=x.count?1:y.count?-1:0;
                else {double vx=spec.ColumnUserID==1?double(x.count):spec.ColumnUserID==2?x.minimum:spec.ColumnUserID==3?x.maximum:x.mean;
                      double vy=spec.ColumnUserID==1?double(y.count):spec.ColumnUserID==2?y.minimum:spec.ColumnUserID==3?y.maximum:y.mean;
                      order=vx<vy?-1:vx>vy?1:0;}
                if(!order)return counters_.series[a].id<counters_.series[b].id;
                return spec.SortDirection==ImGuiSortDirection_Descending?order>0:order<0;
            });sorting->SpecsDirty=false;
        }
        ImGuiListClipper clip;clip.Begin(static_cast<int>(visible.size()));
        while(clip.Step())for(int row=clip.DisplayStart;row<clip.DisplayEnd;++row) {
            size_t i=visible[static_cast<size_t>(row)];auto& c=counters_.series[i];const auto& stats=counter_stats_[i];
            ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();
            std::string label=std::string(c.pinned?"+ ":"")+c.name;
            if(ImGui::Selectable(label.c_str(),selected_counter_==static_cast<int>(i),ImGuiSelectableFlags_SpanAllColumns))selected_counter_=static_cast<int>(i);
            if(ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\nNative CounterId: %u / %s\nUnit: %s\nInvalid or unavailable value samples in interval: %zu\nDouble-click to toggle its timeline track.%s%s",
                    c.name.c_str(),c.id,c.floating?"Double":"Int64",c.unit.empty()?"unavailable":c.unit.c_str(),stats.unavailable,
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
    if(counter_plots_.size()!=counters_.series.size())counter_plots_.resize(counters_.series.size());
    for(size_t i=0;i<counters_.series.size();++i) {
        auto& c=counters_.series[i];if(!c.pinned)continue;
        ImGui::PushID(static_cast<int>(i));const auto pos=ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("counter_lane",ImVec2(width,height));const bool hover=ImGui::IsItemHovered();
        if(pos.y+height<clip.Min.y||pos.y>clip.Max.y){ImGui::PopID();continue;}
        draw->AddRectFilled(pos,ImVec2(pos.x+width,pos.y+header),IM_COL32(59,59,59,255));
        std::string label=c.name+(c.meta?"  [observation]":"");
        draw->PushClipRect(pos,ImVec2(pos.x+width,pos.y+header),true);
        draw->AddText(ImVec2(pos.x+4*scale,pos.y+2*scale),Colour(i),label.c_str());draw->PopClipRect();
        auto& cache=counter_plots_[i];int pixels=std::clamp(static_cast<int>(width),1,16384);
        if(cache.a!=view_start_||cache.b!=view_end_||cache.width!=pixels) {
            cache.a=view_start_;cache.b=view_end_;cache.width=pixels;
            cache.buckets=counters_.Plot(i,view_start_,view_end_,pixels);
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
            draw->AddLine(ImVec2(x,pos.y+header),ImVec2(x,pos.y+height),IM_COL32(79,79,79,255));
        }
        if(any) {
            auto bounds="Min "+c.Text(c.samples[low_sample])+" / Max "+c.Text(c.samples[high_sample])+" "+(c.unit.empty()?"(unit unavailable)":c.unit);
            draw->AddText(ImVec2(pos.x+5*scale,pos.y+header+2*scale),IM_COL32(183,183,183,255),bounds.c_str());
        }
        for(const auto& bucket:cache.buckets) {
            float x=pos.x+static_cast<float>(bucket.pixel);
            draw->AddLine(ImVec2(x,y(c.samples[bucket.minimum].Value(c.floating))),ImVec2(x,y(c.samples[bucket.maximum].Value(c.floating))),Colour(i));
            draw->AddCircleFilled(ImVec2(x,y(c.samples[bucket.last].Value(c.floating))),1.4f*scale,Colour(i),6);
        }
        if(!any)draw->AddText(ImVec2(pos.x+5*scale,pos.y+header+8*scale),IM_COL32(160,160,160,255),"No available observations in this interval");
        if(c.companion>=0) {
            const auto& flag=counters_.series[static_cast<size_t>(c.companion)];
            for(size_t j=flag.Lower(view_start_);j<flag.samples.size()&&flag.samples[j].time<=view_end_;++j) {
                if(flag.samples[j].Value(flag.floating)==1&&flag.samples[j].valid)continue;
                float x=pos.x+width*static_cast<float>((flag.samples[j].time-view_start_)/span);
                draw->AddLine(ImVec2(x,pos.y+height-7*scale),ImVec2(x,pos.y+height-2*scale),IM_COL32(223,119,115,255),2*scale);
            }
        }
        if(range_start_!=range_end_) {
            float a=pos.x+width*static_cast<float>((std::min(range_start_,range_end_)-view_start_)/span);
            float b=pos.x+width*static_cast<float>((std::max(range_start_,range_end_)-view_start_)/span);
            draw->AddRectFilled(ImVec2(a,pos.y+header),ImVec2(b,pos.y+height),IM_COL32(88,143,195,35));
        }
        draw->PopClipRect();
        draw->AddLine(ImVec2(pos.x,pos.y+height-1),ImVec2(pos.x+width,pos.y+height-1),IM_COL32(54,54,54,255));
        if(hover) {
            double t=view_start_+span*std::clamp(double((ImGui::GetIO().MousePos.x-pos.x)/width),0.,1.);
            const auto* sample=counters_.At(i,t);
            if(sample)ImGui::SetTooltip("%s\nValue: %s / unit: %s\nLast observation: %.9f s (native CPU clock)\nCursor: %.9f s%s",
                c.name.c_str(),c.Text(*sample).c_str(),c.unit.empty()?"unavailable":c.unit.c_str(),sample->time,t,
                c.meta?"\nGPU execution frame and runtime measurement interval unavailable.\nPoints are observations; dense columns retain min/max (derived display).":"\nPoints are native counter values; dense columns retain min/max (derived display).");
            else ImGui::SetTooltip("%s\nUnavailable at %.9f s",c.name.c_str(),t);
            if(ImGui::IsMouseClicked(0))selected_counter_=static_cast<int>(i);
        }
        ImGui::PopID();
    }
}
}
