// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ManualView.hpp"
#include "../Common/Documentation/HelpContract.hpp"
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <imgui.h>
#include <filesystem>
#include <fstream>

namespace cyinsights {
namespace {
bool pending=false,prefer_stored_locale=true;std::string topic="welcome",language="en",media,anchor,last_url,error;
std::filesystem::path LanguagePath(){wchar_t root[32768]{};auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",root,32768);return n&&n<32768?std::filesystem::path(root)/L"CyInsights/manual-language.txt":std::filesystem::path();}
std::filesystem::path HelpPath(){wchar_t exe[32768]{};auto n=GetModuleFileNameW(nullptr,exe,32768);return n&&n<32768?std::filesystem::path(exe).parent_path()/L"Documentation/Documentation.html":std::filesystem::path();}
std::string BaseUrl(){auto path=HelpPath();if(!std::filesystem::is_regular_file(path))return {};wchar_t url[65536]{};DWORD size=65536;if(FAILED(UrlCreateFromPathW(path.c_str(),url,&size,0)))return {};return std::filesystem::path(url).u8string();}
}
void InitializeManual(ID3D11Device*){try{std::ifstream in(LanguagePath());std::string value;in>>value;if(value=="fr")language=value;}catch(...){} }
void ShutdownManual(){}
void ShowManual(const std::string& page,const std::string& locale){if(!page.empty()){topic=page;media.clear();anchor.clear();}if(!locale.empty()){language=locale=="fr"?"fr":"en";prefer_stored_locale=false;try{auto file=LanguagePath();if(!file.empty()){std::filesystem::create_directories(file.parent_path());std::ofstream(file)<<language;}}catch(...){}}pending=true;}
void SelectManualMedia(const std::string& id){if(id=="timeline"||id=="histogram"||id=="gpu-passes")media=id;else anchor=id;pending=true;}
void SelectManualAnchor(const std::string& id){anchor=id;pending=true;}
void DrawManual(){
    if(ImGui::IsKeyPressed(ImGuiKey_F1,false)&&!ImGui::GetIO().WantTextInput)ShowManual();
    if(pending){pending=false;auto base=BaseUrl();error.clear();if(base.empty())error="Offline help is missing. Extract the complete application archive, including Documentation/.";
        else{last_url=cyhelp::Route(base,language,topic,anchor,media);if(prefer_stored_locale)last_url+=media.empty()?"?preferred=auto":"&preferred=auto";prefer_stored_locale=true;auto wide=std::filesystem::u8path(last_url).wstring();auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",wide.c_str(),nullptr,nullptr,SW_SHOWNORMAL));if(result<=32)error="Could not open offline help in the default browser. Open Documentation/Documentation.html next to the application.";}}
    if(!error.empty()){ImGui::OpenPopup("Documentation unavailable");if(ImGui::BeginPopupModal("Documentation unavailable",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){ImGui::TextWrapped("%s",error.c_str());if(ImGui::Button("Close")){error.clear();ImGui::CloseCurrentPopup();}ImGui::EndPopup();}}
}
cygi::Json ManualReport(){auto out=cygi::Json::Object();out["reader"]="CyDoc";out["url"]=last_url;out["topic"]=topic;out["language"]=language;out["error"]=error;return out;}
}
