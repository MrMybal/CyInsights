// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Included inside Main.cpp's private UI namespace.
#pragma once
unsigned chart_mask=15|(3u<<6);
int curve_hz=10;
std::set<std::string> hud_graphs={"average_frame_rate","app_gpu_time_microseconds"},hud_stats;
float pending_scroll=0,body_scroll=0,body_scroll_max=0;
int previous_page=-1;
int hud_group=0;
int heatmap_mode=0,heatmap_position=8;float heatmap_ceiling=500,heatmap_hud_opacity=.25f,heatmap_full_opacity=.2f;
ImVec2 choice_center(-1,-1);
struct Chart {const char* name;const char* title;double multiplier;const char* unit;double ceiling;};
constexpr Chart charts[]={
    {"GPU % Bus Busy","GPU bus busy",1,"%",100},{"GPU Frequency","GPU frequency",1e-6,"MHz",600},
    {"% Time Shading Fragments","Fragment shading share",1,"%",100},{"% Time Shading Vertices","Vertex shading share",1,"%",100},
    {"% Texture Fetch Stall","Texture fetch stalls",1,"%",100},{"% Stalled on System Memory","System memory stalls",1,"%",100},
    {"Vulkan Known Draw Calls/s","Known submitted draw calls",1,"draw/s",1000},
    {"Vulkan Known Input Triangles/s","Known submitted input triangles",1e-6,"M triangles/s",10},
    {"Vulkan Indirect Commands/s","Indirect drawing commands",1,"commands/s",100},
    {"Vulkan Draw Calls/Presented Frame","Known draws / presented frame",1,"draw/frame",1000},
    {"Vulkan Known Triangles/Presented Frame","Known triangles / presented frame",1e-6,"M triangles/frame",1}};
