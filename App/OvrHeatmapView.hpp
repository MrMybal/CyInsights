// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "../Core/OvrCapture.hpp"
#include <cyui/CyUI.h>
#include <imgui.h>
#include <cstdio>
#include <set>

namespace cyovr {
// The same native widget is used in the Quest app and the desktop reader.
inline void Heatmap(const Capture& capture,int& selected_surface,int& selected_tile,int& metric,ImTextureID image=0,float opacity=.35f) {
    cyui::callout(cyui::Tone::info,"Recorded GPU snapshot. Render includes vertex and fragment work. An associated image is an approximate visual reference; exact eye, mesh and execution-frame correspondence is unknown.");
    if(capture.surfaces.empty()) {
        cyui::emptyState("No measured tiles",capture.reason.empty()?"Select a target and capture a short detailed GPU snapshot.":capture.reason.c_str());return;
    }
    selected_surface=std::clamp(selected_surface,0,int(capture.surfaces.size()-1));
    auto label=[&](size_t i){const auto& s=capture.surfaces[i];char b[200];std::snprintf(b,sizeof(b),"Surface %s / %ux%u / MSAA %u / %.2f ms",s.id.c_str(),s.width,s.height,s.msaa,s.duration_us/1000);return std::string(b);};
    const float controls_width=ImGui::GetContentRegionAvail().x;
    const bool inline_stage=controls_width>=cyui::dp(600);
    ImGui::SetNextItemWidth(inline_stage?controls_width-cyui::dp(175):controls_width);
    if(ImGui::BeginCombo("##surface",label(size_t(selected_surface)).c_str())) {
        for(size_t i=0;i<capture.surfaces.size();++i)if(ImGui::Selectable(label(i).c_str(),int(i)==selected_surface)){selected_surface=int(i);selected_tile=-1;}
        ImGui::EndCombo();
    }
    std::set<std::string> found;
    for(const auto& source:capture.surfaces)for(const auto& t:source.tiles)for(const auto& kv:t.stages_us)found.insert(kv.first);
    std::vector<std::string> metrics{"Render","Binning"};found.erase("Render");found.erase("Binning");
    metrics.insert(metrics.end(),found.begin(),found.end());
    metric=std::clamp(metric,0,int(metrics.size()-1));if(inline_stage)ImGui::SameLine();ImGui::SetNextItemWidth(cyui::dp(160));
    if(ImGui::BeginCombo("##stage",metrics[size_t(metric)].c_str())){
        for(size_t i=0;i<metrics.size();++i)if(ImGui::Selectable(metrics[i].c_str(),metric==int(i)))metric=int(i);
        ImGui::EndCombo();
    }
    const auto& s=capture.surfaces[size_t(selected_surface)];
    double maximum=0;size_t observed=0;for(const auto& t:s.tiles){auto v=t.Cost(metrics[metric]);if(std::isfinite(v)){maximum=std::max(maximum,v);++observed;}}
    cyui::textDim("%zu / %zu tiles with %s measurements | scale: 0 - %.1f us",observed,s.tiles.size(),metrics[size_t(metric)].c_str(),maximum);
    const auto& p=cyui::palette();const ImVec2 origin=ImGui::GetCursorScreenPos();
    const float available=std::max(1.f,ImGui::GetContentRegionAvail().x);
    const float maximum_height=std::clamp(ImGui::GetContentRegionAvail().y-cyui::dp(110),cyui::dp(96),cyui::dp(430));
    const float width=std::min(available,maximum_height*float(s.width)/float(s.height)),height=width*float(s.height)/float(s.width);
    auto* d=ImGui::GetWindowDrawList();d->AddRectFilled(origin,ImVec2(origin.x+width,origin.y+height),p.bg0);
    if(image)d->AddImage(image,origin,ImVec2(origin.x+width,origin.y+height));
    ImGui::InvisibleButton("##measured_tiles",ImVec2(width,height));const bool hovered=ImGui::IsItemHovered();const auto mouse=ImGui::GetIO().MousePos;
    int hit=-1;
    for(size_t i=0;i<s.tiles.size();++i) {
        const auto& t=s.tiles[i];ImVec2 a(origin.x+width*t.x/s.width,origin.y+height*t.y/s.height),b(origin.x+width*(t.x+t.width)/s.width,origin.y+height*(t.y+t.height)/s.height);
        double cost=t.Cost(metrics[metric]);float normalized=maximum>0&&std::isfinite(cost)?float(cost/maximum):0;
        auto color=std::isfinite(cost)?cyui::mix(p.success,p.error,normalized):p.bg3;
        d->AddRectFilled(a,b,image?cyui::alpha(color,opacity):color);d->AddRect(a,b,p.line);
        if(std::isfinite(cost)&&b.x-a.x>cyui::dp(58)&&b.y-a.y>cyui::dp(24)){
            char value[48];std::snprintf(value,sizeof(value),"%.1f us",cost);
            d->AddText(ImVec2(a.x+cyui::dp(5),a.y+cyui::dp(4)),IM_COL32(12,18,25,255),value);
        }
        if(hovered&&mouse.x>=a.x&&mouse.x<b.x&&mouse.y>=a.y&&mouse.y<b.y)hit=int(i);
        if(int(i)==selected_tile)d->AddRect(a,b,p.accent,0,0,cyui::dp(3));
    }
    if(hit>=0&&ImGui::IsMouseClicked(0))selected_tile=hit;
    const int inspect=hit>=0?hit:selected_tile;
    if(inspect>=0&&size_t(inspect)<s.tiles.size()) {
        const auto& t=s.tiles[size_t(inspect)];
        cyui::textMono("Tile %u | x=%u y=%u | %ux%u",t.id,t.x,t.y,t.width,t.height);
        if(ImGui::BeginTable("##tile_stages",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)){
            ImGui::TableSetupColumn("Measured stage");ImGui::TableSetupColumn("Duration (us)");ImGui::TableHeadersRow();
            for(const auto& kv:t.stages_us){ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(kv.first.c_str());ImGui::TableNextColumn();ImGui::Text("%.3f",kv.second);}
            ImGui::EndTable();
        }
    }
    else cyui::textDim("Select a tile to inspect its stages. The largest recorded surface is selected initially.");
    cyui::textFaint("Gray = unavailable. Colors are relative to this surface and selected stage; they are not shader or actor attribution.");
}
}
