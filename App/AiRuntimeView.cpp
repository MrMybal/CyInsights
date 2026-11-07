// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiRuntimeView.hpp"
#include "AiResources.hpp"
#include "AppPaths.hpp"
#include "UiMetrics.hpp"
#include <imgui.h>
#include <Windows.h>
#include <shellapi.h>
#include <algorithm>
namespace cyinsights {
std::filesystem::path AiSourceDirectory(){
    auto app=ExecutableDirectory();auto source=app/L"AI";
    return std::filesystem::is_regular_file(source/L"app/CyTool.json")?source:app.parent_path().parent_path()/L"AI";
}
ai_runtime::Setup& AiRuntimeSetup(){
    static ai_runtime::Setup setup(AiSourceDirectory(),UserDataDirectory()/L"AI"/L"Runtime");return setup;
}
bool DrawAiRuntimeSetup(bool french){
    using S=ai_runtime::State;auto& setup=AiRuntimeSetup();
    auto text=[&](const char* key){return AiUiText(key,french);};
    ImGui::SeparatorText(text("External MCP/SDK runtime").c_str());
    const char* state=setup.state==S::Ready?"MCP/SDK ready":setup.state==S::Checking?"Checking installation":setup.state==S::Installing?"Installing MCP/SDK":setup.state==S::Cancelled?"Installation cancelled":setup.state==S::Failed?"Installation needs attention":"MCP/SDK not installed";
    ImGui::TextUnformatted(text(state).c_str());
    const bool maintenance=setup.state==S::Ready;
    if(maintenance&&!ImGui::TreeNode((text("Runtime maintenance")+"###ai_runtime_maintenance").c_str()))return true;
    ImGui::TextWrapped("%s",text("Install the private Python runtime here. CyToolsCore, MCP and their dependencies are already included with the application.").c_str());
    if(setup.Busy()){
        const char* phases[]={"Checking installation","Downloading private Python","Extracting private Python","Installing included dependencies","Verifying the complete SDK","MCP/SDK ready"};
        auto phase=std::min(setup.phase.load(),5u);ImGui::TextUnformatted(text(phases[phase]).c_str());
        auto n=setup.received.load(),total=setup.total.load();
        float fraction=phase==1?(total?.1f+.35f*float(double(n)/double(total)):.1f):phase==2?.5f:phase==3?.65f:phase==4?.9f:0.f;
        ImGui::ProgressBar(fraction,ImVec2(-1,0));
        if(total)ImGui::Text("%.1f / %.1f MiB",double(n)/1048576,double(total)/1048576);
        if(ImGui::Button(text("Cancel setup").c_str()))setup.Cancel();
    }else{
        if(ImGui::Button(text(setup.state==S::Ready?"Repair MCP/SDK":"Install MCP/SDK").c_str()))setup.Install();
        ImGui::SameLine();if(ImGui::Button(text("Check installation").c_str()))setup.Check();
    }
    ImGui::TextWrapped("%s",text("Python 3.11 x64 is downloaded from Astral's official GitHub release and verified with a pinned SHA-256. Packages install offline. No administrator access or global Python installation is required.").c_str());
    ImGui::TextWrapped("%s: %s",text("Installation folder").c_str(),setup.storage.u8string().c_str());
    if(!setup.error.empty())ImGui::TextWrapped("%s",text(setup.error.c_str()).c_str());
    if(!setup.log.empty()){
        if(ImGui::Button(text("Copy installation log").c_str()))ImGui::SetClipboardText(setup.log_tail.c_str());
        ImGui::SameLine();if(ImGui::Button(text("Open installation log").c_str()))ShellExecuteW(nullptr,L"open",setup.log.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
        if(ImGui::TreeNode(text("Installation details").c_str())){
            ImGui::InputTextMultiline("##ai_runtime_log",const_cast<char*>(setup.log_tail.c_str()),setup.log_tail.size()+1,ImVec2(-1,150*UiScale()),ImGuiInputTextFlags_ReadOnly);ImGui::TreePop();}
    }
    if(maintenance)ImGui::TreePop();
    return setup.state==S::Ready;
}
}
