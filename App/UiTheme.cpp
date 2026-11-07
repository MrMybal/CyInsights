// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "UiTheme.hpp"
#include "../Core/AppPaths.hpp"
#include <cyui/modules/Inspector.h>
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <map>
#include <cctype>
namespace cyinsights {
namespace {
struct Appearance {
    bool editor=false;
    cyui::InspectorState inspector;
    cyui::TextFilter filter;
    uint64_t revision=0;
    std::vector<cyui::Palette> undo,redo;
    char path[1024]{"my_theme.cytheme"};
    int role=0;
    uint64_t color_revision=UINT64_MAX;
    ImVec4 color{};
};
std::map<ImGuiContext*,std::unique_ptr<Appearance>> appearances;
Appearance& State(){auto& state=appearances[ImGui::GetCurrentContext()];if(!state)state=std::make_unique<Appearance>();return *state;}
std::filesystem::path SettingsPath() {
    return UserDataDirectory()/L"appearance.txt";
}
void Save() {
    try {auto path=SettingsPath();if(path.empty())return;std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path);output<<cyui::currentThemeKey()<<'\n'<<(cyui::density()==cyui::Density::comfortable?"comfortable":"compact")<<'\n'<<cyui::locale()<<'\n';
        output.close();if(!output)cyui::notify("Could not save appearance settings",cyui::Tone::warning);
    }catch(...) {cyui::notify("Could not save appearance settings",cyui::Tone::warning);}
}
void CommitPalette(const cyui::Palette& palette){
    auto& state=State();if(cyui::paletteToText(palette)==cyui::paletteToText(cyui::palette()))return;
    state.undo.push_back(cyui::palette());if(state.undo.size()>128)state.undo.erase(state.undo.begin());state.redo.clear();
    cyui::setPalette(palette);++state.revision;
}
std::string Hex(ImU32 value){const auto c=cyui::vec(value);char text[12];std::snprintf(text,sizeof(text),"#%02X%02X%02X%02X",int(c.x*255+.5f),int(c.y*255+.5f),int(c.z*255+.5f),int(c.w*255+.5f));return text;}
void PaletteEditor(){
    auto& state=State();const auto* viewport=ImGui::GetMainViewport();const auto room=viewport->WorkSize;
    const ImVec2 initial{std::min(cyui::dp(480),std::max(cyui::dp(180),room.x-cyui::dp(24))),std::min(cyui::dp(600),std::max(cyui::dp(180),room.y-cyui::dp(24)))};
    ImGui::SetNextWindowSize(initial,ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({viewport->WorkPos.x+(room.x-initial.x)*.5f,viewport->WorkPos.y+(room.y-initial.y)*.5f},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin(cyui::trLabel("Palette editor").c_str(),&state.editor)){ImGui::End();return;}
    cyui::panelHeader(cyui::tr("Palette editor").c_str(),"Semantic colors / .cytheme",&state.filter);
    const float file_buttons=cyui::buttonWidth(cyui::tr("Save").c_str())+cyui::buttonWidth(cyui::tr("Load").c_str())+ImGui::GetStyle().ItemSpacing.x*2;
    const bool inline_files=ImGui::GetContentRegionAvail().x>file_buttons+cyui::dp(120);
    cyui::textField("##path",state.path,sizeof(state.path),"File",inline_files?-file_buttons:-1);if(inline_files)ImGui::SameLine();
    if(cyui::button(cyui::trLabel("Save").c_str()))cyui::notify(cyui::savePalette(state.path,cyui::palette())?"Theme saved":"Could not save the theme");
    ImGui::SameLine();if(cyui::button(cyui::trLabel("Load").c_str())){auto palette=cyui::palette();if(cyui::loadPalette(state.path,palette))CommitPalette(palette);else cyui::notify("Could not read the theme",cyui::Tone::error);}
    ImGui::BeginDisabled(state.undo.empty());bool undo=cyui::button(cyui::trLabel("Undo").c_str());ImGui::EndDisabled();ImGui::SameLine();
    ImGui::BeginDisabled(state.redo.empty());bool redo=cyui::button(cyui::trLabel("Redo").c_str());ImGui::EndDisabled();
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)&&!ImGui::GetIO().WantTextInput){undo|=ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Z);redo|=ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Y);}
    if(undo)UndoUiPalette();
    if(redo)RedoUiPalette();
    const auto& fields=cyui::paletteFields();
    if(state.color_revision!=state.revision){state.color=cyui::vec(cyui::palette().*fields[state.role].member);state.color_revision=state.revision;}
    if(ImGui::BeginCombo(cyui::trLabel("Color picker").c_str(),cyui::tr(fields[state.role].label).c_str())){
        for(size_t i=0;i<fields.size();++i)if(ImGui::Selectable(cyui::trLabel(fields[i].label).c_str(),state.role==int(i))){state.role=int(i);state.color=cyui::vec(cyui::palette().*fields[i].member);}ImGui::EndCombo();
    }
    if(cyui::button(cyui::trLabel("Read current color").c_str()))state.color=cyui::vec(cyui::palette().*fields[state.role].member);
    ImGui::SetNextItemWidth(std::max(cyui::dp(80),ImGui::GetContentRegionAvail().x-cyui::buttonWidth(cyui::tr("Apply color").c_str())-ImGui::GetStyle().ItemSpacing.x));
    ImGui::ColorEdit4("##picker",&state.color.x,ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_DisplayHex);
    ImGui::SameLine();if(cyui::button(cyui::trLabel("Apply color").c_str())){auto palette=cyui::palette();palette.*fields[state.role].member=ImGui::ColorConvertFloat4ToU32(state.color);CommitPalette(palette);}
    std::vector<cyui::Property> properties;
    for(size_t i=0;i<fields.size();++i){cyui::Property property;property.id=i+1;property.label=fields[i].label;property.help=fields[i].key;property.value=Hex(cyui::palette().*fields[i].member);properties.push_back(std::move(property));}
    const auto edits=cyui::inspector("##palette",state.inspector,properties,state.revision,[](cyui::ItemId,const cyui::PropertyValue& value){
        const auto* text=std::get_if<std::string>(&value);bool valid=text&&(text->size()==7||text->size()==9)&&(*text)[0]=='#';
        if(valid)for(size_t i=1;i<text->size();++i)valid&=std::isxdigit(static_cast<unsigned char>((*text)[i]))!=0;
        return valid?std::string():std::string("Use #RRGGBB or #RRGGBBAA");
    },{&state.filter,"Palette"});
    for(const auto& edit:edits)if(edit.edit.committed()&&edit.id>0&&edit.id<=fields.size()){
        auto palette=cyui::palette();const auto& field=fields[size_t(edit.id-1)];palette=cyui::paletteFromText(std::string(field.key)+" = "+std::get<std::string>(edit.edit.value),palette);CommitPalette(palette);
    }
    ImGui::End();
}
}
void InitializeUi(float dpi) {
    appearances[ImGui::GetCurrentContext()]=std::make_unique<Appearance>();
    cyui::registerTranslations("fr",{{"Appearance","Apparence"},{"Palette editor","Éditeur de palette"},{"Language","Langue"},{"Undo","Annuler"},{"Redo","Rétablir"},{"Color picker","Couleur"},{"Apply color","Appliquer"},{"Read current color","Lire la couleur actuelle"},{"No available observations in this interval","Aucune observation disponible dans cet intervalle"}});
    cyui::Config config;config.themeKey="mid-grey";config.dpiScale=dpi;config.squareWindows=true;
    std::string theme,density,locale="en";
    try {std::ifstream settings(SettingsPath());settings>>theme>>density>>locale;}catch(...) {}
    for(const auto& candidate:cyui::builtInThemes())if(theme==candidate.key)config.themeKey=candidate.key.c_str();
    if(density=="comfortable")config.density=cyui::Density::comfortable;
    cyui::apply(config);cyui::setLocale(locale.c_str());cyui::loadFonts();State().color=cyui::vec(cyui::palette().bg0);
}
void AppearanceMenu() {
    if(!ImGui::BeginMenu(cyui::trLabel("Appearance").c_str()))return;
    for(const auto& theme:cyui::builtInThemes())if(ImGui::MenuItem(theme.name.c_str(),nullptr,cyui::currentThemeKey()==theme.key)){
        cyui::setTheme(theme.key.c_str());++State().revision;Save();
    }
    ImGui::Separator();
    if(ImGui::MenuItem(cyui::trLabel("Compact").c_str(),nullptr,cyui::density()==cyui::Density::compact)){cyui::setDensity(cyui::Density::compact);Save();}
    if(ImGui::MenuItem(cyui::trLabel("Comfortable").c_str(),nullptr,cyui::density()==cyui::Density::comfortable)){cyui::setDensity(cyui::Density::comfortable);Save();}
    if(ImGui::BeginMenu(cyui::trLabel("Language").c_str())){for(const auto& language:cyui::availableLocales())if(ImGui::MenuItem(language.name.c_str(),nullptr,cyui::locale()==language.code)){cyui::setLocale(language.code.c_str());Save();}ImGui::EndMenu();}
    ImGui::Separator();ImGui::MenuItem(cyui::trLabel("Palette editor").c_str(),nullptr,&State().editor);
    ImGui::EndMenu();
}
void RenderUiExtras() {
    if(State().editor)PaletteEditor();
    cyui::renderNotifications();
}
void ShowPaletteEditor(){State().editor=true;}
void CommitUiPalette(const cyui::Palette& palette){CommitPalette(palette);}
bool UndoUiPalette(){auto& state=State();if(state.undo.empty())return false;state.redo.push_back(cyui::palette());cyui::setPalette(state.undo.back());state.undo.pop_back();++state.revision;return true;}
bool RedoUiPalette(){auto& state=State();if(state.redo.empty())return false;state.undo.push_back(cyui::palette());cyui::setPalette(state.redo.back());state.redo.pop_back();++state.revision;return true;}
}
