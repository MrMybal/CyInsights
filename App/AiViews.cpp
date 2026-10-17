// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "AiAnalysis.hpp"
#include "AiResources.hpp"
#include "UiMetrics.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <regex>
#include <stdexcept>
namespace cyinsights {
namespace {
using J=cygi::Json;
fs::path HostDirectory(){wchar_t name[32768]{};GetModuleFileNameW(nullptr,name,32768);return fs::path(name).parent_path();}
fs::path AiRoot(){auto root=HostDirectory()/"AI";return fs::exists(root/"app/CyTool.json")?root:HostDirectory().parent_path().parent_path()/"AI";}
J ReadAi(const fs::path& path){std::ifstream in(path);std::string text((std::istreambuf_iterator<char>(in)),{});J value;J::Parse(text,value);return value;}
void Validate(const J& descriptor,const J& p){
    auto& schema=descriptor["inputSchema"];if(!p.IsObject())throw std::runtime_error("InvalidParameters: parameters must be an object");
    for(auto& name:schema["required"].Items())if(!p.Has(name.AsString()))throw std::runtime_error("InvalidParameters: missing "+name.AsString());
    for(auto& entry:p.Members()){
        if(!schema["properties"].Has(entry.first))throw std::runtime_error("InvalidParameters: unknown field "+entry.first);
        auto& rule=schema["properties"][entry.first];auto& v=entry.second;auto type=rule["type"].AsString();
        bool correct=type=="object"?v.IsObject():type=="string"?v.GetType()==J::Type::string:type=="boolean"?v.GetType()==J::Type::boolean:v.GetType()==J::Type::number;
        if(!correct)throw std::runtime_error("InvalidParameters: wrong type for "+entry.first);
        if(type=="object"){J nested=J::Object();nested["inputSchema"]=rule;Validate(nested,v);}
        if(type=="string"){
            auto size=v.AsString().size();if((rule.Has("minLength")&&size<rule["minLength"].AsUInt())||(rule.Has("maxLength")&&size>rule["maxLength"].AsUInt()))throw std::runtime_error("InvalidParameters: invalid string length");
            if(rule.Has("pattern")&&!std::regex_match(v.AsString(),std::regex(rule["pattern"].AsString())))throw std::runtime_error("InvalidParameters: invalid "+entry.first);
        }else if(type!="boolean"&&type!="object"){
            auto n=v.AsNumber(NAN);if(!std::isfinite(n)||(type=="integer"&&std::floor(n)!=n)||(rule.Has("minimum")&&n<rule["minimum"].AsNumber())||(rule.Has("maximum")&&n>rule["maximum"].AsNumber()))throw std::runtime_error("InvalidParameters: invalid numeric value for "+entry.first);
        }
        if(rule.Has("enum")){bool found=false;for(auto& choice:rule["enum"].Items())found|=choice.Write(-1)==v.Write(-1);if(!found)throw std::runtime_error("InvalidParameters: invalid enum value");}
    }
    if(p.Has("start")&&p.Has("end")&&p["end"].AsNumber()<=p["start"].AsNumber())throw std::runtime_error("InvalidParameters: end must exceed start");
}
J RemoteJson(const RemoteSnapshot& remote){auto j=J::Object();j["state"]=RemoteStateName(remote.state);j["message"]=remote.message;j["phase"]=remote.phase;j["startupElapsed"]=remote.startup_elapsed;j["finishing"]=remote.finishing;j["diagnosticLog"]=remote.diagnostic_log.empty()?J():J(remote.diagnostic_log.u8string());j["sessionId"]=remote.session_id;j["bytes"]=std::to_string(remote.bytes);j["elapsed"]=remote.elapsed;j["archive"]=remote.archive.empty()?J():J(remote.archive.u8string());j["partialTrace"]=remote.partial_trace.empty()?J():J(remote.partial_trace.u8string());j["nativeLiveAnalysis"]=false;j["captureDefaults"]=remote.capture_defaults;j["captureCapabilities"]=remote.capture_capabilities;j["gpuMessage"]=remote.gpu_message;j["metaGpuMessage"]=remote.meta_message;j["captureOptions"]=remote.manifest["capture_options"];return j;}
J Failure(const std::exception& error){std::string text=error.what();auto split=text.find(':');return AiError(split==std::string::npos?(text=="Cancelled"?"Cancelled":"NativeError"):text.substr(0,split),split==std::string::npos?text:text.substr(split+1));}
}
void Application::EnableAi(bool control,bool capture){ai_control_=control;ai_capture_=capture;ai_rpc_.Start(ai_message_,document_id_);}
cygi::Json Application::AiState() const {
    auto j=J::Object();j["protocol"]="cyinsights-native/1";j["generation"]=std::to_string(ai_generation_);j["pid"]=std::to_string(GetCurrentProcessId());j["documentId"]=std::to_string(document_id_);
    j["trace"]=trace_.empty()?J():J(trace_.u8string());j["ready"]=native_.store&&!native_.Busy()&&load_error_.empty();j["busy"]=AnalysisBusy();j["message"]=message_;j["error"]=load_error_.empty()?J():J(load_error_);
    j["events"]=std::to_string(native_.store?native_.store->count:0);j["selection"]["start"]=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);j["selection"]["end"]=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);j["selection"]["explicit"]=range_start_!=range_end_;
    j["exactFunction"]=timing_exact_name_;j["permissions"]["appControl"]=ai_control_;j["permissions"]["remoteCapture"]=ai_capture_;
    j["tasks"]["loaded"]=tasks_store_!=nullptr;j["tasks"]["nativeCount"]=std::to_string(tasks_store_?tasks_store_->count:0);j["tasks"]["status"]=tasks_message_;
    if(native_.store){j["domain"]["start"]=native_.store->start;j["domain"]["end"]=native_.store->end;j["frames"]["count"]=std::to_string(native_.store->frames.size());j["frames"]["identity"]=native_.store->frames_native?"native":"derived_tick_candidate";
        bool gpu=false;for(auto& lane:native_.store->tracks)gpu|=lane.gpu;j["availability"]["gpuTiming"]=gpu;j["availability"]["nativeTaskTrace"]=tasks_store_&&tasks_store_->count>0;}
    j["counters"]["revision"]=std::to_string(counter_catalog_revision_);j["counters"]["loaded"]=!counter_reader_.Busy()&&std::any_of(counters_->series.begin(),counters_->series.end(),[](const auto& c){return !c.samples.empty();});j["counters"]["catalogueCount"]=std::to_string(counters_->series.size());j["counters"]["samplesRequested"]=counter_samples_requested_;j["counters"]["loading"]=counter_reader_.Busy();j["counters"]["status"]=counter_message_;
    j["activities"]["loaded"]=activity_store_!=nullptr;j["activities"]["loading"]=activity_reader_.Busy();j["activities"]["status"]=activity_message_;j["activities"]["events"]=std::to_string(activity_store_?activity_store_->count:0);
    j["comparison"]["loaded"]=comparison_b_!=nullptr;j["comparison"]["path"]=comparison_b_?J(comparison_b_->file.u8string()):J();j["comparison"]["loading"]=comparison_reader_.Busy();j["comparison"]["status"]=comparison_message_;j["tasks"]["loading"]=task_reader_.Busy();
    j["availability"]["vulkanPassContents"]=native_.vulkan_contents&&native_.vulkan_contents->count>0;j["availability"]["nativeLiveAnalysis"]=false;j["remote"]=RemoteJson(remote_.Snapshot());return j;
}
void Application::AiDispatch(const std::shared_ptr<AiRequest>& request,bool external){
    if(request->cancelled)return;
    try{
        auto op=request->document["operation"].AsString();auto p=request->document["parameters"];J descriptor;
        for(auto& candidate:ai_catalog_["operations"].Items())if(candidate["id"].AsString()==op){descriptor=candidate;break;}
        if(descriptor.IsNull())throw std::runtime_error("UnknownOperation: unknown native tool");
        if(p.IsObject())for(auto& property:descriptor["inputSchema"]["properties"].Members())if(!p.Has(property.first)&&property.second.Has("default"))p[property.first]=property.second["default"];
        Validate(descriptor,p);
        if(external)for(auto& permission:descriptor["permissions"].Items()){
            if(permission.AsString()=="appControl"&&!ai_control_)throw std::runtime_error("AccessDenied: enable AI application control in AI Inspector");
            if(permission.AsString()=="remoteCapture"&&!ai_capture_)throw std::runtime_error("AccessDenied: enable AI remote capture in AI Inspector");
        }
        if(AiExtendedDispatch(op,p,request))return;
        if(op=="query_vulkan_passes"){
            if(native_.Busy()||!pending_capture_.empty())throw std::runtime_error("Busy: wait for the native timing index");
            auto details=native_.vulkan_contents;auto store=native_.store;auto gen=ai_generation_;
            if(!store)throw std::runtime_error("NoTrace: open a trace first");
            uint64_t pass=0;auto identity=p["passId"].AsString();if(!identity.empty())try{pass=std::stoull(identity);}catch(...){throw std::runtime_error("InvalidParameters: invalid capture-local pass id");}
            const double selectedBegin=range_start_!=range_end_?std::min(range_start_,range_end_):view_start_,selectedEnd=range_start_!=range_end_?std::max(range_start_,range_end_):view_end_;
            const double a=p.Has("start")?p["start"].AsNumber():selectedBegin,b=p.Has("end")?p["end"].AsNumber():selectedEnd;if(b<=a)throw std::runtime_error("InvalidParameters: end must exceed start");const auto limit=size_t(p["limit"].AsUInt(20));const uint64_t offset=p["offset"].AsUInt();
            ai_work_.push_back({request,std::async(std::launch::async,[details,a,b,limit,pass,offset,request,gen]{auto result=J::Object();result["generation"]=std::to_string(gen);result["rows"]=J::Array();
                result["availability"]=details&&details->count?"measured_arguments":"unavailable";
                result["individualSubpassGpuCost"]=J();result["unrealGroupIdentity"]=J();result["range"]["start"]=a;result["range"]["end"]=b;result["range"]["unit"]="seconds";result["matchedSubpasses"]="0";result["offset"]=std::to_string(offset);result["hasMore"]=false;
                if(!details||!details->count){result["reason"]="No subpass contents recorded; update the plugin and make a new capture";return result;}
                uint64_t matched=0;for(const auto& r:details->Query(a,b,limit,matched,&request->cancelled,pass,offset)){auto row=J::Object();row["passId"]=std::to_string(r.pass);row["submissionId"]=std::to_string(r.submission);row["ordinal"]=int(r.ordinal);row["subpass"]=int(r.subpass);row["subpasses"]=int(r.subpasses);row["width"]=int(r.width);row["height"]=int(r.height);row["parentStart"]=r.begin;row["parentEnd"]=r.end;row["directDraws"]=std::to_string(r.direct);row["indexedDraws"]=std::to_string(r.indexed);row["indirectCalls"]=std::to_string(r.indirect);row["requestedIndirectCommands"]=std::to_string(r.indirect_commands);row["requestedVertices"]=std::to_string(r.vertices);row["requestedIndices"]=std::to_string(r.indices);row["directInstanceArguments"]=std::to_string(r.instances);row["secondaryCoverage"]=r.flags&1?"partial":"recorded";row["indirectArguments"]=r.flags&2?"unavailable":"not_used";result["rows"].Push(row);}
                result["matchedSubpasses"]=std::to_string(matched);result["offset"]=std::to_string(offset);result["hasMore"]=offset+result["rows"].Size()<matched;result["parentAlignment"]="approximate";result["quantityMeaning"]="Intercepted command arguments, not measured shader invocations";return result;
            }),gen});return;
        }
        if(AiAnalysisOperation(op)){
            if(native_.Busy()||!pending_capture_.empty())throw std::runtime_error("Busy: wait for the native timing index");
            auto store=native_.store;auto tasks=tasks_store_;auto gen=ai_generation_;
            if(!p.Has("start"))p["start"]=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);
            if(!p.Has("end"))p["end"]=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
            ai_work_.push_back({request,std::async(std::launch::async,[store,tasks,op,p,request,gen]{auto result=AnalyzeAi(store,tasks,op,p,&request->cancelled);result["generation"]=std::to_string(gen);return result;}),gen});return;
        }
        auto result=J::Object();result["generation"]=std::to_string(ai_generation_);
        if(op=="get_state")result=AiState();
        else if(op=="capture_status")result=RemoteJson(remote_.Snapshot());
        else if(op=="open_trace"){
            auto path=fs::u8path(p["path"].AsString());if(!path.is_absolute()||!fs::is_regular_file(path)||(path.extension()!=L".utrace"&&path.extension()!=L".cycapture"))throw std::runtime_error("InvalidParameters: expected an existing absolute .utrace/.cycapture path");
            if(!Open(path))throw std::runtime_error("Busy: current analysis has not finished");result["accepted"]=true;result["completion"]= "poll_get_state";
        }else if(op=="set_range"){
            auto a=p["start"].AsNumber(),b=p["end"].AsNumber();if(!native_.store)throw std::runtime_error("NoTrace: open a trace first");
            if(p["focus"].AsBool(true))FocusInterval(a,b,0,false);else{range_start_=a;range_end_=b;}result["accepted"]=true;
        }else if(op=="filter_function"){FilterFunction(p["function"].AsString());result["accepted"]=true;}
        else if(op=="show_histogram"){if(!native_.store)throw std::runtime_error("NoTrace: open a trace first");OpenFunctionHistogram(p["function"].AsString());result["accepted"]=true;}
        else if(op=="load_tasks"){if(!native_.store)throw std::runtime_error("NoTrace: open a trace first");FocusTasks();StartNativeTasks();result["accepted"]=task_reader_.Busy()||tasks_store_!=nullptr;result["status"]=tasks_message_;}
        else if(op=="publish_finding"){
            if(p["generation"].AsString()!=std::to_string(ai_generation_))throw std::runtime_error("StaleTrace: finding belongs to another trace generation");
            p["provenance"]="external_ai_hypothesis";ai_findings_.push_back(p);FocusAi();result["accepted"]=true;
        }else if(op=="capture_probe"||op=="capture_start"){
            RemoteOptions options;options.ipv4=p["ipv4"].AsString();options.control_port=static_cast<uint16_t>(p["port"].AsNumber(41855));options.seconds=p["seconds"].AsNumber(0);options.output_directory=HostDirectory()/"Captures";
            if(p.Has("capture_options")){std::string error;if(!ReadCaptureOptions(p["capture_options"],options.capture,error))throw std::runtime_error("InvalidParameters: "+error);options.override_capture_options=true;}
            if(p.Has("meta_gpu")){const auto& meta=p["meta_gpu"];options.meta_gpu.enabled=meta["enabled"].AsBool(true);options.meta_gpu.restart_game=meta["restart_game"].AsBool(false);options.meta_gpu.adb=fs::u8path(meta["adb_path"].AsString());options.meta_gpu.device=meta["device"].AsString();options.meta_gpu.package=meta["package"].AsString();}
            bool accepted=op=="capture_probe"?remote_.Probe(options):remote_.Start(options);if(!accepted)throw std::runtime_error("Busy: remote capture receiver is active");result["accepted"]=true;result["completion"]="poll_capture_status";result["outputDirectory"]=options.output_directory.u8string();
        }else if(op=="capture_gpu"){
            CaptureOptions options;std::string error;if(!ReadCaptureOptions(p["capture_options"],options,error))throw std::runtime_error("InvalidParameters: "+error);
            if(!remote_.RequestGpu(options))throw std::runtime_error("NoRecording: no app-owned recording or a GPU request is already pending");result["accepted"]=true;result["completion"]="poll_capture_status";
        }else if(op=="capture_stop"){
            auto state=remote_.Snapshot().state;if(state!=RemoteState::recording&&state!=RemoteState::starting)throw std::runtime_error("NoRecording: this app owns no active recording");remote_.Stop();result["accepted"]=true;
        }else throw std::runtime_error("UnknownOperation: no native handler registered");
        request->response.set_value(result);
    }catch(const std::exception& error){request->response.set_value(Failure(error));}
}
void Application::StartAiIssues(){
    if(!native_.store||native_.Busy()||ai_issues_task_.valid())return;
    ai_issues_a_=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);ai_issues_b_=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
    if(ai_issues_b_<=ai_issues_a_)return;auto store=native_.store;auto p=J::Object();p["start"]=ai_issues_a_;p["end"]=ai_issues_b_;p["targetFps"]=static_cast<double>(ai_fps_);p["limit"]=20.;ai_issues_cancel_=false;ai_issues_generation_=ai_generation_;
    ai_issues_task_=std::async(std::launch::async,[this,store,p]{return AnalyzeAi(store,nullptr,"detect_issues",p,&ai_issues_cancel_);});
}
void Application::AiTick(){
    if(ai_catalog_.IsNull()){
        ai_catalog_=EmbeddedAiCatalog();auto settings=ReadAi(HostDirectory()/"CyInsights-AI-options.json");ai_french_=settings["french"].AsBool();ai_auto_=settings["autoAnalysis"].AsBool(true);ai_fps_=static_cast<float>(settings["targetFps"].AsNumber(60));
    }
    for(auto& request:ai_rpc_.Take())AiDispatch(request);
    for(size_t i=0;i<ai_work_.size();){auto& work=ai_work_[i];if(work.result.wait_for(std::chrono::seconds(0))!=std::future_status::ready){++i;continue;}
        try{auto result=work.result.get();if(work.request)work.request->response.set_value(result);else ai_result_=result.Write(2);}catch(const std::exception& e){if(work.request)work.request->response.set_value(Failure(e));else ai_result_=Failure(e).Write(2);}ai_work_.erase(ai_work_.begin()+i);
    }
    if(ai_issues_task_.valid()&&ai_issues_task_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto result=ai_issues_task_.get();if(ai_issues_generation_==ai_generation_){ai_issues_=std::move(result);ai_message_="Analysis ready; causes remain hypotheses";}}catch(const std::exception& e){ai_message_=e.what();}}
    if(show_ai_&&ai_auto_&&native_.store&&!ImGui::IsMouseDown(0)){
        double a=range_start_==range_end_?view_start_:std::min(range_start_,range_end_),b=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
        if(a!=ai_issues_a_||b!=ai_issues_b_){if(ImGui::GetTime()-ai_selection_changed_>.35)StartAiIssues();}
    }else if(ImGui::IsMouseDown(0))ai_selection_changed_=ImGui::GetTime();
}
void Application::AiView(){
    if(focus_ai_){
        auto* timing=ImGui::FindWindowByName(WindowName("Timing").c_str());auto* ai=ImGui::FindWindowByName(WindowName("AI Inspector").c_str());
        if(timing&&timing->DockId&&(!ai||!ai->DockId||ai->DockId==timing->DockId)){
            auto* node=ImGui::DockBuilderGetNode(timing->DockId);if(node&&node->IsLeafNode()){
                auto* root=node;while(root->ParentNode)root=root->ParentNode;ImGuiID lower=0,upper=0;auto root_id=root->ID;
                ImGui::DockBuilderSplitNode(node->ID,ImGuiDir_Down,.4f,&lower,&upper);ImGui::DockBuilderDockWindow(WindowName("AI Inspector").c_str(),lower);ImGui::DockBuilderFinish(root_id);
            }
        }
    }
    if(!BeginView("AI Inspector",&show_ai_)){ImGui::End();return;}
    if(focus_ai_){auto* w=ImGui::GetCurrentWindow();if(w->DockNode&&w->DockNode->TabBar){w->DockNode->SelectedTabId=w->TabId;w->DockNode->TabBar->NextSelectedTabId=w->TabId;}ImGui::SetWindowFocus();focus_ai_=false;}
    auto text=[&](const char* en,const char* fr){return ai_french_?fr:en;};
    if(ImGui::BeginTabBar("ai_tabs")){
        if(ImGui::BeginTabItem(text("Findings","Observations"),nullptr,ai_requested_tab_==0?ImGuiTabItemFlags_SetSelected:0)){
            if(ai_requested_tab_==0)ai_requested_tab_=-1;
            ImGui::Checkbox(text("Analyze visible / selected interval","Analyser l'intervalle visible / selectionne"),&ai_auto_);ImGui::SameLine();ImGui::SetNextItemWidth(90*UiScale());if(ImGui::InputFloat(text("Target fps","FPS cible"),&ai_fps_)){ai_fps_=std::isfinite(ai_fps_)?std::clamp(ai_fps_,1.f,1000.f):60.f;ai_issues_a_=NAN;}
            ImGui::SameLine();if(ImGui::Button(text("Analyze now","Analyser")))StartAiIssues();
            if(ai_issues_task_.valid())ImGui::TextDisabled("Preparing native diagnostics...");ImGui::TextWrapped("%s",ai_message_.c_str());
            ImGui::TextWrapped("%s",text("Measured costs / derived interval unions. Probable causes are hypotheses. GPU absent = unavailable. Live selection assistance; native stream decoding during recording is unavailable.","Couts mesures / unions d'intervalles deduites. Les causes sont des hypotheses. GPU absent = indisponible. Assistance sur la selection ; decodage du flux pendant l'enregistrement indisponible."));
            auto& rows=ai_issues_["findings"];ImGui::Text("%s / %s %s",ai_issues_["overBudget"].AsString().c_str(),ai_issues_["examined"].AsString().c_str(),text("frame intervals over budget","intervalles hors budget"));
            if(ai_issues_.Has("range"))ImGui::Text("Result interval: %.6f - %.6f s",ai_issues_["range"]["start"].AsNumber(),ai_issues_["range"]["end"].AsNumber());
            if(ImGui::BeginTable("ai_slow_frames",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_ScrollY,ImVec2(0,180*UiScale()))){
                for(auto name:{"Frame","Duration ms","Probable cause","Action"})ImGui::TableSetupColumn(name);ImGui::TableHeadersRow();int index=0;
                for(auto& row:rows.Items()){ImGui::PushID(index++);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(row["frameIndex"].AsString().c_str());ImGui::TableNextColumn();ImGui::Text("%.3f",row["duration"].AsNumber()*1000);ImGui::TableNextColumn();ImGui::TextUnformatted(row["probableCause"].AsString().c_str());ImGui::TableNextColumn();if(ImGui::SmallButton(text("Show in Timing","Voir dans Timing")))FocusInterval(row["start"].AsNumber(),row["end"].AsNumber(),0,false);ImGui::PopID();}ImGui::EndTable();
            }
            ImGui::SeparatorText(text("External AI hypotheses","Hypotheses de l'IA externe"));int index=0;
            for(auto& finding:ai_findings_){ImGui::PushID(index++);ImGui::TextWrapped("%s",finding["title"].AsString().c_str());ImGui::TextWrapped("%s",finding["detail"].AsString().c_str());if(ImGui::SmallButton(text("Focus / filter","Cadrer / filtrer"))){FocusInterval(finding["start"].AsNumber(),finding["end"].AsNumber(),0,false);if(!finding["function"].AsString().empty())FilterFunction(finding["function"].AsString());}ImGui::PopID();}
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(text("Connection","Connexion"))){
            bool enabled=ai_rpc_.Running();if(ImGui::Checkbox(text("Enable local MCP/SDK bridge","Activer le pont local MCP/SDK"),&enabled)){if(enabled)ai_rpc_.Start(ai_message_,document_id_);else{ai_rpc_.Stop();for(auto& work:ai_work_)if(work.request)work.request->cancelled=true;}}
            ImGui::Checkbox(text("Allow AI application control","Autoriser le controle de l'application"),&ai_control_);ImGui::Checkbox(text("Allow AI remote capture","Autoriser les captures reseau"),&ai_capture_);
            ImGui::Text("Document: %llu",static_cast<unsigned long long>(document_id_));
            ImGui::Text("PID: %lu | CyToolsCore 0.10.1 / MCP 1.30.0",GetCurrentProcessId());ImGui::TextWrapped("%s",text("Local Windows-user named pipe. SDK jobs/reports are private per client. The opened application document and timeline are shared. No provider key or remote trace upload is required.","Pipe local reserve a l'utilisateur Windows. Jobs/rapports SDK prives par client ; document et timeline partages. Aucune cle fournisseur ni envoi distant de trace requis."));
            auto config=J::Object();auto& server=config["mcpServers"]["CyInsights"];server["command"]=(AiRoot()/"runtime/python/Scripts/python.exe").u8string();server["args"]=J::Array();server["args"].Push((AiRoot()/"app/tool.py").u8string());server["args"].Push("mcp");server["env"]["CYINSIGHTS_APP_PID"]=std::to_string(GetCurrentProcessId());server["env"]["CYINSIGHTS_DOCUMENT_ID"]=std::to_string(document_id_);auto display=config.Write(2);
            if(ImGui::Button(text("Copy MCP configuration","Copier la configuration MCP")))ImGui::SetClipboardText(display.c_str());ImGui::TextWrapped("%s",display.c_str());
            if(!fs::exists(AiRoot()/"runtime/python/Scripts/python.exe"))ImGui::TextWrapped("%s",AiUiText("The optional Python companion is needed only for external MCP clients. Local tools and methods remain available.",ai_french_).c_str());
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(text("Tools","Outils"),nullptr,ai_requested_tab_==2?ImGuiTabItemFlags_SetSelected:0)){
            if(ai_requested_tab_==2)ai_requested_tab_=-1;
            auto& catalog=ai_catalog_["operations"].Items();if(catalog.empty())ImGui::TextWrapped("Embedded tool catalogue unavailable");else{
                ai_operation_=std::clamp(ai_operation_,0,static_cast<int>(catalog.size())-1);
                if(ImGui::BeginCombo("Operation",catalog[ai_operation_]["id"].AsString().c_str())){for(size_t i=0;i<catalog.size();++i)if(ImGui::Selectable(catalog[i]["id"].AsString().c_str(),ai_operation_==static_cast<int>(i)))ai_operation_=static_cast<int>(i);ImGui::EndCombo();}
                auto& descriptor=catalog[ai_operation_];ImGui::TextWrapped("%s",descriptor["description"].AsString().c_str());
                if(ImGui::TreeNode("Input contract")){auto schema=descriptor["inputSchema"].Write(2);ImGui::TextWrapped("%s",schema.c_str());ImGui::TreePop();}
                ImGui::InputTextMultiline("Parameters JSON",ai_parameters_,sizeof(ai_parameters_),ImVec2(-1,95*UiScale()));
                if(ImGui::Button(text("Run native tool","Executer l'outil natif"))){auto request=std::make_shared<AiRequest>();request->document["operation"]=descriptor["id"];J p;if(!J::Parse(ai_parameters_,p))ai_result_="Invalid JSON";else{request->document["parameters"]=p;auto future=request->response.get_future();AiDispatch(request,false);ai_work_.push_back({nullptr,std::move(future),ai_generation_});}}
                ImGui::SameLine();if(ImGui::Button(text("Copy result","Copier le resultat")))ImGui::SetClipboardText(ai_result_.c_str());
                ImGui::SameLine();if(ImGui::Button(text("Save parameters","Sauver les parametres"))){J p;if(J::Parse(ai_parameters_,p)){auto recipe=J::Object();recipe["schemaVersion"]=1.;recipe["operation"]=descriptor["id"];recipe["parameters"]=p;auto file=ChooseFile(L"AI parameters\0*.json\0\0",true);if(!file.empty())std::ofstream(file)<<recipe.Write(2);}}
                ImGui::SameLine();if(ImGui::Button(text("Load parameters","Charger les parametres"))){auto file=ChooseFile(L"AI parameters\0*.json\0\0");if(!file.empty()){auto recipe=ReadAi(file);for(size_t i=0;i<catalog.size();++i)if(catalog[i]["id"].AsString()==recipe["operation"].AsString()){auto p=recipe["parameters"].Write(-1);if(p.size()<sizeof(ai_parameters_)){std::memcpy(ai_parameters_,p.c_str(),p.size()+1);ai_operation_=static_cast<int>(i);}break;}}}
                ImGui::InputTextMultiline("Result",const_cast<char*>(ai_result_.c_str()),ai_result_.size()+1,ImVec2(-1,160*UiScale()),ImGuiInputTextFlags_ReadOnly);
            }ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(text("AI method","Methode IA"),nullptr,ai_requested_tab_==3?ImGuiTabItemFlags_SetSelected:0)){
            if(ai_requested_tab_==3)ai_requested_tab_=-1;
            AiMethodView();
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Options")){
            ImGui::Checkbox("Francais",&ai_french_);ImGui::TextUnformatted("CyToolsCore: MIT - https://github.com/MrMybal/CyToolsCore");ImGui::TextUnformatted("CyInsights: AGPL-3.0-or-later | Dear ImGui: MIT | MCP SDK: MIT");
            ImGui::TextWrapped("No model inference/provider connection is embedded. An external MCP AI can use these tools and publish interval findings.");
            if(ImGui::Button(text("Save options","Sauver les options"))){auto p=J::Object();p["schemaVersion"]=1.;p["french"]=ai_french_;p["autoAnalysis"]=ai_auto_;p["targetFps"]=static_cast<double>(ai_fps_);std::ofstream(HostDirectory()/"CyInsights-AI-options.json")<<p.Write(2);}
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }ImGui::End();
}
}
