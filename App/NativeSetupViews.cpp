// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "UiMetrics.hpp"
namespace cyinsights {
void Application::SelectAnalyzer(const fs::path& path) {
    std::snprintf(exe_,sizeof(exe_),"%s",path.u8string().c_str());
    native_setup_.Prepare(path);setup_dismissed_=false;
}
void Application::NativeSetupTick() {
    native_setup_.Poll();
    if(!exe_[0]&&!native_setup_.selected.empty())std::snprintf(exe_,sizeof(exe_),"%s",native_setup_.selected.u8string().c_str());
    if(native_import_pending_&&!native_setup_.Busy()) {native_import_pending_=false;ImportNative();}
}
void Application::NativeSetupView() {
    const bool busy=native_setup_.Busy();
    const bool failed=native_setup_.state==NativeSetup::State::Failed;
    if((!busy&&!failed)||setup_dismissed_)return;
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+viewport->WorkSize.x*.5f,viewport->WorkPos.y+viewport->WorkSize.y*.25f),ImGuiCond_Appearing,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(640*UiScale(),0),ImGuiCond_Always);
    ImGui::Begin("Native trace reader setup",nullptr,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextWrapped("%s",native_setup_.message.c_str());
    if(!native_setup_.selected.empty())ImGui::TextWrapped("Unreal analyzer: %s",native_setup_.selected.u8string().c_str());
    if(busy) {
        ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),ImVec2(-1,18*UiScale()),"Working in background...");
        ImGui::Text("Elapsed: %.1f s | Reader: %u / 3",native_setup_.Elapsed(),native_setup_.phase.load());
        ImGui::TextWrapped("First launch: compiling our readers with your installed Unreal libraries. The engine is not rebuilt. Results are cached for subsequent launches.");
        if(!pending_capture_.empty())ImGui::TextWrapped("Queued capture: %s",pending_capture_.filename().u8string().c_str());
        if(ImGui::Button("Cancel preparation / use CSV")){native_setup_.Cancel();setup_dismissed_=true;}
    }else {
        if(ImGui::Button("Retry preparation"))SelectAnalyzer(fs::u8path(exe_));
        ImGui::SameLine();if(ImGui::Button("Continue with CSV"))setup_dismissed_=true;
    }
    if(!native_setup_.log.empty()) {
        ImGui::TextWrapped("Log: %s",native_setup_.log.u8string().c_str());
        if(ImGui::Button("Copy setup log"))ImGui::SetClipboardText(native_setup_.log_tail.c_str());
    }
    if(!native_setup_.log_tail.empty()) {
        ImGui::BeginChild("##setup-log",ImVec2(0,150*UiScale()),ImGuiChildFlags_Borders,ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(native_setup_.log_tail.c_str());
        if(busy)ImGui::SetScrollHereY(1);
        ImGui::EndChild();
    }
    ImGui::End();
}
}
