// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "../Core/OvrCapture.hpp"
#include <cyui/Core.h>
#include <cyui/modules/Images.h>
#include <cyui/modules/Inspector.h>
#include <imgui.h>
#include <cstdio>
#include <set>

namespace cyovr {
struct HeatmapState {
    cyui::ImageState image;
    cyui::InspectorState properties;
    std::string capture;
    int surface=-1;
    ImVec2 size{};
    cyui::Rect bounds;
};
// The same native widget is used in the Quest app and the desktop reader.
inline void Heatmap(const Capture& capture,int& selected_surface,int& selected_tile,int& metric,HeatmapState& state,ImTextureID image=0,float opacity=.35f) {
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
    state.bounds={origin,ImVec2(origin.x+width,origin.y+height)};
    if(state.capture!=capture.id||state.surface!=selected_surface||state.size.x!=width||state.size.y!=height){
        state.image.fitPending=true;state.capture=capture.id;state.surface=selected_surface;state.size={width,height};
    }
    const auto interaction=cyui::imageViewport("##measured_tiles",state.image,
        {image,int(s.width),int(s.height)},ImVec2(width,height));
    const auto& view=state.image.canvas.view;
    auto* d=ImGui::GetWindowDrawList();
    d->PushClipRect(origin,ImVec2(origin.x+width,origin.y+height),true);
    int hit=-1;
    for(size_t i=0;i<s.tiles.size();++i) {
        const auto& t=s.tiles[i];
        const auto a=view.screen({double(t.x),double(t.y)},origin),b=view.screen({double(t.x+t.width),double(t.y+t.height)},origin);
        double cost=t.Cost(metrics[metric]);float normalized=maximum>0&&std::isfinite(cost)?float(cost/maximum):0;
        auto color=std::isfinite(cost)?cyui::mix(p.success,p.error,normalized):p.bg3;
        d->AddRectFilled(a,b,image?cyui::alpha(color,opacity):color);d->AddRect(a,b,p.line);
        if(std::isfinite(cost)&&b.x-a.x>cyui::dp(58)&&b.y-a.y>cyui::dp(24)){
            char value[48];std::snprintf(value,sizeof(value),"%.1f us",cost);
            const auto rgb=cyui::vec(color);
            const auto text=rgb.x*.2126f+rgb.y*.7152f+rgb.z*.0722f>.5f?cyui::darker(p.text,.92f):cyui::brighter(p.text,.95f);
            d->AddText(ImVec2(a.x+cyui::dp(5),a.y+cyui::dp(4)),text,value);
        }
        if(interaction.pixelX>=int(t.x)&&interaction.pixelX<int(t.x+t.width)&&interaction.pixelY>=int(t.y)&&interaction.pixelY<int(t.y+t.height))hit=int(i);
        if(int(i)==selected_tile)d->AddRect(a,b,p.accent,0,0,cyui::dp(3));
    }
    d->PopClipRect();
    if(hit>=0&&interaction.clicked)selected_tile=hit;
    const int inspect=hit>=0?hit:selected_tile;
    if(inspect>=0&&size_t(inspect)<s.tiles.size()) {
        const auto& t=s.tiles[size_t(inspect)];
        cyui::textMono("Tile %u | x=%u y=%u | %ux%u",t.id,t.x,t.y,t.width,t.height);
        std::vector<cyui::Property> properties;
        cyui::ItemId id=1;
        for(const auto& kv:t.stages_us){cyui::Property property;property.id=id++;property.label=kv.first+" (us)";property.value=kv.second;property.readOnly=true;properties.push_back(std::move(property));}
        cyui::inspector("##tile_stages",state.properties,properties,(uint64_t(t.id)<<32)|uint32_t(selected_surface));
    }
    else cyui::textDim("Select a tile to inspect its stages. The largest recorded surface is selected initially.");
    cyui::textFaint("Gray = unavailable. Colors are relative to this surface and selected stage; they are not shader or actor attribution.");
    cyui::textFaint("Wheel: zoom | Middle drag: pan | Double click: fit surface");
}
}
