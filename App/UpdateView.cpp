// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "UpdateView.hpp"
#include "ReleaseUpdate.hpp"
#include <Windows.h>
#include <cyui/CyUI.h>
#include <imgui.h>
#include <fstream>
#include <future>
#include <chrono>
namespace cyinsights {
namespace {
using namespace updates;
constexpr const char* VersionText=CYINSIGHTS_UPDATE_VERSION;
struct Result {std::optional<Release> release;fs::path work;std::string status;};
struct Controller {
    bool visible=false,automatic=true,suppress=false,started=false,exit=false;int64_t last_check=0;
    std::string status="Check GitHub for a newer stable release.";std::optional<Release> release;fs::path work,preferences;
    std::future<Result> task;std::atomic_bool cancel{false};std::atomic<uint64_t> downloaded{0},total{0};
    Controller(){
        wchar_t local[32768]{};auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,DWORD(std::size(local)));
        if(count&&count<std::size(local)){preferences=fs::path(local)/L"CyInsights"/L"updates.json";try{if(fs::file_size(preferences)>16384)return;std::ifstream in(preferences);std::string text((std::istreambuf_iterator<char>(in)),{});cygi::Json j;if(cygi::Json::Parse(text,j)){automatic=j["automatic"].AsBool(true);last_check=int64_t(j["last_check"].AsUInt());}}catch(...) {}}
    }
    ~Controller(){cancel=true;if(task.valid())task.wait();}
    bool Busy()const{return task.valid();}
    void Save(){try{if(preferences.empty())return;fs::create_directories(preferences.parent_path());auto j=cygi::Json::Object();j["automatic"]=automatic;j["last_check"]=double(last_check);std::ofstream(preferences)<<j.Write(2);}catch(...) {}}
    void Check(){
        if(Busy())return;cancel=false;downloaded=total=0;release.reset();work.clear();status="Checking GitHub stable releases...";
        last_check=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();Save();
        task=std::async(std::launch::async,[this]{auto j=FetchLatest(cancel);auto selected=Select(j,VersionText,"Windows-x64.zip");Result r;r.release=selected;
            r.status=selected?"A newer stable release is available.":"No newer compatible stable release is available. Alpha and other prereleases are ignored.";return r;});
    }
    void DownloadUpdate(){
        if(Busy()||!release)return;cancel=false;downloaded=0;total=release->size;status="Downloading and verifying the update package...";auto chosen=*release;
        task=std::async(std::launch::async,[this,chosen]{auto dir=NewWorkDirectory();Download(chosen,dir/L"update.zip",cancel,[this](uint64_t a,uint64_t b){downloaded=a;total=b;});Prepare(dir/L"update.zip",dir/L"prepared",chosen.tag,cancel);return Result{chosen,dir,"Package and file hashes verified. Ready to install and restart."};});
    }
    void Tick(){
        if(!started){started=true;auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();if(automatic&&!suppress&&(last_check==0||now<last_check||now-last_check>=86400))Check();}
        if(task.valid()&&task.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto r=task.get();release=std::move(r.release);work=std::move(r.work);status=std::move(r.status);if(release&&work.empty())cyui::notify("A stable CyInsights update is available in Help > Updates",cyui::Tone::info);}catch(const std::exception& e){status=e.what();work.clear();}}
    }
    void Install(const std::vector<fs::path>& reopen){
        try{
            if(Busy()||!release||work.empty())return;wchar_t image[32768]{};GetModuleFileNameW(nullptr,image,DWORD(std::size(image)));auto target=fs::path(image).parent_path();
            auto helper=target/L"CyInsightsUpdater.exe";if(!fs::is_regular_file(helper))throw std::runtime_error("CyInsightsUpdater.exe is missing. Extract the complete Windows release package.");
            auto probe=target/(L".cyinsights-update-probe-"+std::to_wstring(GetCurrentProcessId()));
            HANDLE test=CreateFileW(probe.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
            if(test==INVALID_HANDLE_VALUE)throw std::runtime_error("The application folder is not writable. Extract the release to a writable folder and retry.");CloseHandle(test);
            // Preparation already ran off the render thread. The helper verifies
            // the ZIP and every prepared file again before modifying the app.
            fs::create_directories(work/L"helper");fs::copy_file(helper,work/L"helper"/helper.filename(),fs::copy_options::overwrite_existing);
            if(HashFile(helper)!=HashFile(work/L"helper"/helper.filename()))throw std::runtime_error("Updater helper copy failed verification");
            auto job=cygi::Json::Object();job["version"]=release->tag;job["target"]=target.u8string();job["sha256"]=release->sha256;job["size"]=release->size;job["pid"]=uint32_t(GetCurrentProcessId());
            FILETIME created{},ended{},kernel{},user{};if(!GetProcessTimes(GetCurrentProcess(),&created,&ended,&kernel,&user))throw std::runtime_error("Cannot identify this process");ULARGE_INTEGER time;time.LowPart=created.dwLowDateTime;time.HighPart=created.dwHighDateTime;job["process_created"]=std::to_string(time.QuadPart);
            job["reopen"]=cygi::Json::Array();for(const auto& path:reopen)if(!path.empty()&&path.is_absolute())job["reopen"].Push(path.u8string());auto file=work/L"install.json";
            {std::ofstream out(file);out<<job.Write(2);if(!out)throw std::runtime_error("Cannot save the update transaction");}
            auto copy=work/L"helper"/helper.filename();auto command=Quote(copy.wstring())+L" --apply "+Quote(file.wstring());STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
            if(!CreateProcessW(copy.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,work.c_str(),&si,&pi))throw std::runtime_error("Cannot start the updater helper");CloseHandle(pi.hThread);CloseHandle(pi.hProcess);exit=true;
        }catch(const std::exception& e){status=e.what();}
    }
};
Controller& State(){static Controller controller;return controller;}
}
void UpdateMenu(){if(ImGui::BeginMenu("Help")){if(ImGui::MenuItem("Updates..."))State().visible=true;ImGui::EndMenu();}}
void ShowUpdates(){State().visible=true;}
void SuppressAutomaticUpdateCheck(){State().suppress=true;}
bool UpdatesVisible(){return State().visible;}
bool UpdateExitRequested(){return State().exit;}
void DrawUpdates(bool can_install,const std::vector<fs::path>& reopen){
    auto& s=State();s.Tick();if(!s.visible)return;
    ImGui::SetNextWindowSize(cyui::dp(610,390),ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Updates###CyInsightsUpdates",&s.visible,ImGuiWindowFlags_NoDocking)){
        cyui::heading("CyInsights updates",1);cyui::badge("STABLE RELEASES ONLY",cyui::Tone::accent);
        cyui::textDim("Installed: %s",VersionText);cyui::textFaint("github.com/MrMybal/CyInsights");
        if(cyui::toggleSwitch("Check automatically once a day",&s.automatic))s.Save();
        cyui::beginCard("##update_status","GitHub release");ImGui::TextWrapped("%s",s.status.c_str());
        if(s.release)cyui::textDim("Version %s | %.1f MiB",s.release->version.c_str(),double(s.release->size)/1048576.);
        if(s.Busy()){auto total=s.total.load();auto received=s.downloaded.load();cyui::progressBar(total?float(double(received)/total):0,ImVec2(-1,0),total?"Downloading / preparing":"Checking GitHub");if(cyui::button("Cancel"))s.cancel=true;}
        else if(!s.work.empty()){
            if(!can_install)cyui::callout(cyui::Tone::warning,"Stop active captures and wait for analysis before installing.");
            ImGui::BeginDisabled(!can_install);if(cyui::primaryButton("Install and restart"))s.Install(reopen);ImGui::EndDisabled();
        }else if(s.release){if(cyui::primaryButton("Download update"))s.DownloadUpdate();}
        else if(cyui::primaryButton("Check for updates"))s.Check();
        if(!s.Busy()&&s.release){ImGui::SameLine();if(cyui::button("Check again"))s.Check();}
        cyui::endCard();
        ImGui::TextWrapped("Updates are downloaded from this repository and verified with SHA-256. Installation preserves settings and existing captures. Alpha, beta, release candidates and other prereleases are never offered.");
    }ImGui::End();
}
}
