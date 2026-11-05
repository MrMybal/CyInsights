// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Included inside Main.cpp's private UI namespace.
#pragma once
unsigned chart_mask=15|(3u<<6);
int curve_hz=10;
std::set<std::string> hud_graphs={"average_frame_rate","app_gpu_time_microseconds"},hud_stats;
std::set<std::string> custom_graphs={"Vulkan Known Draw Calls/s","Vulkan Known Input Triangles/s"};
std::set<std::string> custom_values=custom_graphs;
int custom_position=8;float custom_opacity=.8f;
float pending_scroll=0,body_scroll=0,body_scroll_max=0;
int previous_page=-1;
int hud_group=0;
int heatmap_mode=0,heatmap_position=8;float heatmap_ceiling=500,heatmap_hud_opacity=.25f,heatmap_full_opacity=.2f;
ImVec2 choice_center(-1,-1);
ImVec2 draw_graph_center(-1,-1),draw_value_center(-1,-1);
ImVec2 ItemCenter(){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();return ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f);}
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
J DisplaySettings(){auto out=J::Object();out["chart_mask"]=chart_mask;out["curve_hz"]=curve_hz;out["hud_size"]=hud_size;out["hud_corner"]=hud_corner;out["graphs"]=Ids(hud_graphs);out["stats"]=Ids(hud_stats);out["custom_graph_names"]=Ids(custom_graphs);out["custom_value_names"]=Ids(custom_values);out["custom_position"]=custom_position;out["custom_opacity"]=custom_opacity;out["heatmap_mode"]=heatmap_mode;out["heatmap_position"]=heatmap_position;out["heatmap_ceiling_us"]=heatmap_ceiling;out["heatmap_hud_opacity"]=heatmap_hud_opacity;out["heatmap_full_opacity"]=heatmap_full_opacity;return out;}
J SelectedHudOptions();
void SaveDisplaySettings(bool applyHud=false){auto out=DisplaySettings();out["manual_locale"]=manual_locale;out["ui_theme"]=cyui::currentThemeKey();out["ui_density"]=cyui::density()==cyui::Density::comfortable?"comfortable":"compact";out["ui_locale"]=cyui::locale();out["operation"]="panel_settings";if(applyHud&&(state["curve_hud"]["active"].AsBool()||state["vr_hud"]["requested"].AsBool()))out["apply_hud"]=SelectedHudOptions();Action(out.Write().c_str());}
J SelectedHudOptions(){auto options=DisplaySettings();options["operation"]="display_hud_on";const char* names[]={"upper_left","upper_right","lower_left","lower_right","upper_center","center_left","center","center_right","lower_center"};options["size"]=hud_size==0?"small":"medium";options["corner"]=names[hud_corner];options["custom_graphs"]=J::Array();for(const auto& c:charts)if(custom_graphs.count(c.name)||custom_values.count(c.name)){auto spec=J::Object();spec["name"]=c.name;spec["title"]=c.title;spec["unit"]=c.unit;spec["scale"]=c.multiplier;spec["graph_max"]=c.ceiling;spec["graph"]=custom_graphs.count(c.name)>0;spec["value"]=custom_values.count(c.name)>0;options["custom_graphs"].Push(spec);}return options;}
void StartSelectedHud(){Action(SelectedHudOptions().Write().c_str());}
void StartSelectedHeatmap(){auto options=J::Object();options["operation"]=state["live_tiles"]["active"].AsBool()?"heatmap_update":"heatmap_start";options["mode"]=heatmap_mode==0?"hud":"fullscreen";options["position"]=heatmap_position;options["ceiling_us"]=heatmap_ceiling;options["opacity"]=heatmap_mode==0?heatmap_hud_opacity:heatmap_full_opacity;Action(options.Write().c_str());}
int HudGroup(const std::string& id){
    if(id.find("memory")!=std::string::npos||id.find("app_")==0&&id.find("_MB")!=std::string::npos)return 3;
    if(id.find("cpu_")==0)return 2;
    if(id.find("battery_")==0||id.find("eye_buffer_")==0||id=="foveation_level")return 4;
    return 1;
}
void DisplayControls(){
    bool changed=false;
    if(ImGui::CollapsingHeader("Appearance")){cyui::beginCard("##appearance",cyui::tr("Appearance").c_str());
    if(ImGui::BeginCombo(cyui::trLabel("Theme").c_str(),cyui::currentThemeKey().c_str())){
        for(const auto& theme:cyui::builtInThemes())if(ImGui::Selectable(theme.name.c_str(),cyui::currentThemeKey()==theme.key)){cyui::setTheme(theme.key.c_str());changed=true;}ImGui::EndCombo();
    }
    int density=cyui::density()==cyui::Density::comfortable?1:0;const char* densities[]={"Compact","Comfortable"};
    if(ImGui::Combo(cyui::trLabel("Density").c_str(),&density,densities,2)){cyui::setDensity(density?cyui::Density::comfortable:cyui::Density::compact);changed=true;}
    if(ImGui::BeginCombo(cyui::trLabel("Language").c_str(),cyui::locale().c_str())){
        for(const auto& locale:cyui::availableLocales())if(ImGui::Selectable(locale.name.c_str(),cyui::locale()==locale.code)){cyui::setLocale(locale.code.c_str());changed=true;}ImGui::EndCombo();
    }
    cyui::endCard();}
    cyui::beginCard("##hud_settings","Graphs & HUD");
    const char* rates[]={"1 Hz","5 Hz","10 Hz","20 Hz"};const int values[]={1,5,10,20};int rate=0;for(int i=0;i<4;++i)if(values[i]==curve_hz)rate=i;
    if(ImGui::Combo("Curve updates",&rate,rates,4)){curve_hz=values[rate];changed=true;}
    ImGui::TextWrapped("HUD graph and HUD value add a curve, text, or both to the same HUD as FPS. Changes apply automatically while the HUD is running. Overview only selects this app's charts.");
    cyui::textDim("Game: %s",state["target_package"].AsString().c_str());
    if(cyui::button("Choose game"))Action("target");
    const auto& own=state["curve_hud"];const auto& counters=state["draw_counters"];
    if(own["active"].AsBool()){
        bool registered=own["meta_publisher"]["defined_metrics"].AsNumber()>0;
        cyui::badge(registered?"CUSTOM METRICS REGISTERED IN HUD":"HUD METRIC REGISTRATION PENDING",registered?cyui::Tone::success:cyui::Tone::info);
        ImGui::TextWrapped("%s",own["reason"].AsString().c_str());
        if(own["target"].AsString()!=state["target_package"].AsString())cyui::callout(cyui::Tone::warning,"The active HUD belongs to a different game. Apply HUD to use the selected game.");
        if(!own["layer_ready"].AsBool())cyui::textFaint("Open the selected Development Vulkan game. After an app update, fully close and reopen that game once.");
        if(!own["meta_publisher"]["error"].AsString().empty())cyui::callout(cyui::Tone::error,own["meta_publisher"]["error"].AsString().c_str());
        if(counters["status"].AsString()=="measured")cyui::textDim("Counter source %.1f Hz | %.0f known draws | %.0f known input triangles in %.0f ms",counters["source_hz"].AsNumber(),counters["draws"].AsNumber(),counters["known_input_triangles"].AsNumber(),counters["interval_seconds"].AsNumber()*1000);
    }
    const char* sizes[]={"Small","Medium"};const char* corners[]={"Top left","Top right","Bottom left","Bottom right","Top center","Center left","Center","Center right","Bottom center"};
    if(ImGui::CollapsingHeader("HUD position and size")){if(cyui::beginProperties("##hud_placement",cyui::dp(160))){
        cyui::property("Size");changed|=ImGui::Combo("##hud_size",&hud_size,sizes,2);
        cyui::property("HUD position");if(ImGui::BeginCombo("##corner",corners[hud_corner])){for(int index:{0,4,1,5,6,7,2,8,3})if(ImGui::Selectable(corners[index],hud_corner==index)){hud_corner=index;changed=true;}ImGui::EndCombo();}
        cyui::endProperties();
    }}
    static cyui::TextFilter filter;cyui::filterBox("##metric_filter",filter,{"Find a metric"});
    const char* groups[]={"All","GPU / Draw","CPU","Memory","Device"};cyui::segmented("##hud_groups",groups,5,&hud_group,-1);
    const auto& catalog=state["hud_catalog"]["metrics"];
    if((!custom_graphs.empty()||!custom_values.empty())&&(!own["layer_ready"].AsBool()||state["live_tiles"]["target"].AsString()!=state["target_package"].AsString()))cyui::textFaint("Start HUD prepares the selected Vulkan game automatically; it can wait while the game is paused.");
    if(state["app_variant"].AsString()=="standard")cyui::textDim("Draw/triangle HUD curves require the Vulkan APK variant.");
    if(cyui::button("Add draw calls + triangles")){for(auto name:{"Vulkan Known Draw Calls/s","Vulkan Known Input Triangles/s"}){custom_graphs.insert(name);custom_values.insert(name);}changed=true;}ImGui::SameLine();
    if(cyui::button("All HUD graphs")){for(const auto& c:charts)custom_graphs.insert(c.name);for(const auto& m:catalog.Items())if(m["available"].AsBool())hud_graphs.insert(m["id"].AsString());changed=true;}ImGui::SameLine();
    if(cyui::button("Clear HUD")){custom_graphs.clear();custom_values.clear();hud_graphs.clear();hud_stats.clear();changed=true;}
    if(ImGui::BeginTable("##all_metrics",4,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_RowBg)){
        ImGui::TableSetupColumn("Metric",ImGuiTableColumnFlags_WidthStretch);ImGui::TableSetupColumn("HUD graph",ImGuiTableColumnFlags_WidthFixed,cyui::dp(100));ImGui::TableSetupColumn("HUD value",ImGuiTableColumnFlags_WidthFixed,cyui::dp(100));ImGui::TableSetupColumn("Overview",ImGuiTableColumnFlags_WidthFixed,cyui::dp(95));ImGui::TableHeadersRow();
        for(unsigned i=0;i<std::size(charts);++i){const auto& c=charts[i];std::string searchable=std::string(c.title)+" "+c.name+(i==6||i==9?" drawcall":"");if(hud_group>1||!filter.matches(searchable.c_str()))continue;ImGui::PushID(c.name);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(c.title);cyui::textFaint(i>=6?"Meta HUD | Vulkan collector":"Meta HUD | system GPU source");
            ImGui::TableNextColumn();bool graph=custom_graphs.count(c.name)>0;if(ImGui::Checkbox("##graph",&graph)){if(graph)custom_graphs.insert(c.name);else custom_graphs.erase(c.name);changed=true;}
            if(i==6)draw_graph_center=ItemCenter();
            ImGui::TableNextColumn();bool value=custom_values.count(c.name)>0;if(ImGui::Checkbox("##value",&value)){if(value)custom_values.insert(c.name);else custom_values.erase(c.name);changed=true;}
            if(i==6)draw_value_center=ItemCenter();
            ImGui::TableNextColumn();bool panel=(chart_mask&(1u<<i))!=0;if(ImGui::Checkbox("##panel",&panel)){if(panel)chart_mask|=1u<<i;else chart_mask&=~(1u<<i);changed=true;}
            if(i==0){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();choice_center=ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f);}ImGui::PopID();
        }
        for(const auto& metric:catalog.Items()){
            const auto id=metric["id"].AsString(),title=metric["title"].AsString();if(hud_group!=0&&HudGroup(id)!=hud_group||!filter.matches(title))continue;bool available=metric["available"].AsBool();ImGui::PushID(id.c_str());ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(title.c_str());cyui::textFaint(available?"Meta VR HUD":"Unavailable on this firmware");
            ImGui::TableNextColumn();bool graph=hud_graphs.count(id)>0;ImGui::BeginDisabled(!available&&!graph);if(ImGui::Checkbox("##graph",&graph)){if(graph)hud_graphs.insert(id);else hud_graphs.erase(id);changed=true;}ImGui::EndDisabled();
            ImGui::TableNextColumn();bool stat=hud_stats.count(id)>0;ImGui::BeginDisabled(!available&&!stat);if(ImGui::Checkbox("##stat",&stat)){if(stat)hud_stats.insert(id);else hud_stats.erase(id);changed=true;}ImGui::EndDisabled();
            ImGui::TableNextColumn();cyui::textFaint("Meta HUD only");ImGui::PopID();
        }ImGui::EndTable();
    }
    cyui::textDim("%zu HUD graphs | %zu HUD values | selections saved automatically",custom_graphs.size()+hud_graphs.size(),custom_values.size()+hud_stats.size());
    if(cyui::button("Reload available Meta metrics"))Action("hud_catalog");
    ImGui::TextWrapped("All statistics use the existing Meta HUD. Draw calls and input triangles require the collector in the selected Development Vulkan game. N/A means no fresh source value; no zero is sent in its place. Heatmap controls remain independent.");
    if(own["active"].AsBool())for(const auto& metric:own["metrics"].Items())cyui::textDim("%s: %s",metric["title"].AsString().c_str(),metric["reason"].AsString().c_str());
    cyui::endCard();
    if(changed)SaveDisplaySettings(true);
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
    if(!state["bridge"].AsBool()){cyui::callout(cyui::Tone::warning,"The on-device GPU service is stopped. Start it here; no PC is needed.");if(cyui::primaryButton("Start GPU service"))Action("gpu_service_start");}
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
