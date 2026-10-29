// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "DocumentWorkspace.hpp"
#include "UiMetrics.hpp"
#include <imgui_internal.h>
#include <fstream>
#include <algorithm>
#include <windows.h>
#include <shobjidl.h>
namespace cyinsights {
std::vector<fs::path> ChooseCaptures(){
    // The native shell dialog returns separate Unicode paths without a fixed
    // concatenated filename buffer, including multi-selection across folders.
    std::vector<fs::path> files;
    auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    IFileOpenDialog* dialog=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))){
        DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_ALLOWMULTISELECT|FOS_FORCEFILESYSTEM|FOS_FILEMUSTEXIST);
        COMDLG_FILTERSPEC filters[]={{L"Unreal / CyInsights captures",L"*.utrace;*.cycapture;*.pftrace;*.cyovr"}};
        dialog->SetFileTypes(1,filters);dialog->SetTitle(L"Open captures in separate tabs");
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))){
            IShellItemArray* items=nullptr;
            if(SUCCEEDED(dialog->GetResults(&items))){
                DWORD count=0;items->GetCount(&count);
                for(DWORD i=0;i<count;++i){IShellItem* item=nullptr;if(SUCCEEDED(items->GetItemAt(i,&item))){PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){files.emplace_back(path);CoTaskMemFree(path);}item->Release();}}
                items->Release();
            }
        }
        dialog->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();return files;
}
void Application::ConfigureDocument(uint64_t id,std::function<void(const fs::path&)> open){document_id_=id;open_document_=std::move(open);}
std::string Application::WindowName(const char* name) const {return document_id_?std::string(name)+"###CyTrace"+std::to_string(document_id_)+"/"+name:std::string(name);}
bool Application::BeginView(const char* name,bool* open,ImGuiWindowFlags flags){
    if(hosted_){ImGuiWindowClass cls;cls.ClassId=ImHashStr(("CyTracePanels"+std::to_string(document_id_)).c_str());cls.DockingAllowUnclassed=false;ImGui::SetNextWindowClass(&cls);}
    return ImGui::Begin(WindowName(name).c_str(),open,flags);
}
void Application::RequestOpen(const fs::path& path){if(open_document_)open_document_(path);else Open(path);}
void Application::ChooseAndOpenCaptures(){for(const auto& file:ChooseCaptures())RequestOpen(file);}
void Application::DrawHosted(bool visible){
    hosted_=true;document_visible_=visible;
    workspace_viewport_={ImGui::GetWindowViewport()->ID,ImGui::GetCursorScreenPos(),ImGui::GetContentRegionAvail()};
    Tick();pending_shown_=true;
    if(visible)Toolbar();
    ImGuiWindowClass cls;cls.ClassId=ImHashStr(("CyTracePanels"+std::to_string(document_id_)).c_str());cls.DockingAllowUnclassed=false;
    auto dock=ImGui::GetID("InsightsLayoutV2");document_dock_=dock;
    ImGui::DockSpace(dock,ImVec2(0,0),!visible||session_view_?ImGuiDockNodeFlags_KeepAliveOnly:0,&cls);
    if(visible&&(first_||reset_layout_)){Layout(dock);first_=reset_layout_=false;}
    if(visible&&session_view_){Session();if(show_ai_)AiView();LoadStatus();NativeSetupView();}
    else if(!session_view_&&!first_)DrawPanels(); // Floating panels remain usable even when their tab is hidden.
}
void Application::DrawInactive(){
    document_visible_=false;Tick();pending_shown_=true;
    if(document_dock_){
        ImGuiWindowClass cls;cls.ClassId=ImHashStr(("CyTracePanels"+std::to_string(document_id_)).c_str());cls.DockingAllowUnclassed=false;
        ImGui::DockSpace(document_dock_,ImVec2(0,0),ImGuiDockNodeFlags_KeepAliveOnly,&cls);
        bool floating=false;
        for(const auto* window:ImGui::GetCurrentContext()->Windows)
            if(window->WindowClass.ClassId==cls.ClassId&&window->WasActive&&!window->DockId&&!(window->Flags&ImGuiWindowFlags_NoDocking)){floating=true;break;}
        if(floating&&!session_view_)DrawPanels();
    }
}
DocumentWorkspace::DocumentWorkspace(ImTextureID logo):logo_(logo){Create();}
DocumentWorkspace::Document& DocumentWorkspace::Create(){
    Document doc;doc.id=next_id_++;doc.app=std::make_unique<Application>();doc.app->SetBrandLogo(logo_);
    doc.app->ConfigureDocument(doc.id,[this](const fs::path& file){pending_open_.push_back(file);});doc.app->FocusSession();
    documents_.push_back(std::move(doc));return documents_.back();
}
void DocumentWorkspace::Open(const fs::path& path){
    auto& doc=documents_.size()==1&&documents_.front().app->DocumentPath().empty()&&!documents_.front().app->AnalysisBusy()&&documents_.front().app->CanCloseDocument()?documents_.front():Create();
    if(doc.app->Open(path)){selected_=menu_owner_=doc.id;select_pending_=true;}
}
bool DocumentWorkspace::Busy() const {for(const auto& doc:documents_)if(doc.app->AnalysisBusy())return true;return false;}
std::string DocumentWorkspace::Title(const Document& doc) const {return (doc.app->DocumentPath().empty()?"Capture session":doc.app->DocumentPath().filename().u8string())+"###CyDocument"+std::to_string(doc.id);}
void DocumentWorkspace::Detach(size_t i){if(i>=documents_.size())return;documents_[i].detached=true;documents_[i].position=true;}
void DocumentWorkspace::Attach(size_t i){if(i>=documents_.size())return;documents_[i].detached=false;selected_=menu_owner_=documents_[i].id;select_pending_=true;}
void DocumentWorkspace::Select(size_t i){if(i<documents_.size()){selected_=menu_owner_=documents_[i].id;select_pending_=true;}}
void DocumentWorkspace::Close(size_t i){
    if(i>=documents_.size()||!documents_[i].app->CanCloseDocument())return;
    auto retired=std::move(documents_[i].app);documents_.erase(documents_.begin()+i);
    // Destruction cancels and joins readers away from the UI thread.
    closing_.push_back(std::async(std::launch::async,[retired=std::move(retired)]() mutable {retired.reset();}));
    if(documents_.empty())Create();
    if(std::none_of(documents_.begin(),documents_.end(),[this](const auto& d){return d.id==selected_&&!d.detached;}))for(const auto& d:documents_)if(!d.detached){selected_=d.id;break;}
    menu_owner_=selected_;
    select_pending_=true;
}
void DocumentWorkspace::Draw(){
    for(auto it=closing_.begin();it!=closing_.end();)if(it->wait_for(std::chrono::seconds(0))==std::future_status::ready){it->get();it=closing_.erase(it);}else ++it;
    auto pending=std::move(pending_open_);pending_open_.clear();for(const auto& path:pending)Open(path);
    auto owner=std::find_if(documents_.begin(),documents_.end(),[this](const auto& d){return d.id==menu_owner_;});
    if(owner==documents_.end())owner=documents_.begin();owner->app->DrawMenuBar();
    if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_O))for(const auto& path:ChooseCaptures())pending_open_.push_back(path);
    const auto* main=ImGui::GetMainViewport();
    const float header=ImGui::GetFrameHeight()*2+ImGui::GetStyle().ItemSpacing.y+ImGui::GetStyle().WindowPadding.y*2;
    cyui::beginTopBar("##CaptureDocuments",header);
    // TopBar centers a single row by default; this strip also contains tabs.
    ImGui::SetCursorPosY(cyui::dp(5));
    size_t close=SIZE_MAX;bool has_embedded=false;
    if(ImGui::BeginTabBar("Capture tabs",ImGuiTabBarFlags_Reorderable|ImGuiTabBarFlags_AutoSelectNewTabs|ImGuiTabBarFlags_FittingPolicyScroll)){
        for(size_t i=0;i<documents_.size();++i){auto& doc=documents_[i];if(doc.detached)continue;has_embedded=true;bool open=true;
            bool chosen=ImGui::BeginTabItem(Title(doc).c_str(),doc.app->CanCloseDocument()?&open:nullptr,select_pending_&&selected_==doc.id?ImGuiTabItemFlags_SetSelected:0);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",doc.app->DocumentPath().empty()?"Capture session":doc.app->DocumentPath().u8string().c_str());
            if(ImGui::BeginPopupContextItem()) {if(ImGui::MenuItem("Detach capture to a native window"))Detach(i);if(ImGui::MenuItem("Close capture",nullptr,false,doc.app->CanCloseDocument()))close=i;ImGui::EndPopup();}
            if(!open)close=i;
            if(chosen){if(selected_!=doc.id)menu_owner_=doc.id;selected_=doc.id;ImGui::EndTabItem();}
        }
        ImGui::EndTabBar();
        select_pending_=false;
    }
    if(cyui::iconTextButton(cyui::icon::folder,"Open captures..."))for(const auto& path:ChooseCaptures())pending_open_.push_back(path);
    cyui::tooltip("Open several captures in separate tabs (Ctrl+O)");
    ImGui::SameLine();if(cyui::ghostButton("New capture session")){auto& doc=Create();selected_=menu_owner_=doc.id;select_pending_=true;doc.app->FocusSession();}
    for(size_t i=0;i<documents_.size();++i)if(!documents_[i].detached&&documents_[i].id==selected_){ImGui::SameLine();if(cyui::iconButton(cyui::icon::newWindow,"Detach this capture to a native window (tab context menu)"))Detach(i);break;}
    if(!has_embedded){ImGui::SameLine();ImGui::TextDisabled("Captures are open in separate windows. Use Return to main window to restore a tab.");}
    cyui::endTopBar();
    cyui::beginStatusBar("##WorkspaceStatus");
    cyui::statusText(Busy()?cyui::Tone::accent:cyui::Tone::neutral,"%s | %zu capture%s",Busy()?"Analyzing":"Ready",documents_.size(),documents_.size()>1?"s":"");
    ImGui::SameLine(0,cyui::dp(24));cyui::statusText(cyui::Tone::neutral,"Right-drag: pan  |  Wheel: zoom  |  Lower ruler: measure  |  F: fit selection");
    cyui::endStatusBar();
    auto pos=main->WorkPos;auto size=main->WorkSize;
    for(size_t i=0;i<documents_.size();++i){auto& doc=documents_[i];bool open=true;
        if(!doc.detached&&doc.id!=selected_){doc.app->DrawInactive();continue;}
        if(!doc.detached){ImGui::SetNextWindowViewport(main->ID);ImGui::SetNextWindowPos(pos);ImGui::SetNextWindowSize(size);}
        else {
            ImGuiWindowClass cls;cls.ViewportFlagsOverrideSet=ImGuiViewportFlags_NoAutoMerge;ImGui::SetNextWindowClass(&cls);
            if(doc.position){ImGui::SetNextWindowPos(ImVec2(main->Pos.x+80,main->Pos.y+100));ImGui::SetNextWindowSize(ImVec2(std::max(800.f,main->Size.x*.8f),std::max(500.f,main->Size.y*.8f)));doc.position=false;}
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
        auto flags=ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoCollapse;
        if(!doc.detached)flags|=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoBringToFrontOnFocus;
        bool visible=ImGui::Begin(Title(doc).c_str(),doc.detached&&doc.app->CanCloseDocument()?&open:nullptr,flags);
        doc.viewport=ImGui::GetWindowViewport()->ID;
        bool active=visible&&(doc.detached||doc.id==selected_);
        const auto* focused=ImGui::GetCurrentContext()->NavWindow;
        const auto panel_class=ImHashStr(("CyTracePanels"+std::to_string(doc.id)).c_str());
        if(active&&(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)||focused&&focused->WindowClass.ClassId==panel_class))menu_owner_=doc.id;
        if(active&&doc.detached){if(ImGui::Button("Return to main window"))Attach(i);ImGui::SameLine();ImGui::TextDisabled("%s",doc.app->DocumentPath().filename().u8string().c_str());}
        doc.app->DrawHosted(active);ImGui::End();ImGui::PopStyleVar();
        if(!open)close=i;
    }
    if(close!=SIZE_MAX)Close(close);
}
void DocumentWorkspace::WriteReport(const fs::path& file) const {
    auto report=cygi::Json::Object();report["documents"]=cygi::Json::Array();
    for(const auto& doc:documents_){auto row=cygi::Json::Object();row["id"]=std::to_string(doc.id);row["path"]=doc.app->DocumentPath().u8string();row["detached"]=doc.detached;row["viewport"]=std::to_string(doc.viewport);row["busy"]=doc.app->AnalysisBusy();row["filter"]=doc.app->InteractionView().filter_name;row["timingWindow"]=doc.app->WindowName("Timing");row["start"]=doc.app->NavigationView().start;row["end"]=doc.app->NavigationView().end;report["documents"].Push(row);}
    report["nativeViewports"]=ImGui::GetPlatformIO().Viewports.Size;
    report["platformWindows"]=cygi::Json::Array();
    for(const auto* viewport:ImGui::GetPlatformIO().Viewports){auto row=cygi::Json::Object();row["id"]=std::to_string(viewport->ID);row["nativeHandle"]=viewport->PlatformHandleRaw!=nullptr;report["platformWindows"].Push(row);}
    report["windows"]=cygi::Json::Array();
    for(const auto* w:ImGui::GetCurrentContext()->Windows){auto row=cygi::Json::Object();row["name"]=w->Name;row["active"]=w->Active;row["hidden"]=w->Hidden;row["skip"]=w->SkipItems;row["vertices"]=w->DrawList->VtxBuffer.Size;row["viewport"]=std::to_string(w->ViewportId);row["x"]=w->Pos.x;row["y"]=w->Pos.y;row["width"]=w->Size.x;row["height"]=w->Size.y;report["windows"].Push(row);}
    std::ofstream(file)<<report.Write(2);
}
}
