// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "AiResources.hpp"
#include "UiMetrics.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstring>
namespace cyinsights {
std::future<cygi::Json> Application::RunLocalAiTool(const std::string& operation,const cygi::Json& parameters){
    if(ai_catalog_.IsNull())ai_catalog_=EmbeddedAiCatalog();
    auto request=std::make_shared<AiRequest>();request->document["operation"]=operation;request->document["parameters"]=parameters;
    auto future=request->response.get_future();AiDispatch(request,false);return future;
}
cygi::Json Application::StandaloneAiState() const{
    auto result=cygi::Json::Object();result["embeddedOperations"]=double(EmbeddedAiCatalog()["operations"].Size());result["methodId"]=EmbeddedAiMethod()["id"];result["methodVersion"]=EmbeddedAiMethod()["version"];result["bridgeRunning"]=ai_rpc_.Running();return result;
}
void Application::PrepareAiTool(const std::string& operation){
    using J=cygi::Json;const auto& catalog=ai_catalog_["operations"].Items();
    for(size_t i=0;i<catalog.size();++i){if(catalog[i]["id"].AsString()!=operation)continue;ai_operation_=int(i);auto parameters=J::Object();const auto& schema=catalog[i]["inputSchema"];
        for(const auto& field:schema["properties"].Members())if(field.second.Has("default"))parameters[field.first]=field.second["default"];
        if(schema["properties"].Has("start")&&schema["properties"].Has("end")){parameters["start"]=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);parameters["end"]=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);}
        if(schema["properties"].Has("function")){auto function=timing_exact_name_;if(function.empty()&&selected_timer_>=0&&size_t(selected_timer_)<timing_model_.names.size())function=timing_model_.names[size_t(selected_timer_)];parameters["function"]=function;}
        if(schema["properties"].Has("counter")&&selected_counter_>=0&&size_t(selected_counter_)<counters_->series.size()){parameters["counter"]=double(selected_counter_);if(schema["properties"].Has("counterRevision"))parameters["counterRevision"]=std::to_string(counter_catalog_revision_);}
        if(schema["properties"].Has("ipv4"))parameters["ipv4"]=std::string(remote_ip_);
        if(schema["properties"].Has("targetFps"))parameters["targetFps"]=double(ai_fps_);
        if(schema["properties"].Has("taskId")&&selected_task_)parameters["taskId"]=std::to_string(selected_task_->id);
        for(const auto& required:schema["required"].Items())if(!parameters.Has(required.AsString())){auto key=required.AsString();auto type=schema["properties"][key]["type"].AsString();parameters[key]=type=="string"?J(""):type=="boolean"?J(false):J(0.);}
        auto content=parameters.Write(2);if(content.size()<sizeof(ai_parameters_))std::memcpy(ai_parameters_,content.c_str(),content.size()+1);
        ai_requested_tab_=2;return;
    }
}
void Application::AiMethodView(){
    const auto& method=EmbeddedAiMethod();auto text=[&](const std::string& key){return AiUiText(key,ai_french_);};
    ImGui::TextUnformatted(text("Guided trace analysis").c_str());
    ImGui::TextWrapped("%s",text("Methods and tools run inside CyInsights. CyAiCodex, Python and an MCP connection are not required for local analysis.").c_str());
    ImGui::TextWrapped("%s",text("Choose a step, then prepare a tool or open its native panel. Preparing a tool does not start a capture or execute a command.").c_str());
    auto content=method.Write(2);if(ImGui::Button(text("Copy analysis method").c_str()))ImGui::SetClipboardText(content.c_str());
    ImGui::SameLine();ImGui::TextDisabled("%s / v%.0f",method["id"].AsString().c_str(),method["version"].AsNumber());
    ImGui::Separator();
    if(ImGui::BeginChild("##method_workflow",ImVec2(0,0))){
        size_t index=0;
        for(const auto& step:method["workflow"].Items()){
            ImGui::PushID(int(index));auto title=std::to_string(index+1)+". "+text(step["step"].AsString());
            if(ImGui::CollapsingHeader(title.c_str(),index<2?ImGuiTreeNodeFlags_DefaultOpen:0)){
                ImGui::TextWrapped("%s",text(step["rule"].AsString()).c_str());
                if(ImGui::SmallButton(text("Open native panel").c_str())){
                    session_view_=false;
                    if(index==3)FocusGpu();else if(index==4)FocusFrameStats();else if(index==5)FocusActivities();else if(index==6)FocusTasks();else if(index==7){show_comparison_=true;ImGui::SetWindowFocus(WindowName("Capture comparison").c_str());}else if(index==8){ai_requested_tab_=0;}else if(index==0){PrepareAiTool("get_state");}else{show_timing_=true;ImGui::SetWindowFocus(WindowName("Timing").c_str());}
                }
                ImGui::TextDisabled("%s",text("Prepare tool").c_str());
                for(const auto& op:step["tools"].Items()){ImGui::PushID(op.AsString().c_str());if(ImGui::GetCursorPosX()+ImGui::CalcTextSize(op.AsString().c_str()).x+ImGui::GetStyle().FramePadding.x*2>ImGui::GetWindowContentRegionMax().x)ImGui::NewLine();if(ImGui::SmallButton(op.AsString().c_str()))PrepareAiTool(op.AsString());ImGui::SameLine();ImGui::PopID();}ImGui::NewLine();ImGui::Spacing();
            }
            ImGui::PopID();++index;
        }
        if(ImGui::CollapsingHeader(text("Capture procedure").c_str()))for(const auto& line:method["capture"].Items())ImGui::BulletText("%s",text(line.AsString()).c_str());
        if(ImGui::CollapsingHeader(text("Evidence and unavailable data").c_str())){
            ImGui::TextWrapped("%s",text("Missing data stays unavailable. Probable causes and reconstructed GPU passes remain hypotheses or derived results.").c_str());
            for(const auto& line:method["boundaries"].Items())ImGui::BulletText("%s",text(line.AsString()).c_str());
        }
        if(ImGui::CollapsingHeader(text("Advanced method JSON").c_str()))ImGui::InputTextMultiline("##analysis_method_json",const_cast<char*>(content.c_str()),content.size()+1,ImVec2(-1,240*UiScale()),ImGuiInputTextFlags_ReadOnly);
    }ImGui::EndChild();
}
}
