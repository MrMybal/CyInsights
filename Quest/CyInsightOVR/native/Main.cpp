// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../../../App/OvrHeatmapView.hpp"
#include <imgui_impl_opengl3.h>
#include <GLES3/gl3.h>
#include <jni.h>
#include <android/log.h>
#include <chrono>
#include <deque>
#include <mutex>
#include <memory>
#include <set>
#include <string>

namespace {
using J=cygi::Json;
struct Point {double time,value;};
struct Metric {double time=0,value=NAN;std::deque<Point> history;};
std::map<std::string,Metric> metrics;
J state;cyovr::Capture capture;std::string capture_id,error,command;
std::mutex command_mutex;
std::mutex snapshot_mutex;
std::vector<J> pending_snapshots;
std::unique_ptr<cyovr::Capture> pending_capture;
std::string pending_capture_error;
double snapshot_prepare_ms=0;
int width=1280,height=720,page=0,surface=0,tile=-1,stage=0;
int hud_size=0,hud_corner=0;
ImVec2 hud_button_center(-1,-1);
ImVec2 capture_button_center(-1,-1);
double previous=0;bool initialized=false;
bool auto_open_capture=true;
unsigned rendered_frames=0;
double Now(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
void Action(const char* name){std::lock_guard<std::mutex> lock(command_mutex);if(command.empty())command=name;}
std::string Utf(JNIEnv* env,jstring s){if(!s)return {};const char* p=env->GetStringUTFChars(s,nullptr);std::string out=p;env->ReleaseStringUTFChars(s,p);return out;}
double Seconds(const J& j){try{return double(std::stoull(j.AsString()))/1e9;}catch(...){return 0;}}
void ApplyPreparedSnapshots(){
    std::vector<J> updates;std::unique_ptr<cyovr::Capture> ready;std::string failure;
    {std::lock_guard<std::mutex> lock(snapshot_mutex);updates.swap(pending_snapshots);ready=std::move(pending_capture);failure.swap(pending_capture_error);}
    for(auto& update:updates){
        for(const auto& sample:update["history"].Items()){
            auto& metric=metrics[sample["name"].AsString()];double time=Seconds(sample["observed_mono_ns"]);if(time<=metric.time)continue;
            metric.time=time;metric.value=sample["value"].AsNumber(NAN);metric.history.push_back({time,metric.value});while(metric.history.size()>600)metric.history.pop_front();
        }state=std::move(update);
    }
    if(ready&&ready->id!=capture_id){capture=std::move(*ready);capture_id=capture.id;surface=cyovr::LargestSurface(capture);tile=-1;error.clear();if(auto_open_capture)page=1;}
    if(!failure.empty())error=std::move(failure);
}
void Curve(const char* name,const char* title,double multiplier,const char* unit,double ceiling) {
    cyui::beginCard(name,title,cyui::dp(160));
    auto f=metrics.find(name);double time=Now();const bool fresh=f!=metrics.end()&&time-f->second.time<3;
    if(fresh&&std::isfinite(f->second.value)) {char text[120];std::snprintf(text,sizeof(text),"%.1f %s",f->second.value*multiplier,unit);cyui::heading(text,2);}
    else cyui::textDim("Unavailable / stale");
    const auto& p=cyui::palette();auto a=ImGui::GetCursorScreenPos();ImVec2 size(ImGui::GetContentRegionAvail().x,cyui::dp(68));auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(a,ImVec2(a.x+size.x,a.y+size.y),p.bg0);ImGui::Dummy(size);
    if(f!=metrics.end()) {
        const auto& points=f->second.history;double max=ceiling;for(const auto& v:points)if(v.time>=time-120&&std::isfinite(v.value))max=std::max(max,v.value*multiplier);
        ImVec2 last;bool have=false;double last_time=0;
        for(const auto& v:points) {
            if(v.time<time-120)continue;
            if(!std::isfinite(v.value)){have=false;continue;}
            ImVec2 point(a.x+float((v.time-(time-120))/120)*size.x,a.y+size.y-float(v.value*multiplier/std::max(1.,max))*size.y);
            if(have&&v.time-last_time<3)draw->AddLine(last,point,p.accent,cyui::dp(1.5f));
            last=point;last_time=v.time;have=true;
        }
    }
    cyui::textFaint("System scope | 120 s history | source updates ~1 Hz");cyui::endCard();
}
#include "DisplaySettings.hpp"
void Draw() {
    ImGui::SetNextWindowPos(ImVec2(0,0));ImGui::SetNextWindowSize(ImVec2(float(width),float(height)));
    ImGui::Begin("CyInsightOVR",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    cyui::heading("CyInsightOVR",1);ImGui::SameLine();cyui::badge("QUEST GPU DIAGNOSTICS",cyui::Tone::accent);
    cyui::textDim("Native companion for CyInsights | %s",state["target_package"].AsString().c_str());
    const bool connected=state["bridge"].AsBool();
    cyui::badge(connected?"Developer collector connected":"Collector unavailable",connected?cyui::Tone::success:cyui::Tone::warning);
    ImGui::SameLine();if(cyui::button("Target package"))Action("target");
    ImGui::SameLine();if(cyui::primaryButton("Capture GPU tiles in 5 s"))Action("capture");
    const auto capture_a=ImGui::GetItemRectMin(),capture_b=ImGui::GetItemRectMax();
    capture_button_center=ImVec2((capture_a.x+capture_b.x)*.5f,(capture_a.y+capture_b.y)*.5f);
    ImGui::SameLine();if(cyui::button("Export .cyovr"))Action("export");ImGui::SameLine();if(cyui::button("Quick access"))Action("quick_access");
    ImGui::SameLine();
    if(cyui::button("Stop HUD"))Action("overlay_stop");
    const char* pages[]={"Overview","GPU heatmap","Display & HUD","Heatmap overlay","Connection"};if(cyui::segmented("##pages",pages,5,&page,-1))auto_open_capture=false;
    const float footer=ImGui::GetFrameHeightWithSpacing()+cyui::dp(6);
    ImGui::BeginChild("##page_content",ImVec2(0,std::max(cyui::dp(80),ImGui::GetContentRegionAvail().y-footer)),ImGuiChildFlags_None,ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if(previous_page!=page){ImGui::SetScrollY(0);pending_scroll=0;previous_page=page;}
    if(pending_scroll!=0){ImGui::SetScrollY(std::clamp(ImGui::GetScrollY()+pending_scroll,0.f,ImGui::GetScrollMaxY()));pending_scroll=0;}
    if(state["capturing"].AsBool())cyui::progressBar(-1,ImVec2(-1,cyui::dp(18)),"Capturing measured GPU stages...");
    if(!error.empty())cyui::callout(cyui::Tone::error,error.c_str());
    if(page==0) {
        if(!connected)cyui::callout(cyui::Tone::warning,state["status"].AsString().c_str());
        if(chart_mask==0){cyui::emptyState("No panel graphs selected","Choose your curves in Display & HUD.");if(cyui::button("Choose graphs"))page=2;}
        if(chart_mask&&ImGui::BeginTable("##graphs",ImGui::GetContentRegionAvail().x>cyui::dp(680)?2:1,ImGuiTableFlags_SizingStretchSame)) {
            for(unsigned i=0;i<std::size(charts);++i)if(chart_mask&(1u<<i)){const auto& c=charts[i];ImGui::TableNextColumn();Curve(c.name,c.title,c.multiplier,c.unit,c.ceiling);}
            ImGui::EndTable();
        }
        cyui::textDim("%s",state["status"].AsString().c_str());
        cyui::callout(cyui::Tone::info,"Counters describe the GPU system. Missing or stale data is not zero. These are not per-frame game timings.");
    }else if(page==1) {
        if(!capture.id.empty())cyui::textDim("Snapshot %s | target %s",capture.id.c_str(),capture.package.c_str());
        cyovr::Heatmap(capture,surface,tile,stage);
    }else if(page==2){DisplayControls();}else if(page==3){HeatmapControls();}else {
        cyui::beginCard("##panel_performance","Panel performance");
        const auto& performance=state["panel_performance"];
        if(performance["fps"].AsNumber()>0){cyui::textDim("%.1f fps / %.0f target | CPU draw %.2f ms | presentation wait %.2f ms",performance["fps"].AsNumber(),performance["target_fps"].AsNumber(),performance["draw_ms"].AsNumber(),performance["swap_wait_ms"].AsNumber());cyui::textDim("95th percentile frame interval: %.2f ms",performance["p95_frame_ms"].AsNumber());}
        else cyui::textDim("Waiting for panel frame measurements");cyui::endCard();
        cyui::beginCard("##access","Developer collection");
        ImGui::TextWrapped("Run the bundled Start-CyInsightOVR.ps1 through authorized ADB once after each headset reboot. The shell-owned collector then runs locally; USB and the PC are not needed to keep collecting. This APK does not grant itself shell access.");
        ImGui::TextWrapped("Detailed GPU tracing needs profiling enabled before the target creates its rendering context. Enabling it never restarts a game automatically.");
        if(cyui::button("Enable detailed profiling for selected target"))Action("enable_details");
        if(cyui::button("Test Android HUD (not validated in VR)"))Action("overlay_android");
        cyui::callout(cyui::Tone::warning,"Detailed profiling changes GPU scheduling and adds overhead. Use short snapshots for diagnosis and lighter recordings to measure final performance.");
        cyui::endCard();
        cyui::beginCard("##availability","Available / unavailable");
        if(cyui::button("Check this headset's XR overlay support"))Action("inspect_xr");
        const auto& xr=state["openxr_capabilities"];
        if(xr.IsObject()){
            if(xr["status"].AsString()=="measured"){
                cyui::textDim("Device runtime: %.0f extensions | independent overlay: %s",xr["extension_count"].AsNumber(),xr["overlay_supported"].AsBool()?"advertised, requires validation":"not advertised");
                cyui::textDim("Overlay render permission: %s | overlay host permission: %s",xr["android_overlay_render_permission"].AsString().c_str(),xr["vr_overlay_host_permission"].AsString().c_str());
            }else cyui::callout(cyui::Tone::warning,xr["reason"].AsString().c_str());
            if(ImGui::TreeNode("Advertised runtime extensions")){for(const auto& name:xr["extensions"].Items())ImGui::TextUnformatted(name.AsString().c_str());ImGui::TreePop();}
            cyui::textFaint("Read-only inquiry: no XR instance or session is created; the running game's rendering is not replaced.");
        }
        ImGui::TextWrapped("Available: measured system counter history; target-filtered surface and tile snapshots when the firmware supplies them; native .cyovr export and desktop heatmap reader.");
        ImGui::TextWrapped("Cross-game VR HUD: the installed Meta renderer. A custom HUD over unmodified games has no validated public compositor path on this Quest. Our Android overlay is an experimental 2D window and is not a working VR HUD. Live image-aligned heatmap and exact Unreal frame/pass/object or shader attribution remain unavailable.");
        cyui::textMono("Android boot: %s",state["android_boot_id"].AsString().c_str());cyui::endCard();
    }
    body_scroll=ImGui::GetScrollY();body_scroll_max=ImGui::GetScrollMaxY();ImGui::EndChild();
    if(cyui::button("Scroll up"))pending_scroll=-height*.55f;ImGui::SameLine();if(cyui::button("Scroll down"))pending_scroll=height*.55f;ImGui::SameLine();
    if(page==2){ImGui::BeginDisabled(hud_graphs.empty()&&hud_stats.empty());if(cyui::button("Start HUD"))StartSelectedHud();auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();hud_button_center=ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f);ImGui::EndDisabled();ImGui::SameLine();}
    if(page==3){ImGui::BeginDisabled(!state["live_tiles"]["layer_ready"].AsBool());if(cyui::primaryButton(state["live_tiles"]["active"].AsBool()?"Apply heatmap":"Start heatmap"))StartSelectedHeatmap();ImGui::EndDisabled();ImGui::SameLine();if(cyui::button("Stop heatmap"))Action("heatmap_stop");ImGui::SameLine();}
    cyui::textDim("Drag / stick / wheel | %.0f%%",body_scroll_max>0?body_scroll/body_scroll_max*100:0);
    cyui::renderNotifications();ImGui::End();
}
}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeInit(JNIEnv*,jclass) {
    if(initialized){ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext();}
    IMGUI_CHECKVERSION();ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
    cyui::Config config;config.themeKey="navy";config.density=cyui::Density::comfortable;config.dpiScale=1.2f;cyui::apply(config);cyui::loadFonts();
    ImGui_ImplOpenGL3_Init("#version 300 es");previous=Now();hud_button_center=ImVec2(-1,-1);initialized=true;
    __android_log_print(ANDROID_LOG_INFO,"CyInsightOVR","Native panel renderer initialized");
}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeResize(JNIEnv*,jclass,jint w,jint h){width=w;height=h;}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeShutdown(JNIEnv*,jclass){if(initialized){ImGui_ImplOpenGL3_Shutdown();ImGui::DestroyContext();initialized=false;}}
extern "C" JNIEXPORT jfloatArray JNICALL Java_org_cyinsights_ovr_MainActivity_nativeHudButtonForTest(JNIEnv* env,jclass){float point[]={initialized?hud_button_center.x:-1,initialized?hud_button_center.y:-1};auto result=env->NewFloatArray(2);env->SetFloatArrayRegion(result,0,2,point);return result;}
extern "C" JNIEXPORT jfloatArray JNICALL Java_org_cyinsights_ovr_MainActivity_nativeCaptureButtonForTest(JNIEnv* env,jclass){float point[]={initialized?capture_button_center.x:-1,initialized?capture_button_center.y:-1};auto result=env->NewFloatArray(2);env->SetFloatArrayRegion(result,0,2,point);return result;}
extern "C" JNIEXPORT jstring JNICALL Java_org_cyinsights_ovr_MainActivity_nativeCaptureReportForTest(JNIEnv* env,jclass){
    auto report=J::Object();report["capture_id"]=capture.id;report["status"]=capture.status;report["parse_error"]=error;
    report["surfaces"]=double(capture.surfaces.size());size_t count=0;for(const auto& s:capture.surfaces)count+=s.tiles.size();report["tiles"]=double(count);
    report["heatmap_page"]=page==1;return env->NewStringUTF(report.Write().c_str());
}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeTouch(JNIEnv*,jclass,jfloat x,jfloat y,jboolean down){if(initialized){ImGui::GetIO().AddMousePosEvent(x,y);ImGui::GetIO().AddMouseButtonEvent(0,down);}}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeHover(JNIEnv*,jclass,jfloat x,jfloat y){if(initialized)ImGui::GetIO().AddMousePosEvent(x,y);}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeWheel(JNIEnv*,jclass,jfloat x,jfloat y,jfloat horizontal,jfloat vertical){if(initialized){auto& io=ImGui::GetIO();io.AddMousePosEvent(x,y);io.AddMouseWheelEvent(horizontal,vertical);}}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeScroll(JNIEnv*,jclass,jfloat pixels){if(std::isfinite(pixels))pending_scroll+=std::clamp(float(pixels),-2000.f,2000.f);}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeSettings(JNIEnv* env,jclass,jstring text){
    J settings;if(!J::Parse(Utf(env,text),settings))return;
    chart_mask=settings["chart_mask"].AsUInt32(15)&63u;hud_size=int(std::clamp(settings["hud_size"].AsNumber(),0.,1.));hud_corner=int(std::clamp(settings["hud_corner"].AsNumber(),0.,8.));
    heatmap_mode=int(std::clamp(settings["heatmap_mode"].AsNumber(),0.,1.));heatmap_position=int(std::clamp(settings["heatmap_position"].AsNumber(8),0.,8.));
    heatmap_ceiling=float(std::clamp(settings["heatmap_ceiling_us"].AsNumber(500),10.,5000.));heatmap_hud_opacity=float(std::clamp(settings["heatmap_hud_opacity"].AsNumber(.25),.05,1.));heatmap_full_opacity=float(std::clamp(settings["heatmap_full_opacity"].AsNumber(.2),.05,1.));
    if(settings["graphs"].IsArray()){hud_graphs.clear();for(const auto& id:settings["graphs"].Items())if(hud_graphs.size()<6)hud_graphs.insert(id.AsString());}
    if(settings["stats"].IsArray()){hud_stats.clear();for(const auto& id:settings["stats"].Items())if(hud_stats.size()<8)hud_stats.insert(id.AsString());}
}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeSelectPage(JNIEnv*,jclass,jint value){page=std::clamp(int(value),0,4);auto_open_capture=false;}
extern "C" JNIEXPORT jstring JNICALL Java_org_cyinsights_ovr_MainActivity_nativeUiReportForTest(JNIEnv* env,jclass){
    auto out=DisplaySettings();out["page"]=page;out["scroll"]=double(body_scroll);out["scroll_max"]=double(body_scroll_max);out["choice_x"]=double(choice_center.x);out["choice_y"]=double(choice_center.y);out["rendered_frames"]=rendered_frames;out["renderer_initialized"]=initialized;out["width"]=width;out["height"]=height;{std::lock_guard<std::mutex> lock(snapshot_mutex);out["snapshot_prepare_ms"]=snapshot_prepare_ms;}return env->NewStringUTF(out.Write().c_str());
}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeDraw(JNIEnv*,jclass) {
    if(!initialized)return;ApplyPreparedSnapshots();++rendered_frames;double now=Now();auto& io=ImGui::GetIO();io.DisplaySize=ImVec2(float(width),float(height));io.DeltaTime=float(std::clamp(now-previous,.001,.1));previous=now;
    ImGui_ImplOpenGL3_NewFrame();ImGui::NewFrame();Draw();ImGui::Render();glViewport(0,0,width,height);auto bg=ImGui::ColorConvertU32ToFloat4(cyui::palette().bg0);glClearColor(bg.x,bg.y,bg.z,1);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
extern "C" JNIEXPORT jstring JNICALL Java_org_cyinsights_ovr_MainActivity_nativeCommand(JNIEnv* env,jclass){std::lock_guard<std::mutex> lock(command_mutex);auto text=command;command.clear();return env->NewStringUTF(text.c_str());}
extern "C" JNIEXPORT void JNICALL Java_org_cyinsights_ovr_MainActivity_nativeSnapshot(JNIEnv* env,jclass,jstring text) {
    const double start=Now();J incoming;
    if(!J::Parse(Utf(env,text),incoming)){std::lock_guard<std::mutex> lock(snapshot_mutex);pending_capture_error="Invalid collector JSON";return;}
    std::unique_ptr<cyovr::Capture> prepared;std::string failure;
    if(incoming["capture"].IsObject())try{prepared=std::make_unique<cyovr::Capture>(cyovr::ParseCapture(incoming["capture"]));}catch(const std::exception& e){failure=e.what();}
    // Large source JSON is destroyed on this worker, never on the render thread.
    auto compact=J::Object();for(const auto& field:incoming.Members())if(field.first!="capture")compact[field.first]=field.second;
    std::lock_guard<std::mutex> lock(snapshot_mutex);snapshot_prepare_ms=(Now()-start)*1000;
    if(prepared)pending_capture=std::move(prepared);if(!failure.empty())pending_capture_error=std::move(failure);
    if(pending_snapshots.size()>=4)pending_snapshots.erase(pending_snapshots.begin());pending_snapshots.push_back(std::move(compact));
}
