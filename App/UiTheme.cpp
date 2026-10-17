// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "UiTheme.hpp"
#include <Windows.h>
#include <filesystem>
#include <fstream>
namespace cyinsights {
namespace {
bool gallery=false,editor=false;
std::filesystem::path SettingsPath() {
    wchar_t root[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",root,32768);
    return n&&n<32768?std::filesystem::path(root)/L"CyInsights/appearance.txt":std::filesystem::path();
}
void Save() {
    try {auto path=SettingsPath();if(path.empty())return;std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path);output<<cyui::currentThemeKey()<<'\n'<<(cyui::density()==cyui::Density::comfortable?"comfortable":"compact")<<'\n';
        output.close();if(!output)cyui::notify("Could not save appearance settings",cyui::Tone::warning);
    }catch(...) {cyui::notify("Could not save appearance settings",cyui::Tone::warning);}
}
}
void InitializeUi(float dpi) {
    cyui::Config config;config.themeKey="mid-grey";config.dpiScale=dpi;config.squareWindows=true;
    std::string theme,density;
    try {std::ifstream settings(SettingsPath());settings>>theme>>density;}catch(...) {}
    for(const auto& candidate:cyui::builtInThemes())if(theme==candidate.key)config.themeKey=candidate.key.c_str();
    if(density=="comfortable")config.density=cyui::Density::comfortable;
    cyui::apply(config);cyui::loadFonts();
}
void AppearanceMenu() {
    if(!ImGui::BeginMenu("Appearance"))return;
    for(const auto& theme:cyui::builtInThemes())if(ImGui::MenuItem(theme.name.c_str(),nullptr,cyui::currentThemeKey()==theme.key)){
        cyui::setTheme(theme.key.c_str());Save();
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Compact",nullptr,cyui::density()==cyui::Density::compact)){cyui::setDensity(cyui::Density::compact);Save();}
    if(ImGui::MenuItem("Comfortable",nullptr,cyui::density()==cyui::Density::comfortable)){cyui::setDensity(cyui::Density::comfortable);Save();}
    ImGui::Separator();ImGui::MenuItem("CyUI gallery",nullptr,&gallery);ImGui::MenuItem("Palette editor",nullptr,&editor);
    ImGui::EndMenu();
}
void RenderUiExtras() {
    if(gallery)cyui::showGallery(&gallery);
    if(editor)cyui::showThemeEditor(&editor);
    cyui::renderNotifications();
}
}