J Ids(const std::set<std::string>& ids){auto a=J::Array();for(const auto& id:ids)a.Push(id);return a;}
J DisplaySettings(){auto out=J::Object();out["chart_mask"]=chart_mask;out["curve_hz"]=curve_hz;out["hud_size"]=hud_size;out["hud_corner"]=hud_corner;out["graphs"]=Ids(hud_graphs);out["stats"]=Ids(hud_stats);out["heatmap_mode"]=heatmap_mode;out["heatmap_position"]=heatmap_position;out["heatmap_ceiling_us"]=heatmap_ceiling;out["heatmap_hud_opacity"]=heatmap_hud_opacity;out["heatmap_full_opacity"]=heatmap_full_opacity;return out;}
void SaveDisplaySettings(){auto out=DisplaySettings();out["operation"]="panel_settings";Action(out.Write().c_str());}
void StartSelectedHud(){auto options=DisplaySettings();options["operation"]="overlay_meta_on";const char* names[]={"upper_left","upper_right","lower_left","lower_right","upper_center","center_left","center","center_right","lower_center"};options["size"]=hud_size==0?"small":"medium";options["corner"]=names[hud_corner];Action(options.Write().c_str());}
void StartSelectedHeatmap(){auto options=J::Object();options["operation"]=state["live_tiles"]["active"].AsBool()?"heatmap_update":"heatmap_start";options["mode"]=heatmap_mode==0?"hud":"fullscreen";options["position"]=heatmap_position;options["ceiling_us"]=heatmap_ceiling;options["opacity"]=heatmap_mode==0?heatmap_hud_opacity:heatmap_full_opacity;Action(options.Write().c_str());}
int HudGroup(const std::string& id){
    if(id.find("memory")!=std::string::npos||id.find("app_")==0&&id.find("_MB")!=std::string::npos)return 3;
    if(id.find("cpu_")==0)return 2;
    if(id.find("battery_")==0||id.find("eye_buffer_")==0||id=="foveation_level")return 4;
    return 1;
}
void DisplayControls(){
    bool changed=false;
    cyui::beginCard("##chart_settings","Panel graphs");
    const char* rates[]={"1 Hz","5 Hz","10 Hz","20 Hz"};const int values[]={1,5,10,20};int rate=0;for(int i=0;i<4;++i)if(values[i]==curve_hz)rate=i;
    if(ImGui::Combo("Curve updates",&rate,rates,4)){curve_hz=values[rate];changed=true;}
    ImGui::TextWrapped("Draw counters use measured submission windows at the selected rate. Meta system counters keep their own source cadence, usually 1 Hz. Faster polling does not invent source samples.");
    ImGui::TextWrapped("Choose the curves shown in Overview. These are sampled system counters; unavailable values stay marked as unavailable.");
    for(unsigned i=0;i<std::size(charts);++i){bool enabled=(chart_mask&(1u<<i))!=0;
        if(ImGui::Checkbox(charts[i].title,&enabled)){if(enabled)chart_mask|=1u<<i;else chart_mask&=~(1u<<i);changed=true;}
        if(i==0){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();choice_center=ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f);}
    }
    cyui::endCard();
    cyui::beginCard("##hud_settings","VR HUD content");
    ImGui::TextWrapped("Choose graphs and numerical values for Meta's VR HUD. This mode cannot display our tile heatmap.");
    const char* sizes[]={"Small","Medium"};const char* corners[]={"Top left","Top right","Bottom left","Bottom right","Top center","Center left","Center","Center right","Bottom center"};
    if(cyui::beginProperties("##hud_placement",cyui::dp(140))){
        cyui::property("Size");changed|=ImGui::Combo("##hud_size",&hud_size,sizes,2);
        cyui::property("Position");if(ImGui::BeginCombo("##hud_corner",corners[hud_corner])){for(int index:{0,4,1,5,6,7,2,8,3})if(ImGui::Selectable(corners[index],hud_corner==index)){hud_corner=index;changed=true;}ImGui::EndCombo();}
        cyui::endProperties();
    }
    const char* groups[]={"All","Frame & GPU","CPU","Memory","Device"};cyui::segmented("##hud_groups",groups,5,&hud_group,-1);
    const auto& catalog=state["hud_catalog"]["metrics"];
    if(!catalog.IsArray()){
        cyui::textDim("Connect the developer collector to load the headset's HUD metrics.");
    }else if(ImGui::BeginTable("##hud_metrics",3,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)){
        ImGui::TableSetupColumn("Metric",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("Graph",ImGuiTableColumnFlags_WidthFixed,cyui::dp(80));ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthFixed,cyui::dp(80));ImGui::TableHeadersRow();
        for(const auto& metric:catalog.Items()){
            const auto id=metric["id"].AsString();if(hud_group!=0&&HudGroup(id)!=hud_group)continue;bool available=metric["available"].AsBool();ImGui::PushID(id.c_str());ImGui::TableNextRow();
            ImGui::TableNextColumn();ImGui::TextUnformatted(metric["title"].AsString().c_str());if(!available)cyui::textFaint("Unavailable on this firmware");
            ImGui::BeginDisabled(!available);ImGui::TableNextColumn();bool graph=hud_graphs.count(id)>0;
            ImGui::BeginDisabled(!graph&&hud_graphs.size()>=6);if(ImGui::Checkbox("##graph",&graph)){if(graph)hud_graphs.insert(id);else hud_graphs.erase(id);changed=true;}ImGui::EndDisabled();
            ImGui::TableNextColumn();bool stat=hud_stats.count(id)>0;
            ImGui::BeginDisabled(!stat&&hud_stats.size()>=8);if(ImGui::Checkbox("##stat",&stat)){if(stat)hud_stats.insert(id);else hud_stats.erase(id);changed=true;}ImGui::EndDisabled();ImGui::EndDisabled();ImGui::PopID();
        }ImGui::EndTable();
    }
    cyui::textDim("%zu / 6 graphs | %zu / 8 values | changes saved automatically",hud_graphs.size(),hud_stats.size());
    ImGui::TextWrapped("Meta selections outside this list are preserved. Selecting values can also show those existing Meta statistics.");
    if(cyui::button("Reload available HUD metrics"))Action("hud_catalog");
    if(cyui::button("Basic: FPS + GPU time")){hud_graphs={"average_frame_rate","app_gpu_time_microseconds"};hud_stats.clear();changed=true;}
    ImGui::TextWrapped("Use Start HUD in the fixed bottom bar to apply this selection. Configuration stays open. Stop HUD restores the previous Meta settings.");
    cyui::endCard();
    if(changed)SaveDisplaySettings();
}
void DrawCounterControls(){
    const auto& counters=state["draw_counters"];cyui::beginCard("##draw_counters","Draw calls & input triangles");
    cyui::badge(counters["status"].AsString()=="measured"?"SUBMISSION COUNTERS":counters["active"].AsBool()?"WAITING FOR COUNTERS":"COUNTERS STOPPED",cyui::Tone::info);
    cyui::textDim("Target: %s | curves %d Hz",counters["active"].AsBool()?counters["target"].AsString().c_str():state["target_package"].AsString().c_str(),curve_hz);
    if(counters["active"].AsBool()&&counters["target"].AsString()!=state["target_package"].AsString())cyui::textFaint("Selected next target: %s",state["target_package"].AsString().c_str());
    ImGui::TextWrapped("%s",counters["reason"].AsString().c_str());
    if(counters["status"].AsString()=="measured"){
        cyui::textDim("Last %.0f ms: %.0f known draws | %.0f known input triangles | %.0f indirect commands",counters["interval_seconds"].AsNumber()*1000,counters["draws"].AsNumber(),counters["known_input_triangles"].AsNumber(),counters["indirect_commands"].AsNumber());
        cyui::textFaint("Measured source cadence: %.1f Hz",counters["source_hz"].AsNumber());
        if(!counters["draws_complete"].AsBool()||!counters["triangles_complete"].AsBool())cyui::callout(cyui::Tone::warning,"Partial totals. GPU-controlled counts, primitive restart and unknown topology are not included in known geometry.");
        if(!counters["per_presented_frame_available"].AsBool())cyui::textFaint("Per-frame values unavailable: no Vulkan presentation boundary in this window. XR queue submissions are not treated as frames.");
    }
    if(cyui::button("Prepare draw counter layer"))Action("draw_counters_prepare");ImGui::SameLine();
    if(cyui::button(counters["active"].AsBool()?"Apply counter frequency":"Start draw counters"))Action("draw_counters_start");ImGui::SameLine();
    if(cyui::button("Stop draw counters"))Action("draw_counters_stop");
    ImGui::TextWrapped("Requires a debuggable Vulkan target. Prepare once, then completely reopen that target manually. This does not enable detailed GPU profiling or restart the game. Change curve frequency in Display & HUD.");
    cyui::textFaint("Input geometry before culling; no visible-triangle or per-eye attribution. No GPU buffer readback.");cyui::endCard();
}
void HeatmapControls(){
    bool changed=false;const char* corners[]={"Top left","Top right","Bottom left","Bottom right","Top center","Center left","Center","Center right","Bottom center"};
    cyui::beginCard("##heatmap_status","Live tile heatmap - Development Vulkan");
    const auto& live=state["live_tiles"];bool fresh=live["active"].AsBool()&&live["tiles"].AsNumber()>0&&!live["sample_age_ms"].IsNull()&&live["sample_age_ms"].AsNumber()<3000;cyui::badge(fresh?"MEASURED TILES RECEIVED":live["active"].AsBool()?"WAITING FOR TARGET DATA":live["layer_ready"].AsBool()?"LAYER LOADED":"SETUP REQUIRED",fresh?cyui::Tone::success:cyui::Tone::info);
    ImGui::TextWrapped("Experimental renderer; visibility is not yet validated. An independent heatmap, in addition to the Meta statistics HUD. Requires a debuggable Vulkan target; external layers are not available on arbitrary commercial APKs.");
    cyui::textDim("Heatmap target: %s",live["prepared"].AsBool()?live["target"].AsString().c_str():state["target_package"].AsString().c_str());
    if(cyui::button("Share Quest view with CyInsights"))Action("share_view");ImGui::SameLine();if(cyui::button("Stop view sharing"))Action("stop_share_view");
    cyui::textFaint("Optional image sharing requests system consent. It does not capture audio or restart the game.");
    const char* modes[]={"Extra HUD","Fullscreen (approximate)"};
    if(cyui::beginProperties("##heatmap_placement",cyui::dp(140))){
        cyui::property("Mode");changed|=ImGui::Combo("##heatmap_mode",&heatmap_mode,modes,2);
        cyui::property("HUD position");if(ImGui::BeginCombo("##heatmap_position",corners[heatmap_position])){for(int index:{0,4,1,5,6,7,2,8,3})if(ImGui::Selectable(corners[index],heatmap_position==index)){heatmap_position=index;changed=true;}ImGui::EndCombo();}
        cyui::property("Opacity");float& opacity=heatmap_mode==0?heatmap_hud_opacity:heatmap_full_opacity;float percent=opacity*100;if(ImGui::SliderFloat("##heatmap_opacity",&percent,5.f,100.f,"%.0f%%")){opacity=percent/100;changed=true;}
        cyui::property("Red at");changed|=ImGui::SliderFloat("##heatmap_ceiling",&heatmap_ceiling,10.f,5000.f,"%.0f us");cyui::endProperties();
    }
    ImGui::TextWrapped("Measured Render time per tile: green = lower cost, red = selected limit, gray = missing. Source updates are sampled, not attributed to the current frame or a known eye. Fullscreen association is approximate.");
    cyui::textDim("%s",live["message"].AsString().c_str());
    if(live["active"].AsBool())cyui::textDim("%.2f updates/s | %.0f tiles | sample age %.0f ms",live["measured_updates_hz"].AsNumber(),live["tiles"].AsNumber(),live["sample_age_ms"].AsNumber());
    ImGui::BeginDisabled(live["active"].AsBool());if(cyui::button("1. Prepare target layer"))Action("heatmap_prepare");ImGui::EndDisabled();
    ImGui::TextWrapped("After preparation, close and reopen the target manually once. Starting either HUD leaves configuration open. No game is restarted automatically.");
    if(ImGui::TreeNode("Advanced setup")){ImGui::TextWrapped("Stop heatmap keeps the target ready for another start. Uninstall removes its setup and requires preparation and a manual game restart next time.");if(cyui::button("Uninstall target integration"))Action("heatmap_remove");ImGui::TreePop();}
    if(cyui::button("Transparent preset")){heatmap_hud_opacity=.25f;heatmap_full_opacity=.2f;changed=true;}
    cyui::textFaint("Fresh recordings stop receiving data after Stop. Cached command buffers can retain an older HUD. Detailed profiling adds overhead.");
    cyui::endCard();if(changed)SaveDisplaySettings();
}
