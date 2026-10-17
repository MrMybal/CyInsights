// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "UiMetrics.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <imgui.h>
namespace cyinsights {
namespace {
void Cost(std::optional<double> cost,bool signed_value=false,bool percent=false) {
    if(!cost){ImGui::TextDisabled("N/A");return;}
    double value=percent?*cost:*cost*1000;
    if(signed_value)ImGui::TextColored(value>0?ImVec4(.96f,.53f,.48f,1):value<0?ImVec4(.53f,.83f,.62f,1):ImVec4(.75f,.75f,.75f,1),percent?"%+.2f%%":"%+.3f",value);
    else ImGui::Text("%.3f",value);
}
double SortValue(const ComparisonRow& row,int column,ComparisonMode mode) {
    switch(column) {
    case 3:return static_cast<double>(row.a.count);
    case 4:return static_cast<double>(row.b.count);
    case 5:return ComparisonCost(row.a,mode).value_or(-INFINITY);
    case 6:return ComparisonCost(row.b,mode).value_or(-INFINITY);
    case 7:return ComparisonDelta(row,mode).value_or(-INFINITY);
    case 8:return ComparisonDelta(row,mode,true).value_or(-INFINITY);
    case 9:return ComparisonCost(row.a,mode,true).value_or(-INFINITY);
    default:return ComparisonCost(row.b,mode,true).value_or(-INFINITY);
    }
}
}
void Application::OpenComparison(const fs::path& file) {
    if(AnalysisBusy()){message_="Wait for the active analysis before loading comparison B";return;}
    wchar_t temp[32768]{};GetTempPathW(32768,temp);
    auto cache=fs::path(temp)/"CyInsights"/("comparison_"+std::to_string(GetCurrentProcessId())+"_"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(comparison_reader_.Start(file,fs::u8path(exe_),cache,comparison_message_)) {
        comparison_message_="Loading comparison B in background...";show_comparison_=true;session_view_=false;comparison_frame_b_=0;
    }
    message_=comparison_message_;
}
void Application::Comparison() {
    if(!BeginView("Capture comparison",&show_comparison_)){ImGui::End();return;}
    ImGui::BeginDisabled(AnalysisBusy()||native_.events.empty());
    if(ImGui::Button("Choose capture B...")) {
        auto file=ChooseFile(L"Unreal / CyInsights captures\0*.cycapture;*.utrace\0\0");
        if(!file.empty())OpenComparison(file);
    }
    ImGui::EndDisabled();
    if(!comparison_message_.empty())ImGui::TextWrapped("%s",comparison_message_.c_str());
    if(comparison_reader_.Busy()) {
        if(ImGui::Button("Cancel comparison"))comparison_reader_.Cancel();
        ImGui::TextDisabled("A remains displayed in the Timing view. Comparison data is being analyzed.");
        ImGui::End();return;
    }
    if(native_.events.empty()||!comparison_b_) {
        ImGui::TextWrapped("Open native timing as A, then choose a .utrace or .cycapture as B.");ImGui::End();return;
    }
    const auto& b=*comparison_b_;
    ImGui::TextWrapped("A: %s",capture_file_.filename().u8string().c_str());
    ImGui::TextWrapped("B: %s",b.file.filename().u8string().c_str());
    ImGui::TextDisabled("Analyzer B: %s",b.analyzer.u8string().c_str());
    for(const char* key:{"project","engine_version","platform","graphics_api","cpu","gpu"}) {
        auto a_value=capture_.manifest["metadata"][key].AsString(),b_value=b.metadata[key].AsString();
        if(a_value!=b_value)ImGui::TextColored(ImVec4(.9f,.74f,.4f,1),"%s: A %s / B %s",key,
            a_value.empty()?"unavailable":a_value.c_str(),b_value.empty()?"unavailable":b_value.c_str());
    }
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Metadata differences can affect cost comparisons. Unknown metadata is not inferred.");
    ImGui::TextDisabled("A interval follows the selected region, otherwise the visible timing range.");
    ImGui::Text("A: %.9f .. %.9f s",timing_model_.aggregate_start,timing_model_.aggregate_end);
    bool apply=false;
    ImGui::SetNextItemWidth(150*UiScale());ImGui::InputDouble("B start (s)",&comparison_b_start_,.001,.1,"%.9f");ImGui::SameLine();
    ImGui::SetNextItemWidth(150*UiScale());ImGui::InputDouble("B end (s)",&comparison_b_end_,.001,.1,"%.9f");ImGui::SameLine();
    if(ImGui::Button("Apply B interval"))apply=true;
    if(ImGui::Button("Full B")){comparison_b_start_=b.timing.start;comparison_b_end_=b.timing.end;apply=true;}
    if(!b.timing.frames.empty()) {
        ImGui::SameLine();ImGui::SetNextItemWidth(105*UiScale());
        ImGui::InputInt(b.timing.frames_native?"B native frame row":"B tick candidate",&comparison_frame_b_);
        comparison_frame_b_=std::clamp(comparison_frame_b_,0,static_cast<int>(b.timing.frames.size())-1);
        ImGui::SameLine();if(ImGui::Button(b.timing.frames_native?"Use B native frame interval":"Use B tick interval")) {
            const auto range=b.timing.frames[static_cast<size_t>(comparison_frame_b_)];comparison_b_start_=range.first;comparison_b_end_=range.second;apply=true;
        }
    }
    ImGui::TextDisabled("Active B: %.9f .. %.9f s / each capture keeps its own clock",b.start,b.end);
    if(b.timing.frames_native&&!b.timing.frames.empty())ImGui::TextDisabled("Native game frame index: %llu",
        static_cast<unsigned long long>(b.timing.frame_ids[static_cast<size_t>(comparison_frame_b_)]));
    if(apply&&comparison_reader_.Range(comparison_b_,comparison_b_start_,comparison_b_end_,comparison_message_)) {
        comparison_message_="Updating B interval in background...";ImGui::End();return;
    }
    if(comparison_a_start_!=timing_model_.aggregate_start||comparison_a_end_!=timing_model_.aggregate_end) {
        comparison_rows_=CompareTiming(native_.store?comparison_a_summary_:comparison_index_.Summarize(native_.events,timing_model_),b.summary);
        comparison_a_start_=timing_model_.aggregate_start;comparison_a_end_=timing_model_.aggregate_end;
    }
    ImGui::Separator();
    ImGui::SetNextItemWidth(240*UiScale());ImGui::InputTextWithHint("##comparison_search","Search timer groups",comparison_search_,sizeof(comparison_search_));ImGui::SameLine();
    ImGui::SetNextItemWidth(160*UiScale());ImGui::Combo("Cost",&comparison_mode_,"Totals\0Per second\0Per occurrence\0");ImGui::SameLine();
    ImGui::SetNextItemWidth(105*UiScale());ImGui::Combo("Tracks",&comparison_domain_,"CPU + GPU\0CPU\0GPU\0");
    ImGui::TextWrapped("Derived comparison by name and track domain. Native IDs belong to their own session; GPU domain follows track naming. All costs are clipped to the chosen interval; per-occurrence means use these clipped costs. Inclusive sums overlap; exclusive costs use contained children. Positive delta means B costs more than A.");
    const auto mode=static_cast<ComparisonMode>(comparison_mode_);
    const char* unit=mode==ComparisonMode::PerSecond?"ms/s":mode==ComparisonMode::PerOccurrence?"ms/occ":"ms";
    ImGui::TextDisabled("Costs: %s / N/A: no complete observations in that interval / A and B are not clock-aligned",unit);
    std::vector<size_t> visible;
    for(size_t i=0;i<comparison_rows_.size();++i) {
        const auto& row=comparison_rows_[i];
        if((comparison_domain_==0||(comparison_domain_==1&&!row.key.gpu)||(comparison_domain_==2&&row.key.gpu))&&TimingMatches(row.key.name,comparison_search_))visible.push_back(i);
    }
    if(ImGui::BeginTable("comparison_totals",11,ImGuiTableFlags_Resizable|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollX|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Sortable)) {
        const char* names[]={"Timer name","Domain","Correspondence","Count A","Count B","Incl A","Incl B","Delta B-A","Delta %","Excl A","Excl B"};
        for(int i=0;i<11;++i)ImGui::TableSetupColumn(names[i],i==0?ImGuiTableColumnFlags_WidthStretch:
            ImGuiTableColumnFlags_WidthFixed|(i==7?ImGuiTableColumnFlags_DefaultSort|ImGuiTableColumnFlags_PreferSortDescending:0),i==0?3.f:(i==2?160.f:85.f)*UiScale(),static_cast<ImGuiID>(i));
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        if(auto* sort=ImGui::TableGetSortSpecs();sort&&sort->SpecsCount) {
            auto spec=sort->Specs[0];
            std::stable_sort(visible.begin(),visible.end(),[&](size_t a,size_t b_index){
                const auto& x=comparison_rows_[a];const auto& y=comparison_rows_[b_index];int order=0;
                if(spec.ColumnUserID==0)order=x.key.name.compare(y.key.name);
                else if(spec.ColumnUserID==1)order=x.key.gpu==y.key.gpu?0:x.key.gpu?1:-1;
                else if(spec.ColumnUserID==2)order=std::string(x.Relation()).compare(y.Relation());
                else {double vx=SortValue(x,spec.ColumnUserID,mode),vy=SortValue(y,spec.ColumnUserID,mode);order=vx<vy?-1:vx>vy?1:0;}
                return order?(spec.SortDirection==ImGuiSortDirection_Descending?order>0:order<0):a<b_index;
            });sort->SpecsDirty=false;
        }
        ImGuiListClipper clip;clip.Begin(static_cast<int>(visible.size()));
        while(clip.Step())for(int n=clip.DisplayStart;n<clip.DisplayEnd;++n) {
            auto i=visible[static_cast<size_t>(n)];const auto& row=comparison_rows_[i];
            ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();
            ImGui::Selectable(row.key.name.c_str(),false,ImGuiSelectableFlags_SpanAllColumns);
            if(ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();ImGui::TextUnformatted(row.key.name.c_str());
                ImGui::Text("%s / IDs are session-local",row.Relation());
                for(const auto* side:{&row.a,&row.b}) {
                    ImGui::Text("%s: %zu complete / %zu unfinished",side==&row.a?"A":"B",side->count,side->unfinished);
                    for(auto id:side->group.native_ids)ImGui::Text("TimerId %llu",static_cast<unsigned long long>(id));
                    if(side->group.identity_missing)ImGui::TextDisabled("Native identity unavailable");
                }
                ImGui::TextUnformatted("Double-click focuses A only when its group contains one timer.");ImGui::EndTooltip();
                if(ImGui::IsMouseDoubleClicked(0)&&row.a.group.timers.size()==1) {show_timing_=true;FocusTimer(row.a.group.timers.front());ImGui::SetWindowFocus(WindowName("Timing").c_str());}
            }
            ImGui::TableNextColumn();ImGui::TextUnformatted(row.key.gpu?"GPU*":"CPU*");
            ImGui::TableNextColumn();ImGui::TextUnformatted(row.Relation());
            ImGui::TableNextColumn();if(row.defined_a)ImGui::Text("%zu",row.a.count);else ImGui::TextDisabled("N/A");
            ImGui::TableNextColumn();if(row.defined_b)ImGui::Text("%zu",row.b.count);else ImGui::TextDisabled("N/A");
            ImGui::TableNextColumn();Cost(ComparisonCost(row.a,mode));
            ImGui::TableNextColumn();Cost(ComparisonCost(row.b,mode));
            ImGui::TableNextColumn();Cost(ComparisonDelta(row,mode),true);
            ImGui::TableNextColumn();Cost(ComparisonDelta(row,mode,true),true,true);
            ImGui::TableNextColumn();Cost(ComparisonCost(row.a,mode,true));
            ImGui::TableNextColumn();Cost(ComparisonCost(row.b,mode,true));
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}
}
