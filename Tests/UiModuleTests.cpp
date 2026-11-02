// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "UiPlotData.hpp"
#include "OvrHeatmapView.hpp"
#include "UiTheme.hpp"
#include <cyui/modules/Diagnostics.h>
#include <cyui/modules/Timeline.h>
#include <cyui/modules/Nodes.h>
#include <imgui_internal.h>
#include <cstdio>
#include <functional>

namespace {
int checks=0,failures=0;
void Check(bool condition,const char* message){++checks;if(!condition){++failures;std::fprintf(stderr,"FAIL: %s\n",message);}}
void Frame(const std::function<void()>& draw,ImVec2 mouse={-100,-100},int button=-1,bool down=false,float wheel=0){
    auto& io=ImGui::GetIO();io.AddMousePosEvent(mouse.x,mouse.y);
    if(button>=0)io.AddMouseButtonEvent(button,down);if(wheel)io.AddMouseWheelEvent(0,wheel);
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({1000,800});
    ImGui::Begin("Host",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetCursorScreenPos({20,20});draw();ImGui::Dummy({0,0});ImGui::End();ImGui::Render();
}
void CounterAdapters(){
    using namespace cyinsights;
    CounterSeries source;source.floating=true;
    source.samples={{0,2},{.1,1000},{.2,-7},{.3,3},{.4,NAN,0,false},{.5,4}};
    const auto data=PrepareCounterPlot(source,{{0,3,2,1,0},{5,5,5,5,1}});
    Check(data.available&&data.low==-7&&data.high==1000,"counter range retains dense positive and negative spikes");
    Check(data.line.size()==6&&data.line[1].y==1&&data.line[2].y==-.007,"dense bucket retains first, min, max, last in observation order");
    Check(std::isnan(data.line[4].y),"unavailable source observations break the plot line");
    Check(source.samples[1].real==1000&&source.samples[4].valid==false,"preparation never mutates native data or availability");
    source.floating=false;source.samples={{0,0,9007199254740993LL},{1,0,9007199254740992LL}};
    const auto integer=PrepareCounterPlot(source,{{0,1,1,0,0}});
    Check(integer.minimum==1&&integer.maximum==0&&source.samples[0].integer==9007199254740993LL,"integer extrema and data remain exact beyond double precision");
    source.frame_stats=true;source.frame_samples.resize(2);source.frame_samples[0].flags=source.frame_samples[1].flags=cyfs::FrameBounds;
    source.frame_samples[0].game_frame=3;source.frame_samples[1].game_frame=5;
    auto frames=PrepareCounterPlot(source,{{0,0,0,0,0},{1,1,1,1,1}});
    Check(frames.line.size()==3&&std::isnan(frames.line[1].y),"missing game frame breaks adjacent counter samples");
    source.frame_samples[1].game_frame=4;frames=PrepareCounterPlot(source,{{0,0,0,0,0},{1,1,1,1,1}});
    Check(frames.line.size()==2,"consecutive source frames remain connected");
    CounterSeries dense;dense.floating=true;std::vector<CounterBucket> buckets;
    for(size_t i=0;i<8192;++i){for(size_t j=0;j<4;++j)dense.samples.push_back({double(i*4+j),double(j)});buckets.push_back({i*4,i*4+3,i*4,i*4+3,int(i)});}
    const auto bounded=PrepareCounterPlot(dense,buckets);
    Check(bounded.line.size()<=65536&&bounded.dots.size()<=8192,"maximum visible counter dataset fits the common plot budget");
    ActivityWindow window;window.start=10;window.end=12;ActivityLaneWindow lane;lane.bins={{0},{3,-10,30,true}};
    const auto activity=PrepareActivityPlot(lane,window,true);
    Check(activity.size()==2&&activity[0].x==10.5&&activity[0].y==0&&activity[1].x==11.5&&activity[1].y==1,"activity bins keep source time, missing columns and signed memory range");
}
}
int main(){
    CounterAdapters();auto* context=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1000,800};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    cyui::apply();cyui::loadFonts();unsigned char* pixels=nullptr;int width=0,height=0;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);io.Fonts->SetTexID(1);
    cyovr::Capture capture;capture.id="test-only";cyovr::Surface surface;surface.id="fixture";surface.width=128;surface.height=64;
    surface.tiles={{1,0,0,32,64,{{"Render",100}}},{2,32,0,96,64,{}}};capture.surfaces={surface};
    cyovr::HeatmapState heatmap;int selected_surface=0,tile=-1,metric=0;
    auto drawHeatmap=[&]{cyovr::Heatmap(capture,selected_surface,tile,metric,heatmap);};Frame(drawHeatmap);Frame(drawHeatmap);
    const auto target=ImFloor(heatmap.image.canvas.view.screen({80,32},heatmap.bounds.min));Frame(drawHeatmap,target);Frame(drawHeatmap,target,0,true);Frame(drawHeatmap,target,0,false);
    Check(tile==1,"heatmap selects the measured irregular tile using the shared image projection");
    const auto before=heatmap.image.canvas.view.world(target,heatmap.bounds.min);const double zoom=heatmap.image.canvas.view.zoom;
    Frame(drawHeatmap,target,-1,false,1);const auto after=heatmap.image.canvas.view.world(target,heatmap.bounds.min);
    if(!(heatmap.image.canvas.view.zoom>zoom&&std::abs(before.x-after.x)<1e-4))std::fprintf(stderr,"zoom %.6f -> %.6f, anchor %.6f -> %.6f, target %.2f %.2f\n",zoom,heatmap.image.canvas.view.zoom,before.x,after.x,target.x,target.y);
    Check(heatmap.image.canvas.view.zoom>zoom&&std::abs(before.x-after.x)<1e-4,"heatmap zoom preserves the hovered surface pixel");
    cyui::InspectorState inspector;cyui::Property property;property.id=0x100000001;property.label="Source time";property.value=std::string("1.000000001");
    std::vector<cyui::PropertyEdit> edits;inspector.drafts[property.id]={std::string("preview"),property.value,true};
    Frame([&]{edits=cyui::inspector("Properties",inspector,{&property,1},1);});
    Check(edits.size()==1&&edits.front().edit.phase==cyui::EditPhase::cancel&&std::get<std::string>(property.value)=="1.000000001","external revision cancels a stale inspector draft without mutating the host");
    cyui::LogBuffer log;log.append(cyui::LogLevel::info,"first source observation");log.append(cyui::LogLevel::warning,"capture unavailable");cyui::LogState logState;logState.query="unavailable";
    Frame([&]{cyui::logView("Messages",logState,log,{800,200});});
    Check(logState.filtered.size()==1&&logState.rows[logState.filtered[0]].level==cyui::LogLevel::warning,"messages use the common cached filter and severity model");
    cyui::TimelineState timeline;timeline.view={0,1};std::vector<cyui::TimelineLane> lanes{{1,"Execution",60}};
    std::vector<cyui::TimelineClip> clips{{0x100000001,1,.1,.1,"Task",0,true}};cyui::TimelineOptions options;options.readOnly=true;cyui::TimelineResult result;
    auto drawTimeline=[&]{result=cyui::timeline("Lifecycle",timeline,lanes,clips,{},ImVec2(800,160),options);};
    Frame(drawTimeline);Frame(drawTimeline,{500,90});Frame(drawTimeline,{500,90},0,true);Frame(drawTimeline,{650,90});Frame(drawTimeline,{650,90},0,false);
    Check(timeline.selection.max>timeline.selection.min&&result.clips.phase==cyui::EditPhase::none&&clips[0].start==.1,"read-only task timeline still selects ranges and never edits recorded tasks");
    cyui::NodeState graph;std::vector<cyui::Node> nodes{{1,{0,0},"Derived pass",{{5,""}},{{6,""}},0}};cyui::NodeOptions nodeOptions;nodeOptions.readOnly=true;cyui::NodeResult nodeResult;
    auto drawNodes=[&]{nodeResult=cyui::nodeEditor("Passes",graph,nodes,{},ImVec2(800,180),nodeOptions);};
    Frame(drawNodes);Frame(drawNodes,{100,40});Frame(drawNodes,{100,40},0,true);Frame(drawNodes,{170,60});Frame(drawNodes,{170,60},0,false);
    Check(!graph.selection.empty()&&graph.selection.front()==1&&nodeResult.moved.phase==cyui::EditPhase::none&&nodes[0].position.x==0,"GPU graph permits inspection while preserving derived pass positions and identity");
    const auto original=cyui::palette();auto changed=original;changed.accent=cyui::rgb(0xabcdef);
    cyinsights::CommitUiPalette(changed);cyinsights::CommitUiPalette(changed);
    Check(cyui::palette().accent==changed.accent&&cyinsights::UndoUiPalette()&&cyui::palette().accent==original.accent&&!cyinsights::UndoUiPalette(),"one accepted palette change produces one undo transaction; identical redraws do not duplicate it");
    Check(cyinsights::RedoUiPalette()&&cyui::palette().accent==changed.accent,"palette redo reapplies the committed value");
    cyinsights::UndoUiPalette();changed.warning=cyui::rgb(0x123456);cyinsights::CommitUiPalette(changed);
    Check(!cyinsights::RedoUiPalette(),"a new committed palette edit invalidates the redo branch");
    for(const auto& theme:cyui::builtInThemes())for(auto density:{cyui::Density::compact,cyui::Density::comfortable})for(float dpi:{1.f,1.5f,2.f}){
        cyui::setTheme(theme.key.c_str());cyui::setDensity(density);cyui::applyDpi(dpi);
        Frame(drawHeatmap);Check(ImGui::GetDrawData()->TotalVtxCount>0&&std::abs(cyui::dp(10)-10*dpi)<.01f,"heatmap and inspector render at each theme/density/DPI with scaled geometry");
    }
    cyui::setTheme("navy");auto* second=ImGui::CreateContext();ImGui::SetCurrentContext(second);cyui::apply();cyui::setTheme("light-grey");cyui::setLocale("fr");ImGui::SetCurrentContext(context);
    Check(cyui::currentThemeKey()=="navy"&&cyui::locale()=="en","shared modules preserve context-local theme and locale");
    ImGui::DestroyContext(second);ImGui::SetCurrentContext(context);ImGui::DestroyContext();
    std::printf("%d checks, %d failures\n",checks,failures);return failures?1:0;
}
