#include "cyui/Gallery.h"

#include "cyui/Icons.h"
#include "cyui/Layout.h"
#include "cyui/Theme.h"
#include "cyui/CoreWidgets.h"
#if defined(CYUI_HAS_DATAGRID)
#include "cyui/modules/DataGrid.h"
#endif
#if defined(CYUI_HAS_SPREADSHEET)
#include "cyui/modules/Spreadsheet.h"
#endif
#if defined(CYUI_HAS_RICHDOCUMENT)
#include "cyui/modules/RichDocument.h"
#endif

#if defined(CYUI_HAS_AUDIO)
#include "cyui/modules/Audio.h"
#endif
#if defined(CYUI_HAS_CODEEDITOR)
#include <TextEditor.h>
#endif

#include <imgui.h>
#include "cyui/CyUI.h"
#if defined(CYUI_HAS_INSPECTOR)
#include "cyui/examples/EditorWorkspace.h"
#endif
#include <cstdint>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>

namespace cyui {
namespace {

void themeBar()
{
    const auto& themes = builtInThemes();
    static const char* names[16] {};
    auto current = 0;
    const auto count = static_cast<int>(std::min<std::size_t>(themes.size(), 16));
    for (auto i = 0; i < count; ++i) {
        names[i] = themes[static_cast<std::size_t>(i)].name.c_str();
        if (themes[static_cast<std::size_t>(i)].key == currentThemeKey())
            current = i;
    }
    ImGui::AlignTextToFramePadding();
    textDim(tr("Theme").c_str());
    ImGui::SameLine();
    if (combo("##theme", &current, names, count, dp(200.0f)))
        setTheme(themes[static_cast<std::size_t>(current)].key.c_str());
    ImGui::SameLine();
    static const char* densities[] { "Compact", "Comfortable" };
    auto dense = density() == Density::compact ? 0 : 1;
    if (segmented("##density", densities, 2, &dense))
        setDensity(dense == 0 ? Density::compact : Density::comfortable);
    ImGui::SameLine();
    const auto languages = availableLocales();
    std::vector<const char*> languageNames;
    int language = 0;
    for (std::size_t i = 0; i < languages.size(); ++i) {
        languageNames.push_back(languages[i].name.c_str());
        if (languages[i].code == locale()) language = int(i);
    }
    textDim(tr("Language").c_str());
    ImGui::SameLine();
    if (combo("##language", &language, languageNames.data(), int(languageNames.size()), dp(112)))
        setLocale(languages[std::size_t(language)].code.c_str());
}

void palettePreview()
{
    const auto& p = palette();
    for (const auto& field : paletteFields()) {
        ImGui::PushID(field.key);
        colourChip("##chip", p.*field.member, dp(22.0f, 22.0f));
        tooltip(field.label);
        ImGui::PopID();
        ImGui::SameLine(0.0f, dp(3.0f));
    }
    ImGui::NewLine();
}

void textPage()
{
    heading(tr("Heading 1").c_str(), 1);
    heading(tr("Heading 2").c_str(), 2);
    heading(tr("Heading 3").c_str(), 3);
    sectionTitle(tr("Section title").c_str());
    overline(tr("OVERLINE LABEL").c_str());
    ImGui::TextUnformatted(tr("Primary text").c_str());
    textDim(tr("Secondary text").c_str());
    textFaint(tr("Faint text, hints").c_str());
    textColoured(palette().info, tr("Coloured text (a palette role)").c_str());
    textMono(tr("C:/Projects/mono/path.txt").c_str());
    ImGui::TextUnformatted(tr("Shortcut").c_str());
    ImGui::SameLine();
    kbd("Ctrl+S");
    ImGui::SameLine();
    textDim(tr("Help").c_str());
    helpMarker(tr("A help marker: its tooltip explains the option before it.").c_str());
    ImGui::SameLine(0.0f, dp(16.0f));
    if (linkText(trLabel("A link").c_str()))
        notify(tr("Link clicked").c_str(), Tone::info);
    space();
    badge(tr("NEUTRAL").c_str());
    ImGui::SameLine();
    badge(tr("ACCENT").c_str(), Tone::accent);
    ImGui::SameLine();
    badge(tr("OK").c_str(), Tone::success);
    ImGui::SameLine();
    badge(tr("BETA").c_str(), Tone::warning);
    ImGui::SameLine();
    badge(tr("ERROR").c_str(), Tone::error);
    ImGui::SameLine();
    badge(tr("INFO").c_str(), Tone::info);
    statusDot(palette().success, "Online");
    ImGui::SameLine();
    ImGui::TextUnformatted(tr("Online").c_str());
    ImGui::SameLine(0.0f, dp(14.0f));
    statusDot(palette().error, "Offline");
    ImGui::SameLine();
    ImGui::TextUnformatted(tr("Offline").c_str());
    space();
    callout(Tone::info, tr("A callout gives context to a whole panel: what it does, or why it is empty.").c_str());
    space(4.0f);
    callout(Tone::warning, tr("Unsaved changes will be lost when the project closes.").c_str());
}

void buttonsPage()
{
    button(trLabel("Secondary").c_str());
    ImGui::SameLine();
    primaryButton(trLabel("Primary").c_str());
    ImGui::SameLine();
    ghostButton(trLabel("Ghost").c_str());
    ImGui::SameLine();
    button(trLabel("Outline").c_str(), Variant::outline);
    ImGui::SameLine();
    dangerButton(trLabel("Delete").c_str());
    ImGui::SameLine();
    button(trLabel("Confirm").c_str(), Variant::success);

    iconTextButton(icon::save, trLabel("Save").c_str());
    ImGui::SameLine();
    iconTextButton(icon::folder, trLabel("Open").c_str(), Variant::ghost);
    ImGui::SameLine();
    iconTextButton(icon::add, trLabel("New project").c_str(), Variant::primary);
    ImGui::SameLine();
    {
        ScopedDisabled disabled;
        button(trLabel("Disabled").c_str());
    }

    space();
    sectionTitle(tr("Toolbar").c_str());
    static auto tool = 0;
    static auto playing = false;
    static auto loop = true;
    const char* tools[] { icon::edit, icon::crop, icon::zoomIn, icon::touch };
    const char* tips[] { "Brush (B)", "Crop (C)", "Zoom (Z)", "Hand (H)" };
    for (auto i = 0; i < 4; ++i) {
        if (iconButton(tools[i], tips[i], tool == i))
            tool = i;
        ImGui::SameLine(0.0f, dp(2.0f));
    }
    toolbarSeparator();
    if (iconButton(playing ? icon::pause : icon::play, "Play / pause (Space)", playing, palette().success))
        playing = !playing;
    ImGui::SameLine(0.0f, dp(2.0f));
    iconButton(icon::stop, "Stop");
    ImGui::SameLine(0.0f, dp(2.0f));
    if (iconButton(icon::loop, "Loop (L)", loop))
        loop = !loop;

    space();
    sectionTitle(tr("Toggles").c_str());
    static bool mute = false, solo = true, arm = false, snap = true, dark = true;
    toggleButton(trLabel("M").c_str(), &mute, palette().info, ImVec2(ImGui::GetFrameHeight(), 0.0f), "Mute");
    ImGui::SameLine(0.0f, dp(2.0f));
    toggleButton(trLabel("S").c_str(), &solo, palette().warning, ImVec2(ImGui::GetFrameHeight(), 0.0f), "Solo");
    ImGui::SameLine(0.0f, dp(2.0f));
    toggleButton(trLabel("R").c_str(), &arm, palette().error, ImVec2(ImGui::GetFrameHeight(), 0.0f), "Arm");
    ImGui::SameLine(0.0f, dp(10.0f));
    toggleButton(trLabel("Snap").c_str(), &snap);
    toggleSwitch(trLabel("Dark mode").c_str(), &dark);
    ImGui::SameLine(0.0f, dp(16.0f));
    static auto autosave = false;
    toggleSwitch(trLabel("Autosave").c_str(), &autosave);

    space();
    sectionTitle(tr("Segmented").c_str());
    static auto view = 1;
    const char* views[] { "List", "Grid", "Details" };
    segmented("##view", views, 3, &view);
    static auto unit = 0;
    const char* units[] { "px", "%", "cm", "in" };
    segmented("##unit", units, 4, &unit, -1.0f);
}

void controlsPage()
{
    const auto t = static_cast<float>(ImGui::GetTime());
#if defined(CYUI_HAS_AUDIO)
    static float gain = 0.5f, pan = 0.0f, cutoff = 1200.0f, voices = 8.0f;
    knob("##gain", &gain, 0.0f, 1.0f, 0.5f, { 34.0f, 0, "Gain", "%.2f" });
    ImGui::SameLine();
    KnobStyle panStyle { 34.0f, palette().info, "Pan", "%.2f" };
    panStyle.flags = knobBipolar;
    knob("##pan", &pan, -1.0f, 1.0f, 0.0f, panStyle);
    ImGui::SameLine();
    KnobStyle cutoffStyle { 34.0f, palette().success, "Cutoff", "%.0f", "Hz" };
    cutoffStyle.flags = knobLogarithmic;
    cutoffStyle.modulation = 0.5f + 0.4f * static_cast<float>(std::sin(ImGui::GetTime() * 2.0));
    knob("##cutoff", &cutoff, 20.0f, 20000.0f, 1200.0f, cutoffStyle);
    ImGui::SameLine();
    KnobStyle voiceStyle { 34.0f, palette().warning, "Voices", "%.0f" };
    voiceStyle.flags = knobInteger;
    voiceStyle.steps = 15;
    knob("##voices", &voices, 1.0f, 16.0f, 8.0f, voiceStyle);

    static float faderDb = 0.0f;
    fader("##fader", &faderDb, -70.0f, 6.0f, dp(30.0f, 150.0f));
    ImGui::SameLine();
    const auto level = 0.35f + 0.3f * std::abs(std::sin(t * 1.7f));
    meter("##meter", dp(12.0f, 150.0f), level * 1.2f, level, level * 0.7f, level * 0.6f);
    ImGui::SameLine();
#endif
    ImGui::BeginGroup();
    static float opacity = 0.75f, tempo = 120.0f;
    sliderBar("##opacity", &opacity, 0.0f, 1.0f, tr("Opacity %.2f").c_str(), dp(220.0f));
    dragValue("##tempo", &tempo, 0.1f, 20.0f, 300.0f, "%.2f BPM", dp(220.0f));
#if defined(CYUI_HAS_AUDIO)
    meter("##hmeter", ImVec2(dp(220.0f), dp(8.0f)), level, level * 0.9f, level * 0.6f, level * 0.5f, false);
#endif
    space(4.0f);
    progressBar(std::fmod(t * 0.15f, 1.0f), ImVec2(dp(220.0f), 0.0f));
    progressBar(-1.0f, ImVec2(dp(220.0f), 0.0f), nullptr, Tone::info);
    progressBar(0.62f, ImVec2(dp(220.0f), 0.0f), tr("Exporting 62 %").c_str(), Tone::success);
    spinner("##spin");
    ImGui::SameLine();
    textDim(tr("Working...").c_str());
    ImGui::EndGroup();

    space();
    sectionTitle(tr("Fields").c_str());
    static char search[128] {};
    searchField("##search", search, sizeof(search), "Search effects", dp(260.0f));
    static char name[128] { "Layer 1" };
    textField("##name", name, sizeof(name), "Name", dp(260.0f));
    static auto blend = 0;
    const char* blends[] { "Normal", "Multiply", "Screen", "Overlay" };
    combo("##blend", &blend, blends, 4, dp(260.0f));
}

void containersPage()
{
    if (beginCard("##card1", tr("Project").c_str())) {
        textDim(tr("D:/Projects/MySong.cydaw").c_str());
        ImGui::SameLine();
        badge(tr("SAVED").c_str(), Tone::success);
        space(4.0f);
        primaryButton(trLabel("Open").c_str());
        ImGui::SameLine();
        button(trLabel("Reveal").c_str());
    }
    endCard();
    space(6.0f);
    if (beginCard("##card2", tr("Update available").c_str(), 0.0f, Tone::accent)) {
        ImGui::TextWrapped("%s", tr("Version 2.4 brings a new mixer and faster loading.").c_str());
        button(trLabel("Install").c_str(), Variant::primary);
    }
    endCard();

    space();
    if (beginSection(trLabel("Transform").c_str())) {
        static float position[2] { 120.0f, 64.0f };
        static float rotation = 0.0f;
        static auto visible = true;
        static auto locked = false;
        if (beginProperties("##transform")) {
            property(tr("Position").c_str());
            ImGui::DragFloat2("##position", position);
            property(tr("Rotation").c_str(), "Degrees, clockwise.");
            sliderBar("##rotation", &rotation, -180.0f, 180.0f, "%.1f deg", -1.0f);
            property(tr("Visible").c_str());
            toggleSwitch("##visible", &visible);
            property(tr("Locked").c_str());
            ImGui::Checkbox("##locked", &locked);
            property(tr("A very long property label that is clipped").c_str());
            static auto mode = 0;
            const char* modes[] { "Fast", "Best" };
            segmented("##mode", modes, 2, &mode, -1.0f);
            endProperties();
        }
        endSection();
    }
    if (beginSection(trLabel("Appearance").c_str(), false)) {
        static float colour[3] { 0.9f, 0.5f, 0.2f };
        if (beginProperties("##appearance")) {
            property(tr("Colour").c_str());
            ImGui::ColorEdit3("##colour", colour);
            endProperties();
        }
        endSection();
    }

    space();
    sectionTitle(tr("Table (stock ImGui, themed)").c_str());
    if (ImGui::BeginTable("##table", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn(trLabel("Name").c_str());
        ImGui::TableSetupColumn(trLabel("Type").c_str());
        ImGui::TableSetupColumn(trLabel("Size").c_str());
        ImGui::TableHeadersRow();
        const char* rows[][3] { { "kick.wav", "Audio", "312 kB" }, { "lead.mid", "MIDI", "4 kB" }, { "mix.cydaw", "Project", "6.2 MB" } };
        for (const auto& row : rows) {
            ImGui::TableNextRow();
            for (auto c = 0; c < 3; ++c) {
                ImGui::TableSetColumnIndex(c);
                if (c == 0)
                    ImGui::TextUnformatted(row[c]);
                else
                    textDim(tr("%s").c_str(), row[c]);
            }
        }
        ImGui::EndTable();
    }

    space();
    static auto showModal = false;
    if (button(trLabel("Open a modal").c_str()))
        showModal = true;
    ImGui::SameLine();
    if (button(trLabel("Notify").c_str()))
        notify(tr("Project saved").c_str(), Tone::success);
    ImGui::SameLine();
    if (button(trLabel("Notify error").c_str()))
        notify(tr("Could not open the audio device").c_str(), Tone::error);
    if (beginModal(trLabel("Delete the layer?").c_str(), &showModal)) {
        ImGui::TextWrapped("%s", tr("The layer \"Background\" and its masks will be deleted. This cannot be undone.").c_str());
        if (modalButtons("Delete", "Cancel", true) == 1)
            notify(tr("Layer deleted").c_str(), Tone::neutral);
        endModal();
    }

    space();
    if (beginCard("##empty", nullptr, 170.0f))
        emptyState(tr("No layers yet").c_str(), tr("Drop an image here, or press Ctrl+N").c_str(), icon::photo);
    endCard();
}

// The application shell at a small scale. The real top bar, status bar and
// sidebar are viewport bars (the demo application around this window shows
// them); child regions stand in for them here.
void layoutPage()
{
    const auto& p = palette();
    callout(Tone::info, tr("beginTopBar, beginStatusBar and beginSidebar take their room off the main viewport, then dockSpace "
                        "fills the rest. Below, child regions stand in for those bars.").c_str());
    static auto mode = 0;
    static auto page = 0;
    static auto playing = false;

    space();
    sectionTitle(tr("Top bar").c_str());
    {
        ScopedColor ground(ImGuiCol_ChildBg, p.bg1);
        ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(8.0f, metrics().toolbarHeight * 0.5f));
        if (ImGui::BeginChild("##topBarPreview", ImVec2(0.0f, ImGui::GetFrameHeight() + dp(metrics().toolbarHeight)),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
            const char* modes[] { "Studio", "Mixer", "Library" };
            segmented("##mode", modes, 3, &mode);
            toolbarSeparator();
            if (iconButton(playing ? icon::pause : icon::play, "Play / pause (Space)", playing, p.success))
                playing = !playing;
            ImGui::SameLine(0.0f, dp(2.0f));
            iconButton(icon::stop, "Stop");
        }
        ImGui::EndChild();
    }

    space();
    sectionTitle(tr("Sidebar, panel header and toolbar").c_str());
    const auto height = dp(250.0f);
    {
        ScopedColor ground(ImGuiCol_ChildBg, p.bg0);
        ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(10.0f, 12.0f));
        if (ImGui::BeginChild("##sidebarPreview", ImVec2(dp(190.0f), height),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
            sidebarHeader("CYUI DEMO", "1.1");
            sidebarGroup("WORKSPACE");
            if (sidebarItem(icon::home, trLabel("Home").c_str(), page == 0))
                page = 0;
            if (sidebarItem(icon::library, trLabel("Library").c_str(), page == 1, "12"))
                page = 1;
            sidebarGroup("SYSTEM");
            if (sidebarItem(icon::settings, trLabel("Settings").c_str(), page == 2))
                page = 2;
        }
        ImGui::EndChild();
    }
    ImGui::SameLine();
    if (ImGui::BeginChild("##panelPreview", ImVec2(0.0f, height), ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding)) {
        panelHeader(tr("Devices").c_str(), "Kick drum - Track 1");
        space(4.0f);
        if (beginToolbar("##toolbarPreview")) {
            iconButton(icon::add, "Add a device");
            ImGui::SameLine(0.0f, dp(2.0f));
            iconButton(icon::presets, "Presets");
            toolbarSeparator();
            badge(tr("3 DEVICES").c_str());
        }
        endToolbar();
        space(6.0f);
        textDim(tr("Panel content: cards, properties, views.").c_str());
    }
    ImGui::EndChild();

    space();
    sectionTitle(tr("Status bar").c_str());
    {
        ScopedColor ground(ImGuiCol_ChildBg, p.bg0);
        ScopedVar padding(ImGuiStyleVar_WindowPadding, dp(8.0f, 4.0f));
        if (ImGui::BeginChild("##statusPreview", ImVec2(0.0f, ImGui::GetFontSize() * 0.86f + dp(10.0f) + dp(2.0f)),
                              ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_NoScrollbar)) {
            statusText(Tone::neutral, tr("Ready").c_str());
            ImGui::SameLine(0.0f, dp(18.0f));
            statusText(Tone::success, tr("Audio: 48 kHz, 256 samples").c_str());
            ImGui::SameLine(0.0f, dp(18.0f));
            statusText(Tone::warning, tr("2 warnings").c_str());
        }
        ImGui::EndChild();
    }

    space();
    sectionTitle(tr("Dock space").c_str());
#ifdef IMGUI_HAS_DOCK
    textDim(tr("dockSpace() docks the panels over what the bars leave of the main viewport.").c_str());
#else
    textFaint(tr("dockSpace() needs ImGui's docking branch.").c_str());
#endif
}

void filteringPage(const GalleryResources& resources)
{
    const auto dispatch = [&](ItemId action) {
        if (action == 22 && resources.openDocumentation)
            resources.openDocumentation("filtering", "searchable-menus");
        else if (action) notify(tr("Menu action requested").c_str(), Tone::info);
    };
    sectionTitle(tr("Searchable menus and panels").c_str());
    textDim("%s", tr("Try a property, section or menu action. Words can match English or the selected language.").c_str());
    static TextFilter panelFilter = [] { TextFilter value; value.query = "surface"; return value; }();
    static TextFilter menuFilter = [] { TextFilter value; value.query = "export"; return value; }();
    static TextFilter popupFilter;
    static const std::vector<FilterMenuItem> actions{
        {0, "File", {}, {}, true, false, {
            {1, "New", "Ctrl+N"}, {2, "Open...", "Ctrl+O"}, {3, "Save", "Ctrl+S"},
            {0, "Export", {}, {}, true, false, {
                {11, "Export PNG", {}, "image texture"}, {12, "Export JPEG", {}, "image photograph"},
                {13, "Export HDR", {}, "image high dynamic range", false}}}}},
        {}, {21, "Preferences", {}, "settings configuration"}, {22, "Help", "F1"}};
    if (ImGui::BeginTable("##filterExamples", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        if (beginCard("##filterPanel", nullptr, 420)) {
            panelHeader(tr("Panel filter").c_str(), nullptr, &panelFilter);
            static char name[80] = "Landscape material";
            static bool enabled = true, shadows = true;
            static float roughness = .38f, metallic = .12f, opacity = 1;
            const auto section = [&](const char* title, std::initializer_list<const char*> labels, auto draw) {
                const std::string context = std::string(title) + " " + tr(title);
                const auto visible = [&](const char* label) { return panelFilter.matches(label, context); };
                const bool any = std::any_of(labels.begin(), labels.end(), visible);
                if (beginFilteredSection(title, panelFilter, any)) {
                    if (beginProperties(title)) { draw(visible); endProperties(); }
                    endSection();
                }
            };
            section("Identity", {"Name", "Enabled"}, [&](auto visible) {
                if (visible("Name")) { property(tr("Name").c_str()); textField("##name", name, sizeof(name)); }
                if (visible("Enabled")) { property(tr("Enabled").c_str()); toggleSwitch("##enabled", &enabled); }
            });
            section("Surface", {"Roughness", "Metallic", "Opacity"}, [&](auto visible) {
                if (visible("Roughness")) { property(tr("Roughness").c_str()); sliderBar("##roughness", &roughness, 0, 1, "%.2f", -1); }
                if (visible("Metallic")) { property(tr("Metallic").c_str()); sliderBar("##metallic", &metallic, 0, 1, "%.2f", -1); }
                if (visible("Opacity")) { property(tr("Opacity").c_str()); sliderBar("##opacity", &opacity, 0, 1, "%.2f", -1); }
            });
            section("Rendering", {"Cast shadows"}, [&](auto visible) {
                if (visible("Cast shadows")) { property(tr("Cast shadows").c_str()); toggleSwitch("##shadows", &shadows); }
            });
            space();
            textDim("%s", tr("Clear the query to restore every section.").c_str());
        }
        endCard();
        ImGui::TableNextColumn();
        if (beginCard("##filterMenu", tr("Menu filter").c_str(), 420)) {
            const auto action = filterMenu("##actions", menuFilter, actions, {300, false, false});
            dispatch(action);
            space();
            if (button(trLabel("Open searchable menu").c_str())) ImGui::OpenPopup("##searchableMenu");
            if (ImGui::BeginPopup("##searchableMenu")) {
                dispatch(filterMenu("##popup", popupFilter, actions));
                ImGui::EndPopup();
            }
        }
        endCard();
        ImGui::EndTable();
    }
}

#if defined(CYUI_HAS_PLOTS)
void plotsPage()
{
    sectionTitle(tr("Frequency response").c_str());
    textDim(tr("Logarithmic frequency axis, multiple series and host-defined units.").c_str());
    static std::vector<Point> low, bell, combined;
    if (low.empty()) {
        for (int i = 0; i < 360; ++i) {
            const double hz = 20 * std::pow(1000., i / 359.);
            const double shelf = 5.5 / (1 + std::pow(hz / 180., 3));
            const double peak = -7 * std::exp(-std::pow(std::log(hz / 1800.) / .42, 2));
            low.push_back({hz, shelf});
            bell.push_back({hz, peak});
            combined.push_back({hz, shelf + peak + 3 * std::exp(-std::pow(std::log(hz / 8500.) / .7, 2))});
        }
    }
    static PlotState response{{{20, 20000}, true}, {{-12, 12}, false}};
    const std::array<std::string, 3> legends{tr("Low shelf"), tr("Bell"), tr("Combined response")};
    PlotSeries series[]{{1, legends[0].c_str(), low, PlotStyle::line, 1},
                        {2, legends[1].c_str(), bell, PlotStyle::line, 2},
                        {3, legends[2].c_str(), combined, PlotStyle::line, 0}};
    PlotOptions options;
    options.formatX = [](double hz) {
        char label[32];
        std::snprintf(label, sizeof(label), hz >= 1000 ? "%.1f kHz" : "%.0f Hz", hz >= 1000 ? hz / 1000 : hz);
        return std::string(label);
    };
    options.formatY = [](double db) {
        char label[32];
        std::snprintf(label, sizeof(label), "%+.0f dB", db);
        return std::string(label);
    };
    plot("##response", response, {series, 3}, dp(0, 260), options);
    space();
    if (ImGui::BeginTable("##plotDetails", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        sectionTitle(tr("Distribution").c_str());
        static std::vector<Point> bins = [] {
            std::vector<double> samples;
            for (int i = 0; i < 1200; ++i)
                samples.push_back(std::sin(i * .13) * std::sin(i * .037));
            const auto counts = histogram(samples, {-1, 1}, 32);
            std::vector<Point> result;
            for (std::size_t i = 0; i < counts.size(); ++i)
                result.push_back({double(i), double(counts[i])});
            return result;
        }();
        static PlotState histogramView{{{0, 32}, false}, {{0, 160}, false}};
        const auto samplesLabel = tr("Samples");
        PlotSeries distribution{4, samplesLabel.c_str(), bins, PlotStyle::bars, 1};
        plot("##distribution", histogramView, {&distribution, 1}, dp(0, 190));
        ImGui::TableNextColumn();
        sectionTitle(tr("Heatmap").c_str());
        static std::vector<float> values = [] {
            std::vector<float> result(32 * 16);
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 32; ++x)
                    result[std::size_t(y * 32 + x)] = float(.5 + .5 * std::sin(x * .25) * std::cos(y * .3));
            return result;
        }();
        heatmap("##heatmap", values, 32, 16, {0, 1}, dp(0, 190));
        ImGui::EndTable();
    }
}
#endif

#if defined(CYUI_HAS_NODES)
std::vector<Node> translatedNodes(const std::vector<Node>& nodes)
{
    auto shown = nodes;
    for (auto& node : shown) {
        node.label = tr(node.label.c_str());
        for (auto& pin : node.inputs) pin.label = tr(pin.label.c_str());
        for (auto& pin : node.outputs) pin.label = tr(pin.label.c_str());
    }
    return shown;
}
void imageGraphPage(const GalleryResources& resources)
{
    sectionTitle(tr("Image-processing graph").c_str());
    textDim(tr("Host-prepared image previews inside node cards. CyUI displays the graph; the host performs processing.").c_str());
    static NodeState state;
    static std::vector<Node> nodes{
        {1, {24, 95}, "Image source", {}, {{11, "RGBA", 1}}, 1},
        {2, {350, 50}, "Color grade", {{21, "Image", 1}}, {{22, "RGBA", 0}}, 0},
        {3, {680, 150}, "Blur", {{31, "Image", 0}}, {{32, "RGBA", 2}}, 2},
        {4, {1010, 95}, "Output", {{41, "Image", 2}}, {}, 3}};
    static std::vector<NodeLink> links{{1, 11, 21, 1}, {2, 22, 31, 0}, {3, 32, 41, 2}};
    static ItemId nextLink = 4;
    const auto size = dp(0, 460);
    static bool framed = false;
    if (!framed) {
        state.canvas.view.fit({0, 0}, {1260, 370}, {ImGui::GetContentRegionAvail().x, size.y}, dp(24));
        state.selection = {2};
        framed = true;
    }
    NodeOptions options;
    options.width = 230;
    options.row = 160; // One pin row, with room below the labels for a host-drawn preview.
    options.snap = 8;
    options.drawBody = [&](const Node& node, const Rect& area) {
        const float zoom = float(state.canvas.view.zoom);
        const auto index = std::size_t(node.id - 1);
        const auto texture = resources.imagePreviews[index] ? resources.imagePreviews[index] : resources.texture;
        const ImVec2 a{area.min.x, area.min.y + dp(24) * zoom};
        const ImVec2 b{area.max.x - dp(8) * zoom, area.max.y - dp(8) * zoom};
        auto* draw = ImGui::GetWindowDrawList();
        if (texture)
            draw->AddImage(texture, a, b);
        else {
            draw->AddRectFilled(a, b, palette().bg3, dp(3));
            clippedText(draw, {a.x + dp(8), a.y + dp(8)}, b.x - a.x - dp(16),
                        palette().textFaint, tr("Host image preview").c_str());
        }
    };
    const auto shown = translatedNodes(nodes);
    const auto edit = nodeEditor("##imageGraph", state, shown, links, size, options);
    if (edit.moved.committed())
        for (const auto& move : edit.moved.value)
            for (auto& node : nodes)
                if (node.id == move.id) node.position = move.position;
    if (edit.connect)
        links.push_back({nextLink++, edit.connectFrom, edit.connectTo, 0});
    if (edit.linkRemoved || !edit.removeLinks.empty())
        links.erase(std::remove_if(links.begin(), links.end(),
                                   [&](const NodeLink &link) {
                                       return (edit.linkRemoved && link.id == edit.removeLink) ||
                                              std::find(edit.removeLinks.begin(), edit.removeLinks.end(),
                                                        link.id) != edit.removeLinks.end();
                                   }),
                    links.end());
    for (auto id : edit.removeNodes) {
        for (const auto &node : nodes)
            if (node.id == id)
                links.erase(std::remove_if(links.begin(), links.end(), [&](const NodeLink& link) {
                    const auto owns = [&](ItemId pin) {
                        return std::any_of(node.inputs.begin(), node.inputs.end(), [&](const NodePin& p) { return p.id == pin; }) ||
                               std::any_of(node.outputs.begin(), node.outputs.end(), [&](const NodePin& p) { return p.id == pin; });
                    };
                    return owns(link.from) || owns(link.to);
                }), links.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const Node& node) { return node.id == id; }), nodes.end());
    }
}

void nodesPage(const GalleryResources& resources)
{
    static int example = 0;
    if (resources.previewImageGraph)
        example = 1;
    const char *examples[]{"Signal graph", "Image graph"};
    segmented("##nodeExample", examples, 2, &example);
    if (example == 1) {
        imageGraphPage(resources);
        return;
    }
    sectionTitle(tr("Voice graph").c_str());
    textDim(tr("Drag nodes or connect pins. Ctrl: multi-select. Home: frame. Delete: remove. Execution is "
               "host-owned.")
                .c_str());
    textDim("%s",
            tr("Copy/paste nodes, groups, links and reroutes: Ctrl+C/V/D or right-click. Paste at pointer.")
                .c_str());
    static NodeState state;
    static std::vector<Node> nodes{
        {1, {30, 35}, "On note", {}, {{11, "Trigger", 3}, {12, "Pitch", 1}, {13, "Velocity", 2}}, 3},
        {2, {30, 270}, "Voice settings", {}, {{21, "Cutoff", 0}, {22, "Resonance", 0}, {23, "Level", 2}}, 0},
        {3,
         {330, 35},
         "Oscillator",
         {{31, "Trigger", 3}, {32, "Pitch", 1}, {33, "Velocity", 2}},
         {{34, "Audio", 0}},
         1},
        {4,
         {640, 55},
         "Filter",
         {{41, "Audio", 0}, {42, "Cutoff", 0}, {43, "Resonance", 0}},
         {{44, "Signal", 0}},
         0},
        {5, {640, 290}, "Gain", {{51, "Signal", 0}, {52, "Level", 2}}, {{53, "Output", 0}}, 2},
        {6, {950, 190}, "Stereo output", {{61, "Left", 0}, {62, "Right", 0}}, {}, 3}};
    static std::vector<NodeLink> links{{1, 11, 31, 3}, {2, 12, 32, 1}, {3, 13, 33, 2}, {4, 34, 41, 0},
                                       {5, 21, 42, 0}, {6, 22, 43, 0}, {7, 44, 51, 0}, {8, 23, 52, 2},
                                       {9, 53, 61, 0}, {10, 53, 62, 0}};
    static ItemId nextLink = 1000;
    static std::vector<NodeGroup> groups{{900, {-5, 5}, {570, 405}, "Voice controls", 1},
                                         {903, {15, 235}, {225, 170}, "Voice settings", 0}};
    static std::vector<NodeReroute> reroutes{{901, 5, {570, 250}}, {902, 1, {265, 95}}};
    const auto size = dp(0, 470);
    static bool framed = false;
    if (!framed) {
        state.canvas.view.fit({0, 0}, {1170, 425}, {ImGui::GetContentRegionAvail().x, size.y}, dp(24));
        framed = true;
    }
    if (ghostButton(trLabel("Arrange nodes").c_str()))
        for (const auto &move : arrangeNodes(nodes, links, {290, 145}))
            for (auto &node : nodes)
                if (node.id == move.id)
                    node.position = move.position;
    NodeOptions options;
    options.snap = 8;
    options.groups = groups;
    options.reroutes = reroutes;
    options.allocateId = []() { return nextLink++; };
    const auto shown = translatedNodes(nodes);
    const auto edit = nodeEditor("##nodes", state, shown, links, size, options);
    if (edit.groups.committed())
        groups = edit.groups.value;
    if (edit.reroutes.committed())
        reroutes = edit.reroutes.value;
    nodes.insert(nodes.end(), edit.created.nodes.begin(), edit.created.nodes.end());
    links.insert(links.end(), edit.created.links.begin(), edit.created.links.end());
    groups.insert(groups.end(), edit.created.groups.begin(), edit.created.groups.end());
    reroutes.insert(reroutes.end(), edit.created.reroutes.begin(), edit.created.reroutes.end());
    for (auto key : edit.removeGroups)
        groups.erase(std::remove_if(groups.begin(), groups.end(), [&](const auto &g) { return g.id == key; }),
                     groups.end());
    for (auto key : edit.removeReroutes)
        reroutes.erase(
            std::remove_if(reroutes.begin(), reroutes.end(), [&](const auto &r) { return r.id == key; }),
            reroutes.end());
    if (edit.moved.committed())
        for (const auto &move : edit.moved.value)
            for (auto &node : nodes)
                if (node.id == move.id)
                    node.position = move.position;
    if (edit.connect)
        links.push_back({nextLink++, edit.connectFrom, edit.connectTo, 0});
    if (edit.linkRemoved || !edit.removeLinks.empty())
        links.erase(std::remove_if(links.begin(), links.end(),
                                   [&](const NodeLink &link) {
                                       return (edit.linkRemoved && link.id == edit.removeLink) ||
                                              std::find(edit.removeLinks.begin(), edit.removeLinks.end(),
                                                        link.id) != edit.removeLinks.end();
                                   }),
                    links.end());
    for (auto id : edit.removeNodes) {
        for (const auto &node : nodes)
            if (node.id == id)
                links.erase(std::remove_if(
                                links.begin(), links.end(),
                                [&](const NodeLink &link) {
                                    const auto owns = [&](ItemId pin) {
                                        return std::any_of(node.inputs.begin(), node.inputs.end(),
                                                           [&](const NodePin &p) { return p.id == pin; }) ||
                                               std::any_of(node.outputs.begin(), node.outputs.end(),
                                                           [&](const NodePin &p) { return p.id == pin; });
                                    };
                                    return owns(link.from) || owns(link.to);
                                }),
                            links.end());
        nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const Node &node) { return node.id == id; }), nodes.end());
    }
    reroutes.erase(std::remove_if(reroutes.begin(), reroutes.end(),
                                  [&](const auto &route) {
                                      return std::none_of(links.begin(), links.end(), [&](const auto &link) {
                                          return route.link == link.id;
                                      });
                                  }),
                   reroutes.end());
    if (!state.clipboardError.empty())
        textColoured(palette().error, "%s", tr(state.clipboardError.c_str()).c_str());
}
#endif

#if defined(CYUI_HAS_ASSETS)
void assetsPage(const GalleryResources& resources)
{
    sectionTitle(tr("Content browser").c_str());
    textDim(tr("Folder navigation, thumbnail grid, selection and host-owned asset metadata.").c_str());
    static AssetState state = [] { AssetState value; value.folder = 10; value.selection = {103}; return value; }();
    const auto preview = [&](std::size_t index) {
        return resources.imagePreviews[index] ? resources.imagePreviews[index] : resources.texture;
    };
    const std::vector<AssetItem> items{
        {10, 0, "Textures", "Folder", 0, true}, {20, 0, "Materials", "Folder", 0, true},
        {30, 0, "Scenes", "Folder", 0, true}, {40, 0, "Audio", "Folder", 0, true},
        {101, 10, "Mountain lake", "Texture", preview(0)},
        {102, 10, "Warm grade", "Texture", preview(1)},
        {103, 10, "Soft landscape", "Texture", preview(2)},
        {104, 10, "Vignette", "Texture", preview(3)},
        {105, 10, "Sky reference", "Texture", preview(0)},
        {106, 10, "Ground detail", "Texture", preview(1)},
        {107, 10, "Backdrop", "Texture", preview(2)},
        {108, 10, "Preview output", "Texture", preview(3)},
        {201, 20, "Landscape", "Material", preview(1)},
        {202, 20, "Water surface", "Material", preview(0)},
        {301, 30, "Environment", "Scene", preview(3)},
        {401, 40, "Atmosphere", "Audio"}};
    if (!ImGui::BeginTable("##contentBrowser", 3, ImGuiTableFlags_SizingStretchProp)) return;
    ImGui::TableSetupColumn(trLabel("Folders").c_str(), ImGuiTableColumnFlags_WidthFixed, dp(170));
    ImGui::TableSetupColumn(trLabel("Items").c_str(), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn(trLabel("Selection").c_str(), ImGuiTableColumnFlags_WidthFixed, dp(285));
    ImGui::TableNextColumn();
    sectionTitle(tr("Content").c_str());
    if (ImGui::Selectable(trLabel("All folders").c_str(), state.folder == 0)) { state.folder = 0; state.selection.clear(); }
    for (const auto& folder : items) {
        if (!folder.folder) continue;
        ImGui::PushID(int(folder.id));
        if (ImGui::Selectable(folder.label.c_str(), state.folder == folder.id)) {
            state.folder = folder.id;
            state.query.clear();
            state.selection.clear();
        }
        ImGui::PopID();
    }
    space();
    badge(tr("HOST COLLECTION").c_str(), Tone::info);
    ImGui::TableNextColumn();
    const auto result = assetBrowser("##browser", state, items, dp(0, 430));
    if (result.action != AssetAction::none)
        notify(tr("Asset action requested; the host handles this operation.").c_str(), Tone::info);
    ImGui::TableNextColumn();
    sectionTitle(tr("Selection").c_str());
    const AssetItem* chosen = nullptr;
    if (!state.selection.empty())
        for (const auto& item : items)
            if (item.id == state.selection.front()) chosen = &item;
    if (chosen) {
        if (chosen->thumbnail)
            ImGui::Image(ImTextureRef(chosen->thumbnail), {ImGui::GetContentRegionAvail().x, dp(170)});
        heading(chosen->label.c_str(), 3);
        badge(chosen->type.c_str(), Tone::accent);
        space();
        if (beginProperties("##assetMetadata")) {
            property(tr("Resolution").c_str()); ImGui::TextUnformatted(tr("256 x 160").c_str());
            property(tr("Format").c_str()); ImGui::TextUnformatted(tr("RGBA8").c_str());
            property(tr("Color space").c_str()); ImGui::TextUnformatted(tr("sRGB").c_str());
            property(tr("Ownership").c_str()); ImGui::TextUnformatted(tr("Application").c_str());
            endProperties();
        }
        space();
        if (primaryButton(trLabel("Open asset").c_str())) notify(tr("Open requested; no file is read by CyUI.").c_str());
    } else {
        textDim(tr("Select an asset to inspect its metadata.").c_str());
    }
    ImGui::EndTable();
}
#endif

#if defined(CYUI_HAS_INSPECTOR)
void virtualArraysPage() {
    textDim("%s",
            tr("Only visible rows are drawn. Right-click an element to copy, paste, duplicate or move it.")
                .c_str());
    static int kind = 6, count = 1, builtKind = -1, builtCount = -1;
    const char *kinds[]{"Bool",    "Int64", "Float", "Double",        "Text",    "Vector2",    "Vector3",
                        "Vector4", "Color", "Asset", "Nested arrays", "Objects", "Custom type", "Set", "Map", "Optional", "Enum", "Flags"};
    const char *counts[]{"100", "10000", "100000"};
    ImGui::SetNextItemWidth(dp(160));
    combo(trLabel("Element type").c_str(), &kind, kinds, 18);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(dp(110));
    combo(trLabel("Element count").c_str(), &count, counts, 3);
    static std::vector<Property> fields(1);
    static InspectorState state;
    static std::uint64_t revision = 0;
    if (kind != builtKind || count != builtCount) {
        std::vector<PropertyValue> prototypes{true,
                                              std::int64_t(9007199254740993LL),
                                              2.56f,
                                              2.56,
                                              std::string("Element"),
                                              PropertyVector{{2.56, 5}},
                                              PropertyVector{{2.56, 5, 8}},
                                              PropertyVector{{2.56, 5, 8, 10}},
                                              PropertyColor{{.35f, .65f, .8f, 1}},
                                              PropertyAsset{"Texture", "/Game/Textures/Terrain"},
                                              makePropertyArray(PropertyVector{{1, 2, 3}}, 3),
                                              PropertyObject{"SpawnPoint",
                                                             {{"Name", std::string("Spawn")},
                                                              {"Position", PropertyVector{{1, 2, 3}}},
                                                              {"Weights", makePropertyArray(.5f, 3)}}},
                                              PropertyCustom{"Angle", "45"}, makePropertySet(std::int64_t(0)), makePropertyMap(std::string{}, 0.),
                                              makePropertyOptional(2.56), PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "Medium"}, {2, "High"}}},
                                              PropertyEnum{"Channels", 3, {{1, "A"}, {2, "B"}, {4, "C"}}, true}};
        auto &object = std::get<PropertyObject>(prototypes[11]);
        object.metadata["Name"].readOnly = true;
        object.metadata["Position"].bounded = true; object.metadata["Position"].minimum = 0; object.metadata["Position"].maximum = 100;
        auto &field = fields[0];
        field.id = 900;
        field.label = "Values";
        field.value = makePropertyArray(prototypes[kind], std::size_t(std::stoul(counts[count])));
        builtKind = kind;
        builtCount = count;
        ++revision;
    }
    InspectorOptions options;
    options.arrayHeight = 280;
    options.browseAssetAt = [](ItemId, const std::vector<std::string> &path) {
        notify((tr("Asset element requested") + ": " + (path.empty() ? "" : path.front())).c_str());
    };
    options.editCustom = [](ItemId, const std::vector<std::string> &, const PropertyCustom &value) {
        if (button((value.json + " deg###angle").c_str()))
            return Edit<PropertyCustom>{
                EditPhase::commit, {value.type, std::to_string(std::fmod(std::stod(value.json) + 15, 360))}};
        return Edit<PropertyCustom>{};
    };
    for (const auto &event : inspector("##virtualArrays", state, fields, revision, {}, options))
        if (event.edit.committed()) {
            fields[0].value = event.edit.value;
            ++revision;
        }
    std::size_t drawn = 0;
    for (const auto &list : state.lists)
        drawn += list.second.renderedRows;
    textDim(tr("Visible rows: %zu / %zu").c_str(), drawn,
            std::get<PropertyArray>(fields[0].value).values.size());
    textDim("%s", tr("Nested arrays and objects open a detail editor. Custom types use a host callback or "
                     "the JSON editor.")
                      .c_str());
}
PropertyValue uniqueExampleValue(PropertyValue value, std::size_t index) {
    std::visit(
        [&](auto &element) {
            using T = std::decay_t<decltype(element)>;
            if constexpr (std::is_same_v<T, bool>)
                element = index != 0;
            else if constexpr (std::is_same_v<T, std::int64_t>)
                element = std::int64_t(index);
            else if constexpr (std::is_floating_point_v<T>)
                element = T(index) + T(.25);
            else if constexpr (std::is_same_v<T, std::string>)
                element = "Element " + std::to_string(index);
            else if constexpr (std::is_same_v<T, PropertyVector>)
                element.values.front() = double(index);
            else if constexpr (std::is_same_v<T, PropertyColor>)
                element.rgba[0] = float(index) / 100000.f;
            else if constexpr (std::is_same_v<T, PropertyAsset>)
                element.path += "/" + std::to_string(index);
            else if constexpr (std::is_same_v<T, PropertyCustom>)
                element.json = std::to_string(index);
            else if constexpr (std::is_same_v<T, PropertyObject>)
                element.fields["Name"] = std::string("Record ") + std::to_string(index);
            else if constexpr (std::is_same_v<T, PropertyEnum>)
                element.value =
                    element.flags ? std::int64_t(index) : element.items[index % element.items.size()].first;
            else if constexpr (std::is_same_v<T, PropertyOptional>) {
                element.value = index ? std::make_unique<PropertyValue>(
                                            uniqueExampleValue(*element.elementTemplate, index - 1))
                                      : nullptr;
            } else if constexpr (std::is_same_v<T, PropertyArray> || std::is_same_v<T, PropertySet>) {
                auto prototype = element.elementTemplate ? *element.elementTemplate : element.values.front();
                element.values = {uniqueExampleValue(std::move(prototype), index)};
            } else if constexpr (std::is_same_v<T, PropertyMap>) {
                element.entries = {{uniqueExampleValue(*element.keyTemplate, index), *element.valueTemplate}};
            }
        },
        value);
    return value;
}
void associativeCollectionsPage(bool maps) {
    struct Example {
        int kind = 1, key = 4, count = 1, builtKind = -1, builtKey = -1, builtCount = -1;
        std::vector<Property> fields{1};
        InspectorState state;
        std::uint64_t revision = 0;
    };
    static std::array<Example, 2> examples;
    auto &example = examples[maps ? 1 : 0];
    if (maps && example.builtKind < 0)
        example.kind = 5;
    textDim("%s",
            tr("Set elements and Map keys are unique. Conflicting edits and paste are rejected atomically.")
                .c_str());
    const char *kinds[]{"Bool",  "Int64",   "Float",         "Double",   "Text", "Vector3", "Color",
                        "Asset", "Objects", "Nested arrays", "Optional", "Enum", "Flags"};
    const char *counts[]{"100", "10000", "100000"};
    ImGui::SetNextItemWidth(dp(150));
    combo(trLabel(maps ? "Value type" : "Element type").c_str(), &example.kind, kinds, 13);
    if (maps) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(dp(150));
        combo(trLabel("Key type").c_str(), &example.key, kinds, 13);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(dp(100));
    combo(trLabel("Element count").c_str(), &example.count, counts, 3);
    if (example.kind != example.builtKind || example.key != example.builtKey ||
        example.count != example.builtCount) {
        PropertyMetadata locked;
        locked.readOnly = true;
        PropertyMetadata limited;
        limited.bounded = true;
        limited.minimum = 0;
        limited.maximum = 10;
        std::vector<PropertyValue> prototypes{
            true,
            std::int64_t(0),
            2.56f,
            2.56,
            std::string{},
            PropertyVector{{2.56, 5, 8}},
            PropertyColor{{.35f, .65f, .8f, 1}},
            PropertyAsset{"Texture", "/Game/Textures"},
            PropertyObject{"Record",
                           {{"Name", std::string("Record")},
                            {"Strength", 2.56},
                            {"Tint", PropertyColor{{.35f, .65f, .8f, 1}}}},
                           {{"Name", locked}, {"Strength", limited}}},
            makePropertyArray(PropertyVector{{1, 2, 3}}, 2),
            makePropertyOptional(2.56),
            PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "Medium"}, {2, "High"}}},
            PropertyEnum{"Channels", 3, {{1, "A"}, {2, "B"}, {4, "C"}}, true}};
        auto number = std::size_t(std::stoul(counts[example.count]));
        const auto uniqueKind = maps ? example.key : example.kind;
        if (uniqueKind == 0)
            number = std::min(number, std::size_t(2));
        if (uniqueKind == 11)
            number = std::min(number, std::size_t(3));
        if (uniqueKind == 12)
            number = std::min(number, std::size_t(8));
        auto &field = example.fields[0];
        field.id = 901;
        field.label = maps ? "Map" : "Set";
        if (maps) {
            auto map = makePropertyMap(prototypes[example.key], prototypes[example.kind]);
            for (std::size_t n = 0; n < number; ++n)
                map.entries.push_back(
                    {uniqueExampleValue(prototypes[example.key], n), prototypes[example.kind]});
            field.value = std::move(map);
        } else {
            auto set = makePropertySet(prototypes[example.kind]);
            for (std::size_t n = 0; n < number; ++n)
                set.values.push_back(uniqueExampleValue(prototypes[example.kind], n));
            field.value = std::move(set);
        }
        example.builtKind = example.kind;
        example.builtKey = example.key;
        example.builtCount = example.count;
        ++example.revision;
    }
    InspectorOptions options;
    options.arrayHeight = 280;
    for (const auto &event :
         inspector("##associative", example.state, example.fields, example.revision, {}, options))
        if (event.edit.committed()) {
            example.fields[0].value = event.edit.value;
            ++example.revision;
        }
    std::size_t drawn = 0;
    for (const auto &list : example.state.lists)
        drawn += list.second.renderedRows;
    const auto count = maps ? std::get<PropertyMap>(example.fields[0].value).entries.size()
                            : std::get<PropertySet>(example.fields[0].value).values.size();
    textDim(tr("Visible rows: %zu / %zu").c_str(), drawn, count);
    textDim("%s", tr("Ctrl/Shift select rows. Edit selection applies a formula or typed JSON to the "
                     "selection. Add validates uniqueness.")
                      .c_str());
}
void advancedInspectorPage() {
    textDim(
        "%s",
        tr("Right-click a field or category to copy/paste JSON. Double-click a number to enter Value*100.")
            .c_str());
    static std::vector<Property> first, second;
    static InspectorState state;
    static bool multi = false;
    static std::uint64_t revision = 0;
    if (first.empty()) {
        auto field = [](ItemId id, const char *key, const char *category, PropertyValue value) {
            Property p;
            p.id = id;
            p.key = key;
            p.label = key;
            p.category = category;
            p.value = std::move(value);
            p.defaultValue = p.value;
            return p;
        };
        first = {field(1, "Strength", "Material", 2.56),
                 field(2, "Enabled", "Material", true),
                 field(3, "Tint", "Material", PropertyColor{{.4f, .6f, .8f, 1}}),
                 field(4, "Position", "Transform", PropertyVector{{0, 25, 100}}),
                 field(5, "Scale", "Transform", PropertyVector{{1, 1, 1}}),
                 field(6, "Texture", "Resources", PropertyAsset{"Texture", "/Game/Textures/Terrain"}),
                 field(7, "Tags", "Resources",
                       PropertyArray{{std::string("Environment"), std::string("Outdoor")}})};
        first[0].bounded = true;
        first[0].minimum = 0;
        first[0].maximum = 10;
        first[0].help = "Try Value*100: 2.56 becomes 10 because the maximum is 10.";
        second = first;
        second[0].value = 5.;
        second[3].value = PropertyVector{{10, 25, 100}};
    }
    if (ImGui::Checkbox(trLabel("Edit two objects").c_str(), &multi))
        ++revision;
    auto fields = multi ? commonProperties({first, second}) : first;
    InspectorOptions options;
    options.browseAsset = [](ItemId) { notify(tr("The host opens its asset selector here.").c_str()); };
    static TextFilter filter;
    filterBox("##advancedFilter", filter);
    options.filter = &filter;
    for (const auto &e : inspector("##advanced", state, fields, revision, {}, options))
        if (e.edit.committed()) {
            for (auto &p : first)
                if (p.id == e.id)
                    p.value = e.edit.value;
            if (multi)
                for (auto &p : second)
                    if (p.id == e.id)
                        p.value = e.edit.value;
            ++revision;
        }
    if (ghostButton(trLabel("Copy category as JSON").c_str()))
        ImGui::SetClipboardText(propertyCategoryToJson(first, "Material").c_str());
}
void inspectorPage(const GalleryResources& resources)
{
    static int view = 1;
    const char *views[]{"Material example", "Advanced inspector"};
    segmented("##inspectorExample", views, 2, &view);
    if (view == 1) {
        advancedInspectorPage();
        return;
    }
    sectionTitle(tr("Details inspector").c_str());
    textDim(tr("Grouped editor properties with typed values, validation, read-only fields and committed edits.").c_str());
    static std::vector<Property> identity, surface, sampling, rendering;
    static std::array<InspectorState, 4> states;
    if (identity.empty()) {
        const auto field = [](ItemId id, const char* label, PropertyValue value, bool readOnly = false) {
            Property item; item.id = id; item.label = label; item.value = std::move(value); item.readOnly = readOnly; return item;
        };
        identity = {field(1, "Name", std::string("Landscape material")),
                    field(2, "Enabled", true), field(3, "Asset ID", std::int64_t(9007199254740993LL), true)};
        surface = {field(11, "Shader", std::string("Lit")), field(12, "Roughness", .38),
                   field(13, "Metallic", .12), field(14, "Opacity", 1.)};
        surface[0].choices = {"Lit", "Unlit", "Subsurface"};
        for (std::size_t i = 1; i < surface.size(); ++i) { surface[i].bounded = true; surface[i].minimum = 0; surface[i].maximum = 1; }
        sampling = {field(21, "Filtering", std::string("Trilinear")), field(22, "UV scale X", 1.),
                    field(23, "UV scale Y", 1.), field(24, "Use normal map", true)};
        sampling[0].choices = {"Nearest", "Bilinear", "Trilinear"};
        rendering = {field(31, "Cast shadows", true), field(32, "Two-sided", false),
                     field(33, "Sort priority", std::int64_t(2))};
    }
    if (!ImGui::BeginTable("##details", 2, ImGuiTableFlags_SizingStretchProp)) return;
    ImGui::TableSetupColumn(trLabel("Preview").c_str(), ImGuiTableColumnFlags_WidthFixed, dp(330));
    ImGui::TableSetupColumn(trLabel("Properties").c_str(), ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextColumn();
    sectionTitle(tr("Selected material").c_str());
    const auto texture = resources.imagePreviews[1] ? resources.imagePreviews[1] : resources.texture;
    if (texture) ImGui::Image(ImTextureRef(texture), {ImGui::GetContentRegionAvail().x, dp(205)});
    heading(tr("Landscape material").c_str(), 3);
    badge(tr("MATERIAL INSTANCE").c_str(), Tone::accent);
    space();
    ImGui::PushTextWrapPos(0);
    textDim(tr("Example editor object. The application owns the asset, preview renderer and Undo history.").c_str());
    ImGui::PopTextWrapPos();
    ImGui::TableNextColumn();
    static TextFilter filter;
    FilterOptions filtering;
    filtering.hint = "Filter properties...";
    if (filterBox("##detailsFilter", filter, filtering))
        for (auto& state : states) state.drafts.clear(); // example previews are not applied to the model
    bool anyGroup = false;
    const auto group = [&](const char* title, const char* id, InspectorState& state, std::vector<Property>& fields) {
        const bool any = std::any_of(fields.begin(), fields.end(), [&](const auto& field) { return propertyMatches(filter, field, title); });
        anyGroup |= any;
        if (beginFilteredSection(title, filter, any)) {
            for (const auto& change : inspector(id, state, fields, 0, {}, {&filter, title}))
                if (change.edit.committed())
                    for (auto& field : fields)
                        if (field.id == change.id) field.value = change.edit.value;
            endSection();
        }
    };
    group("Identity", "##identity", states[0], identity);
    group("Surface", "##surface", states[1], surface);
    group("Texture sampling", "##sampling", states[2], sampling);
    group("Rendering", "##rendering", states[3], rendering);
    if (!anyGroup) textDim("%s", tr("No matching properties").c_str());
    ImGui::EndTable();
}
#endif

#if defined(CYUI_HAS_AUDIOVIEWS)
void spectralViewsPage()
{
    static int colors = 0;
    static SpectrogramHistory history;
    static SpectrogramState horizontal, vertical;
    static double time = 0;
    static StereoDensityState stereo[3];
    static SpatialScopeState spatial;
    const char *ramps[]{"Intensity", "Frequency", "Monochrome", "Theme colors"};
    ImGui::SetNextItemWidth(dp(150)); combo(trLabel("Color map").c_str(), &colors, ramps, 4);
    ImGui::SameLine();
    if (ImGui::Checkbox(trLabel("Follow latest").c_str(), &horizontal.followLatest)) vertical.followLatest = horizontal.followLatest;
    ImGui::SameLine();
    if (ImGui::SmallButton(trLabel("Clear persistence").c_str())) { spatial.density.clear(); for (auto &s : stereo) s.density.clear(); }
    const RampStyle styles[]{RampStyle::intensity, RampStyle::frequency, RampStyle::monochrome, RampStyle::themed};
    const ColorRamp ramp{styles[colors], {}};
    auto append = [&] {
        std::vector<float> column;
        const double fundamental = 180 + 70 * std::sin(time * .65);
        for (int f = 0; f < 128; ++f) {
            const double hz = 20 * std::pow(1000., (f + .5) / 128.);
            double level = -98 + 3 * std::sin(f * .8 + time);
            for (int h = 1; h <= 15; ++h) level = std::max(level, -12 - h * 3 - 65 * std::pow(std::log(hz / (fundamental * h)) / .16, 2));
            column.push_back(float(level));
        }
        history.append(column); time += .0625;
    };
    static bool initialized = false;
    if (!initialized) { history.reset(128, 128, .0625, {20, 20000}, true); for (int n = 0; n < 128; ++n) append(); initialized = true; }
    append();
    std::vector<Point> left, right;
    const auto level = [](double hz) { return -72 + 36 * std::exp(-std::pow(std::log(hz / 150.) / 1.1, 2)) + 29 * std::exp(-std::pow(std::log(hz / 2200.) / 1.8, 2)); };
    for (int n = 0; n < 240; ++n) {
        const double hz = 20 * std::pow(1000., n / 239.);
        left.push_back({hz, level(hz) + 3 * std::sin(n * .7)});
        right.push_back({hz, level(hz) - 7 + 2 * std::cos(n * .4)});
    }
    PlotSeries spectra[]{{1, "L", left, PlotStyle::area}, {2, "R", right, PlotStyle::area, 1}};
    for (auto &line : spectra) { line.baseline = -90; line.fillAlpha = .65f; line.gradient = true; }
    spectra[0].colorByX = true; spectra[0].ramp = {RampStyle::frequency, {}};
    static PlotState spectrumView{{{20, 20000}, true}, {{-90, 0}, false}}, bandsView = spectrumView;
    if (!ImGui::BeginTable("##spectral", 2, ImGuiTableFlags_SizingStretchSame)) return;
    ImGui::TableNextColumn(); sectionTitle(tr("Filled spectra and gradients").c_str());
    spectrum("##filled", spectrumView, {spectra, 2}, dp(0, 150));
    ImGui::TableNextColumn(); sectionTitle(tr("Band spectrum and FFT overlay").c_str());
    std::vector<SpectrumBand> bands;
    for (int n = 0; n < 30; ++n) { const double a = 20 * std::pow(1000., n / 30.), b = 20 * std::pow(1000., (n + 1) / 30.); bands.push_back({{a, b}, level(std::sqrt(a * b)) - 5}); }
    PlotSeries outline{2, "FFT", left, PlotStyle::line, 1}; BandSpectrumOptions bandOptions; bandOptions.label = "RTA";
    bandSpectrum("##bands", bandsView, bands, {&outline, 1}, dp(0, 150), bandOptions);
    ImGui::TableNextColumn(); sectionTitle(tr("Vertical scrolling spectrogram").c_str());
    const auto data = history.view();
    SpectrogramOptions image; image.ramp = ramp; image.legend = true; image.colorByFrequency = colors == 1;
    image.orientation = SpectrogramOrientation::timeVertical; image.reverseTime = true;
    spectrogram("##vertical", vertical, data, dp(0, 164), image);
    ImGui::TableNextColumn(); sectionTitle(tr("Horizontal scrolling spectrogram").c_str());
    image.orientation = SpectrogramOrientation::timeHorizontal; image.reverseTime = false;
    spectrogram("##horizontal", horizontal, data, dp(0, 164), image);
    ImGui::TableNextColumn(); sectionTitle(tr("Spatial stereo spectrogram").c_str());
    std::vector<SpatialSample> cloud;
    const double phase = ImGui::GetTime();
    for (int f = 0; f < 128; ++f) for (int n = 0; n < 8; ++n) {
        const double spread = .025 + .6 * f / 127.;
        const double pan = .15 * std::sin(f * .08 + phase) + spread * std::sin(n * 1.7 + f * .3 + phase);
        cloud.push_back({20 * std::pow(1000., f / 127.), pan, -12. - f * .45 + 6 * std::sin(f * .2 + n)});
    }
    SpatialScopeOptions spatialOptions; spatialOptions.persistence.exposure = .06f;
    spatialOptions.persistence.resolution = 160; spatialOptions.persistence.pointRadius = 3;
    spatialScope("##spatial", spatial, cloud, std::uint64_t(ImGui::GetFrameCount()), dp(0, 154), spatialOptions);
    ImGui::TableNextColumn(); sectionTitle(tr("Persistent vectorscope").c_str());
    const char *names[]{"Stereo", "Mono", "Out of phase"};
    for (int k = 0; k < 3; ++k) {
        ImGui::PushID(k); ImGui::BeginGroup();
        std::vector<Point> trace;
        for (int n = 0; n < 720; ++n) {
            const double l = .85 * std::sin(n * .08 + phase), r = k == 1 ? l : (k == 2 ? -l : .75 * std::sin(n * .08 + phase + .75));
            trace.push_back({l, r});
        }
        StereoDensityOptions scopeOptions; scopeOptions.persistence.axes = false;
        stereoDensity("##density", stereo[k], trace, std::uint64_t(ImGui::GetFrameCount()), dp(145, 134), scopeOptions);
        textDim("%s", tr(names[k]).c_str()); ImGui::EndGroup(); ImGui::PopID();
        if (k != 2) ImGui::SameLine(0, dp(12));
    }
    ImGui::EndTable();
}

void audioAnalysisPage()
{
    textDim(tr("Prepared audio displays. Drag EQ handles; Escape cancels. Wheel zooms, middle drag pans.").c_str());
    static std::vector<float> matrix;
    static std::vector<Point> momentary, shortTerm, integrated, coherence, transfer;
    if (matrix.empty()) {
        for (int t = 0; t < 192; ++t)
            for (int f = 0; f < 96; ++f) {
                const double hz = 20 * std::pow(1000., (f + .5) / 96.);
                const double fundamental = 180 + 100 * std::sin(t * .04);
                double v = -92 + 5 * std::sin(t * .3 + f * .71);
                for (int h = 1; h <= 12; ++h)
                    v = std::max(v, -15 - h * 3 - 60 * std::pow(std::log(hz / (fundamental * h)) / .13, 2));
                matrix.push_back(float(v));
            }
        for (int n = 0; n <= 120; ++n) {
            momentary.push_back({double(n), -19 + 4 * std::sin(n * .12) + std::sin(n * .67)});
            shortTerm.push_back({double(n), -19 + 2.5 * std::sin(n * .12 - .7)});
            integrated.push_back({double(n), -21 + 1.7 * (1 - std::exp(-n / 35.))});
            coherence.push_back({double(n), .55 + .3 * std::sin(n * .08)});
            const double input = -60 + n * .5;
            transfer.push_back({input, input < -18 ? input : -18 + (input + 18) / 4});
        }
    }
    static PlotState spectrogramView{{{0, 8}, false}, {{20, 20000}, true}};
    static PlotState loudnessView{{{0, 120}, false}, {{-60, 0}, false}};
    static PlotState correlationView{{{0, 120}, false}, {{-1, 1}, false}};
    static PlotState transferView{{{-60, 0}, false}, {{-60, 0}, false}};
    static ResponseState response;
    static std::vector<ResponsePoint> bands{{1, 120, 4}, {2, 1800, -7}, {3, 8000, 3}};
    static bool resetLoudness = false, resetPeaks = false;
    if (!ImGui::BeginTable("##analysis", 2, ImGuiTableFlags_SizingStretchSame)) return;
    ImGui::TableNextColumn(); sectionTitle(tr("Spectrogram").c_str());
    spectrogram("##spectrogram", spectrogramView, {matrix, 192, 96, {0, 8}, {20, 20000}, true}, dp(0, 192));
    ImGui::TableNextColumn(); sectionTitle(tr("EQ and phase response").c_str());
    std::vector<Point> gain, phase;
    const auto &preview = response.dragging ? response.preview : bands;
    for (int n = 0; n < 240; ++n) {
        const double hz = 20 * std::pow(1000., n / 239.);
        double db = 0, angle = 0;
        for (const auto &band : preview) {
            const double x = std::log(hz / band.frequency) / .5, bell = std::exp(-x * x);
            db += band.gain * bell; angle += band.gain * x * bell * 5;
        }
        gain.push_back({hz, db}); phase.push_back({hz, angle});
    }
    const auto gainLabel = tr("Gain"), phaseLabel = tr("Phase"), m = tr("Momentary"), s = tr("Short-term"), in = tr("Integrated");
    PlotSeries gainCurve{1, gainLabel.c_str(), gain, PlotStyle::line, 0}, phaseCurve{2, phaseLabel.c_str(), phase, PlotStyle::line, 1};
    const auto edit = filterResponse("##response", response, {&gainCurve, 1}, {&phaseCurve, 1}, bands, dp(0, 192));
    if (edit.points.committed()) bands = edit.points.value;
    ImGui::TableNextColumn(); sectionTitle(tr("Loudness").c_str());
    PlotSeries histories[]{{1, m.c_str(), momentary, PlotStyle::line, 1}, {2, s.c_str(), shortTerm, PlotStyle::line, 2}, {3, in.c_str(), integrated, PlotStyle::line, 0}};
    LoudnessData loudnessData{-16.2, -18.1, resetLoudness ? missingAudioValue : -19.3, -25, -17, {histories, 3}};
    if (loudness("##loudness", loudnessView, loudnessData, dp(0, 190)).resetRequested) resetLoudness = true;
    ImGui::TableNextColumn(); sectionTitle(tr("Dynamics transfer").c_str());
    const auto outputLabel = tr("Output");
    PlotSeries curve{1, outputLabel.c_str(), transfer, PlotStyle::line, 0};
    dynamics("##dynamics", transferView, {{&curve, 1}, -9, -15.75, 6.75}, dp(0, 190));
    ImGui::TableNextColumn(); sectionTitle(tr("True peak").c_str());
    TruePeakChannel channels[]{{"L", -1.3, resetPeaks ? -1.3 : .4, resetPeaks ? 0u : 3u}, {"R", -2.1, resetPeaks ? -2.1 : -.6, 0}};
    if (truePeak("##truePeak", {channels, 2}, dp(0, 126)).resetRequested) resetPeaks = true;
    ImGui::TableNextColumn(); sectionTitle(tr("Correlation and phase").c_str());
    const auto correlationLabel = tr("Correlation");
    PlotSeries correlationCurve{1, correlationLabel.c_str(), coherence, PlotStyle::line, 2};
    correlation("##correlation", correlationView, .72, {&correlationCurve, 1}, dp(0, 126));
    ImGui::EndTable();
}

void envelopesPage()
{
    textDim(tr("Host-prepared curves. CyUI draws the envelope, phases and live markers.").c_str());
    static float attack = .015f, hold = 0, decay = .6f, sustain = .35f, release = .25f, bend = 3;
    static bool expand = true, gradient = false;
    if (ImGui::BeginTable("##envelopeControls", 3, ImGuiTableFlags_SizingStretchSame)) {
        auto control = [](const char *label, float &value, float maximum) {
            ImGui::TableNextColumn(); textDim(tr(label).c_str());
            ImGui::PushID(label); sliderBar("##value", &value, 0, maximum, "%.3f", -1); ImGui::PopID();
        };
        control("Attack", attack, 1); control("Hold", hold, 1); control("Decay", decay, 2);
        control("Sustain", sustain, 1); control("Release", release, 2); control("Curvature", bend, 6);
        ImGui::EndTable();
    }
    ImGui::Checkbox(trLabel("Expand short phases").c_str(), &expand); ImGui::SameLine();
    ImGui::Checkbox(trLabel("Gradient fill").c_str(), &gradient);
    static EnvelopeEditState decayEdit;
    const double a = attack, h = hold, held = .35, r = release;
    const auto preview = decayEdit.preview.position;
    const double d = decayEdit.original.id ? std::clamp(preview.x - a - h, 0., 2.) : decay;
    const double previewLevel = decayEdit.original.id ? preview.y : sustain;
    // Gallery-owned interpolation illustrates the host contract. CyUI receives the samples.
    auto prepare = [&](double level) {
        std::vector<Point> points;
        for (int i = 0; i <= 32; ++i) points.push_back({a * i / 32, i / 32.});
        points.push_back({a + h, 1}); // intentional repeated peak when Hold == 0
        for (int i = 1; i <= 32; ++i)
            points.push_back({a + h + d * i / 32, level + (1 - level) * std::pow(1 - i / 32., std::max(.1f, bend))});
        points.push_back({a + h + d + held, level});
        for (int i = 1; i <= 32; ++i) points.push_back({a + h + d + held + r * i / 32, level * (1 - i / 32.)});
        points.push_back(points.back()); // repeated zero endpoint is also safe
        return points;
    };
    const auto attackLabel = tr("Attack"), holdLabel = tr("Hold"), decayLabel = tr("Decay"),
               sustainLabel = tr("Sustain"), releaseLabel = tr("Release");
    const EnvelopeStage stages[]{
        {{0, a}, attackLabel.c_str()}, {{a, a + h}, holdLabel.c_str()},
        {{a + h, a + h + d}, decayLabel.c_str()}, {{a + h + d, a + h + d + held}, sustainLabel.c_str()},
        {{a + h + d + held, a + h + d + held + r}, releaseLabel.c_str()}};
    EnvelopeOptions options; options.time = {0, a + h + d + held + r};
    options.timePower = expand ? .6 : 1; options.gradient = gradient; options.grid = true;
    auto points = prepare(previewLevel);
    EnvelopeData data{points, {stages, 5}};
    data.playhead = a + h + d * .45;
    data.level = previewLevel + (1 - previewLevel) * std::pow(.55, std::max(.1f, bend));
    sectionTitle("AHDSR");
    const float height = std::clamp(ImGui::GetContentRegionAvail().y - dp(190), dp(100), dp(220));
    const EnvelopeHandle handle{1, {a + h + d, previewLevel}, EnvelopeAxes::both};
    const auto edit = envelope("##ahdsr", decayEdit, data, {&handle, 1},
                               {ImGui::GetContentRegionAvail().x, height}, options);
    if (edit.edit.committed()) {
        decay = float(std::clamp(edit.edit.value.position.x - a - h, 0., 2.));
        sustain = float(edit.edit.value.position.y);
    }
    if (edit.hoveredHandle)
        ImGui::SetTooltip("%s", tr("Drag to edit decay and sustain").c_str());
    if (ImGui::BeginTable("##envelopeExamples", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn(); sectionTitle(tr("Zero sustain").c_str());
        auto zero = prepare(0); EnvelopeData zeroData{zero, {stages, 5}};
        auto instrument = options;
        instrument.curveColor = palette().success;
        instrument.levelLineColor = alpha(palette().info, .55f);
        instrument.levelDotColor = palette().info;
        instrument.margins = {4, 6, 4, 4};
        instrument.frameRounding = 4;
        instrument.borderWidth = 0;
        zeroData.level = .35;
        envelope("##zeroSustain", zeroData, dp(0, 150), instrument);
        ImGui::TableNextColumn(); sectionTitle(tr("Custom canvas: bipolar fill").c_str());
        {
            CanvasState canvas; CanvasOptions canvasOptions; canvasOptions.grid = canvasOptions.navigation = false;
            CanvasScope scope("##fillPrimitive", canvas, dp(0, 150), canvasOptions);
            const auto bounds = scope.frame().bounds; const auto room = bounds.size();
            const float baseline = bounds.min.y + room.y * .5f;
            std::vector<ImVec2> line;
            for (int i = 0; i <= 80; ++i)
                line.push_back({bounds.min.x + room.x * i / 80, baseline - room.y * .38f * float(std::sin(i * .12))});
            drawCurveFill(line, baseline, {palette().info, .25f, gradient});
            auto *draw = ImGui::GetWindowDrawList();
            draw->AddLine({bounds.min.x, baseline}, {bounds.max.x, baseline}, palette().grid, dp(1));
            draw->AddPolyline(line.data(), int(line.size()), palette().info, 0, dp(2));
        }
        ImGui::EndTable();
    }
}

void audioViewsPage()
{
    sectionTitle(tr("Stereo waveform").c_str());
    static std::vector<Peak> left, right;
    if (left.empty()) {
        std::vector<float> a, b;
        for (int i = 0; i < 48000; ++i) {
            const double envelope = .15 + .7 * std::abs(std::sin(i * .00015)) * std::exp(-i / 90000.);
            a.push_back(float(envelope * (std::sin(i * .045) + .2 * std::sin(i * .12))));
            b.push_back(float(envelope * (std::sin(i * .045 + .7) + .2 * std::sin(i * .13))));
        }
        left = makePeaks(a, 1600);
        right = makePeaks(b, 1600);
    }
    static WaveformState wave;
    WaveformData data{{0, 1}, {left, right}};
    waveform("##wave", wave, data, .42, dp(0, 150));
    if (ImGui::BeginTable("##audioMonitors", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        sectionTitle(tr("Spectrum").c_str());
        static std::vector<Point> input, output, signal, stereo;
        if (input.empty()) {
            for (int i = 0; i < 320; ++i) {
                const double hz = 20 * std::pow(1000., i / 319.);
                const double level = -65 + 33 * std::exp(-std::pow(std::log(hz / 1100.) / 1.5, 2)) +
                                     3 * std::sin(i * .2) + 1.5 * std::cos(i * .53);
                input.push_back({hz, level});
                output.push_back({hz, level - 6 * std::exp(-std::pow(std::log(hz / 1800.) / .4, 2))});
                signal.push_back({i / 319., .7 * std::sin(i * .055) + .18 * std::sin(i * .11)});
                stereo.push_back({.8 * std::sin(i * .065), .65 * std::sin(i * .065 + .65)});
            }
        }
        static PlotState frequencies{{{20, 20000}, true}, {{-90, 0}, false}};
        const auto inputLabel = tr("Input"), outputLabel = tr("Output");
        PlotSeries spectrumSeries[]{{1, inputLabel.c_str(), input, PlotStyle::line, 1}, {2, outputLabel.c_str(), output, PlotStyle::line, 0}};
        spectrum("##spectrum", frequencies, {spectrumSeries, 2}, dp(0, 185));
        ImGui::TableNextColumn();
        sectionTitle(tr("Oscilloscope").c_str());
        static PlotState scope{{{0, 1}, false}, {{-1, 1}, false}};
        const auto signalLabel = tr("Signal");
        PlotSeries trace{3, signalLabel.c_str(), signal, PlotStyle::line, 2};
        oscilloscope("##oscilloscope", scope, {&trace, 1}, dp(0, 185));
        ImGui::TableNextColumn();
        sectionTitle(tr("Stereo scope").c_str());
        static PlotState field{{{-1, 1}, false}, {{-1, 1}, false}};
        PlotSeries stereoSeries{4, "L / R", stereo, PlotStyle::points, 3};
        stereoScope("##stereo", field, {&stereoSeries, 1}, dp(0, 170));
        ImGui::TableNextColumn();
        sectionTitle(tr("Monitor controls").c_str());
#if defined(CYUI_HAS_AUDIO)
        static float gain = .65f, pan = -.15f, cutoff = 1800;
        knob("##monitorGain", &gain, 0, 1, .65f, {42, 0, "Gain"});
        ImGui::SameLine(0, dp(24));
        KnobStyle panStyle{42, palette().info, "Pan"};
        panStyle.flags = knobBipolar;
        knob("##monitorPan", &pan, -1, 1, 0, panStyle);
        ImGui::SameLine(0, dp(24));
        KnobStyle filterStyle{42, palette().success, "Cutoff", "%.0f", "Hz"};
        filterStyle.flags = knobLogarithmic;
        knob("##monitorCutoff", &cutoff, 20, 20000, 1800, filterStyle);
        meter("##monitorLevel", {ImGui::GetContentRegionAvail().x, dp(24)}, .75f, .62f, .42f, .38f, false);
#endif
        textDim(tr("Synthetic preview data. Processing and playback are supplied by the host.").c_str());
        ImGui::EndTable();
    }
}
#endif

#if defined(CYUI_HAS_CODE)
#if defined(CYUI_HAS_CODEEDITOR)
// Gallery-owned fixtures demonstrate the host contract; CyUI owns no source files.
struct PagedGalleryHost {
    PagedCodeEditor editor;
    std::string text = "// An editable, host-owned UTF-8 document\n"
                       "#include <algorithm>\n\n"
                       "float process(float sample, float gain) {\n"
                       "\t// Unicode: été 🛠\n"
                       "\treturn std::clamp(sample * gain, -1.0f, 1.0f);\n}\n";
    std::vector<std::pair<std::string, PagedCodeSelection>> undo, redo;
    std::uint64_t revision = 1, active = 0, jump = 500001;
    bool large = false;
    std::future<PagedCodePage> worker;
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::optional<PagedCodeRead> queued;
    std::size_t preparedRows = 0;
    std::vector<PagedCodeInput> recoveredInput;

    explicit PagedGalleryHost(bool virtualSource = false) : large(virtualSource) {
        editor.setDocument(snapshot());
        if (large) editor.scrollTo(499990);
    }
    PagedCodeDocument snapshot() const {
        return {large ? 2u : 1u, revision, large ? 1000000u : 1u + std::uint64_t(std::count(text.begin(), text.end(), '\n')),
                large ? 2u * 1024u * 1024u : 0u, large};
    }
    static std::vector<std::string_view> lines(const std::string &text) {
        std::vector<std::string_view> values;
        for (std::size_t at = 0; at <= text.size();) {
            const auto end = text.find('\n', at);
            values.emplace_back(text.data() + at, end == std::string::npos ? text.size() - at : end - at);
            if (end == std::string::npos) break;
            at = end + 1;
        }
        return values;
    }
    static PagedCodePage page(PagedCodeRead read, const std::string &source, bool large,
                               const std::shared_ptr<std::atomic<bool>> &cancel) {
        const auto small = large ? std::vector<std::string_view>{} : lines(source);
        auto line = [&](std::uint64_t number, std::uint64_t column, std::optional<std::uint64_t> reveal,
                        std::optional<std::uint64_t> *revealed) {
            PagedCodeLineBuilder build(number, column, read.columns, read.tabSize, reveal);
            auto append = [&](std::string_view chunk) {
                if (cancel->load() || !build.append(chunk)) throw std::runtime_error("Page preparation cancelled or invalid");
            };
            if (!large) append(small.at(std::size_t(number)));
            else if (number == 500003) {
                append("// A streamed long line: ");
                for (unsigned n = 0; n < 16384; ++n) append("value\t0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
            } else append("static const uint64 Value_" + std::to_string(number) + " = " + std::to_string(number) + "; // generated row");
            PagedCodeLine value;
            if (!build.finish(value)) throw std::runtime_error("Invalid source line");
            if (revealed) *revealed = build.revealedColumn();
            return value;
        };
        auto firstColumn = read.firstColumn;
        if (read.reveal) {
            std::optional<std::uint64_t> column;
            line(read.reveal->line, firstColumn, read.reveal->character, &column);
            if (!column) throw std::runtime_error("Invalid UTF-16 reveal position");
            if (*column < firstColumn || *column - firstColumn >= read.columns)
                firstColumn = *column > read.columns / 2 ? *column - read.columns / 2 : 0;
        }
        PagedCodePage result{read.id, read.document, read.revision, firstColumn, {}};
        const auto total = large ? 1000000u : std::uint64_t(small.size());
        const auto count = std::min<std::uint64_t>(read.lines, total - read.firstLine);
        const auto tokens = large ? std::vector<CodeToken>{} : highlightCode(source, CodeLanguage::cpp);
        for (std::uint64_t n = 0; n < count; ++n) {
            auto value = line(read.firstLine + n, firstColumn, {}, nullptr);
            if (large && value.line == 500003) {
                if (!value.text.empty()) value.tokens.push_back({0, value.text.size(), TokenRole::comment});
            } else if (large) value.tokens = highlightCode(value.text, CodeLanguage::cpp);
            else {
                const auto offset = std::uint64_t(small[std::size_t(value.line)].data() - source.data()) + value.firstByte;
                for (const auto &token : tokens) {
                    const auto begin = std::max<std::uint64_t>(offset, token.start);
                    const auto end = std::min<std::uint64_t>(offset + value.text.size(), token.start + token.length);
                    if (end > begin) value.tokens.push_back({std::size_t(begin - offset), std::size_t(end - begin), token.role});
                }
            }
            result.lines.push_back(std::move(value));
        }
        return result;
    }
    std::size_t byte(PagedCodePosition point) const {
        const auto rows = lines(text);
        if (point.line >= rows.size()) throw std::runtime_error("Stale selection");
        const auto row = rows[std::size_t(point.line)];
        const auto offset = std::size_t(row.data() - text.data());
        PagedCodeLineBuilder build(point.line, 0, 1, 4, point.character); PagedCodeLine value;
        if (!build.append(row) || !build.finish(value)) throw std::runtime_error("Invalid fixture line");
        if (point.character <= value.characters && build.revealedByte()) return offset + std::size_t(*build.revealedByte());
        throw std::runtime_error("Position is not a UTF-16 boundary");
    }
    static PagedCodePosition position(const std::string &text, std::size_t offset) {
        std::uint64_t line = 0; std::size_t start = 0;
        for (std::size_t n = 0; n < offset; ++n) if (text[n] == '\n') { ++line; start = n + 1; }
        PagedCodeLineBuilder builder(line, 0, 1); PagedCodeLine value;
        if (!builder.append(std::string_view(text).substr(start, offset - start)) || !builder.finish(value))
            throw std::runtime_error("Invalid fixture position");
        return {line, value.characters};
    }
    void apply(const PagedCodeReplacement &edit) {
        if (large || edit.document != 1 || edit.revision != revision) { editor.rejectEdit(edit.id, "Stale host revision"); return; }
        try {
            const auto first = byte(edit.selection.first()), last = byte(edit.selection.last());
            std::string replacement;
            for (std::size_t n = 0; n < edit.text.size(); ++n) {
                if (edit.text[n] == '\r') { replacement += '\n'; if (n + 1 < edit.text.size() && edit.text[n + 1] == '\n') ++n; }
                else replacement += edit.text[n];
            }
            if (text.size() - (last - first) + replacement.size() > 128 * 1024)
                throw std::runtime_error("The small editable Gallery fixture is limited to 128 KiB");
            auto candidate = text;
            candidate.replace(first, last - first, replacement);
            const auto caret = position(candidate, first + replacement.size());
            auto previous = std::move(text); text = std::move(candidate); ++revision;
            if (!editor.acceptEdit(edit.id, snapshot(), {caret, caret})) {
                text = std::move(previous); --revision;
                throw std::runtime_error("Host acknowledgement failed");
            }
            undo.emplace_back(std::move(previous), edit.selection);
            if (undo.size() > 32) undo.erase(undo.begin()); redo.clear();
        } catch (const std::exception &error) { editor.rejectEdit(edit.id, error.what()); }
    }
    void draw(const char *id, ImVec2 size) {
        if (worker.valid() && worker.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            try { const auto result = worker.get(); if (editor.supplyPage(result)) preparedRows = result.lines.size(); }
            catch (const std::exception &error) { editor.failRead(active, error.what()); }
        }
        auto result = editor.draw(id, size);
        if (result.cancelledRead == active && cancelled) cancelled->store(true);
        if (result.read) queued = result.read;
        if (result.edit.committed()) apply(result.edit.value);
        for (auto &input : result.cancelledInput) recoveredInput.push_back(std::move(input));
        if (!recoveredInput.empty()) {
            textDim(tr("Input retained for host recovery").c_str());
            ImGui::SameLine();
            if (button(trLabel("Copy##recover-input").c_str())) {
                std::string recovered;
                for (const auto &input : recoveredInput) recovered += input.text;
                ImGui::SetClipboardText(recovered.c_str()); recoveredInput.clear();
            }
        }
        for (const auto &action : result.actions) {
            if (action.document != snapshot().id || action.revision != revision) continue;
            if (!large && (action.command == PagedCodeCommand::copy || action.command == PagedCodeCommand::cut)) {
                try {
                    const auto first = byte(action.selection.first()), last = byte(action.selection.last());
                    if (first != last) {
                        ImGui::SetClipboardText(text.substr(first, last - first).c_str());
                        if (action.command == PagedCodeCommand::cut && editor.setSelection(action.selection)) editor.replaceSelection({});
                    }
                } catch (const std::exception &error) { notify(error.what(), Tone::error); }
            } else if (!large && (action.command == PagedCodeCommand::undo || action.command == PagedCodeCommand::redo)) {
                auto &from = action.command == PagedCodeCommand::undo ? undo : redo;
                auto &to = action.command == PagedCodeCommand::undo ? redo : undo;
                if (!from.empty()) {
                    to.emplace_back(text, editor.selection());
                    auto previous = std::move(from.back()); from.pop_back(); text = std::move(previous.first); ++revision;
                    editor.setDocument(snapshot()); editor.setSelection(previous.second);
                }
            } else notify(tr("This action is handled by the host application.").c_str(), Tone::info);
        }
        if (queued && !worker.valid()) {
            const auto read = *queued; queued.reset(); active = read.id;
            cancelled = std::make_shared<std::atomic<bool>>(false);
            worker = std::async(std::launch::async, [read, source = large ? std::string() : text, virtualSource = large, cancel = cancelled] {
                return page(read, source, virtualSource, cancel);
            });
        }
    }
};
void pagedCodePage() {
    sectionTitle(tr("Paged code editor").c_str());
    textDim(tr("Only visible fragments are cached. The application owns storage, syntax, search and Undo/Redo.").c_str());
    static PagedGalleryHost editable, large(true);
    if (ImGui::BeginTable("##paged-examples", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        sectionTitle(tr("Editable host document").c_str());
        textDim(tr("Type, select, copy/paste and Undo. Each accepted edit is one host transaction.").c_str());
        editable.draw("##paged-editable", dp(0, 380));
        ImGui::TableNextColumn();
        sectionTitle(tr("Generated million-line source").c_str());
        ImGui::SetNextItemWidth(dp(130));
        ImGui::InputScalar(trLabel("Line##paged-jump").c_str(), ImGuiDataType_U64, &large.jump);
        ImGui::SameLine();
        if (button(trLabel("Go##paged-go").c_str())) large.editor.scrollTo(large.jump ? large.jump - 1 : 0);
        large.draw("##paged-large", dp(0, 360));
        ImGui::EndTable();
    }
}
#endif

void codePage(bool previewPaged)
{
#if defined(CYUI_HAS_CODEEDITOR)
    static bool paged = false, preset = false;
    if (previewPaged && !preset) { paged = true; preset = true; }
    if (button(trLabel("In-memory editor##code-mode-memory").c_str())) paged = false;
    ImGui::SameLine();
    if (button(trLabel("Paged editor##code-mode-paged").c_str())) paged = true;
    if (paged) { pagedCodePage(); return; }
#else
    (void)previewPaged;
#endif

    sectionTitle(tr("Source view & annotations").c_str());
    textDim(tr("Ctrl + mouse wheel over code changes text size (50-300%).").c_str());
    textDim(tr("Orange strings, green comments: shared code colors across languages.").c_str());
    static CodeDocument document;
    static CodeViewState state;
    document.setText("// audio/Voice.cpp - read-only source with host annotations\n"
                     "#include <algorithm>\n\n"
                     "float applyGain(float sample, float gainDb) {\n"
                     "    const float gain = dbToLinear(gainDb);\n"
                     "    return std::clamp(sample * gain, -1.0f, 1.0f);\n"
                     "}\n\n"
                     "// Source data, diagnostics and file access belong to the host.\n", CodeLanguage::cpp);
    CodeAnnotation note{1, 5, Tone::info, "Host-supplied diagnostic: inspect gain conversion"};
    codeViewer("##source", state, document, dp(0, 205), {&note, 1});
#if defined(CYUI_HAS_CODEEDITOR)
    space();
    sectionTitle(tr("Native code editor").c_str());
    textDim(tr("Edit this document. Selection, Undo/Redo and language colors use the native editor backend.").c_str());
    static CodeEditor editor;
    static bool initialized = false;
    if (!initialized) {
        editor.setLanguage(CodeLanguage::cpp);
        editor.native().SetShowWhitespacesEnabled(false);
        editor.setText("// ui/Monitor.cpp - editable C++\n"
                       "#include <cyui/Core.h>\n"
                       "#include <cyui/modules/Audio.h>\n\n"
                       "void drawMonitor(float& gain) {\n"
                       "    cyui::sectionTitle(\"Output monitor\");\n"
                       "    cyui::KnobStyle style{42.0f, 0, \"Gain\"};\n"
                       "    if (cyui::knob(\"##gain\", &gain, 0.0f, 1.0f, 0.5f, style)) {\n"
                       "        // Apply the host's preview or Undo transaction here.\n"
                       "        updatePreview(gain);\n"
                       "    }\n"
                       "}\n");
        initialized = true;
    }
    const auto catalogue=codeLanguages();
    const auto current=codeLanguageInfo(editor.language());
    if (ImGui::BeginCombo(trLabel("Syntax##shared-language").c_str(),current.name.c_str())) {
        for (const auto &item : catalogue) if (ImGui::Selectable(item.name.c_str(),item.language==editor.language())) editor.setLanguage(item.language);
        ImGui::EndCombo();
    }
    editor.draw("##editable", dp(0, 275));
#endif
}
#endif

#if defined(CYUI_HAS_DIFF)
void diffPage()
{
    sectionTitle(tr("Compare document revisions").c_str());
    static DiffDocument document;
    static DiffState state;
    static bool initialized = false;
    if (!initialized) {
        document.setTexts(
            "#include <algorithm>\n\n"
            "struct Monitor {\n"
            "    float gain = 0.5f;\n"
            "    bool enabled = true;\n"
            "};\n\n"
            "float process(float sample, const Monitor& monitor) {\n"
            "    if (!monitor.enabled) {\n"
            "        return sample;\n"
            "    }\n"
            "    return sample * monitor.gain;\n"
            "}\n",
            "#include <algorithm>\n"
            "#include <cmath>\n\n"
            "struct Monitor {\n"
            "    float gainDb = -6.0f;\n"
            "    bool enabled = true;\n"
            "    bool limitOutput = true;\n"
            "};\n\n"
            "float process(float sample, const Monitor& monitor) {\n"
            "    if (!monitor.enabled) {\n"
            "        return sample;\n"
            "    }\n"
            "    const float gain = std::pow(10.0f, monitor.gainDb / 20.0f);\n"
            "    const float output = sample * gain;\n"
            "    return monitor.limitOutput\n"
            "        ? std::clamp(output, -1.0f, 1.0f) : output;\n"
            "}\n", CodeLanguage::cpp);
        initialized = true;
    }
    diffViewer("##diff", state, document, dp(0, 440));
}
#endif

#if defined(CYUI_HAS_DOCUMENTVIEWS)
std::string galleryMarkdown() {
    return locale() == "fr"
               ? "# Un document, plusieurs usages\n\nÉcrivez des **notes**, du *texte riche* et des guides "
                 "avec <u>liens</u>, ~~anciennes idées~~ et ||spoilers interactifs||.\n\n"
                 "![Paysage de démonstration|360](gallery/landscape.png)\n\n"
                 "## Préparer une version\n\n- [x] Images natives et zoom\n- [ ] Relire la documentation\n  "
                 "- Vérifier les exemples imbriqués\n\n"
                 "| Élément | Affichage |\n|:---|---:|\n| **Code** | Coloration native |\n| Médias | "
                 "Adaptateurs optionnels |\n\n"
                 "> Les couleurs et les dimensions suivent le thème actif.\n\n```cpp\ncyui::Document "
                 "notes;\nnotes.setMarkdown(markdown);\n```\n\n"
                 "<mark>À retenir</mark> : <kbd>Ctrl</kbd> + <kbd>B</kbd> applique le "
                 "gras.\n\nhttps://example.com/guide\n\n![Démonstration vidéo](gallery/example.mp4)\n"
               : "# One document, many uses\n\nWrite **notes**, *rich text* and guides with <u>links</u>, "
                 "~~old ideas~~ and ||interactive spoilers||.\n\n"
                 "![Demonstration landscape|360](gallery/landscape.png)\n\n"
                 "## Prepare a release\n\n- [x] Native images and zoom\n- [ ] Review the documentation\n  - "
                 "Check nested examples\n\n"
                 "| Element | Display |\n|:---|---:|\n| **Code** | Native syntax colors |\n| Media | "
                 "Optional adapters |\n\n"
                 "> Colors and dimensions follow the active theme.\n\n```cpp\ncyui::Document "
                 "notes;\nnotes.setMarkdown(markdown);\n```\n\n"
                 "<mark>Remember</mark>: <kbd>Ctrl</kbd> + <kbd>B</kbd> applies "
                 "bold.\n\nhttps://example.com/guide\n\n![Video demonstration](gallery/example.mp4)\n";
}
DocumentServices galleryDocumentServices(const GalleryResources &resources) {
    DocumentServices services;
    services.image = [&resources](const std::string &path) {
        return path == "gallery/landscape.png"
                   ? ImageData{resources.imagePreviews[0] ? resources.imagePreviews[0] : resources.texture,
                               256, 160}
                   : ImageData{};
    };
    services.preview = [](const std::string &) {
        return DocumentLinkPreview{tr("Example guide"),
                                   tr("A cached link preview, supplied by the application."), "example.com"};
    };
    return services;
}
void documentsPage(const GalleryResources &resources) {
    static Document doc;
    static DocumentState state;
    static std::string source, lastLocale;
    if (lastLocale != locale()) {
        source = galleryMarkdown();
        lastLocale = locale();
    }
    doc.setMarkdown(source);
    static bool previewZoom = false;
    if (resources.previewDocumentImage && !previewZoom) {
        state.enlargedImage = "gallery/landscape.png";
        state.enlargedCaption = locale() == "fr" ? "Paysage de démonstration" : "Demonstration landscape";
        state.imageView = {};
    }
    previewZoom = resources.previewDocumentImage;
    auto services = galleryDocumentServices(resources);
    DocumentOptions options;
    options.services = &services;
    options.imageHeight = 180;
    const auto result = documentView("##document", state, doc, dp(0, 440), options);
    if (result.taskOffset != std::string::npos)
        source[result.taskOffset] = result.taskChecked ? 'x' : ' ';
    static TranscriptState chat;
    static std::vector<TranscriptMessage> messages{
        {1, tr("User"), tr("Show the result."), "10:00"},
        {2, tr("Assistant"),
         "**Markdown** · <u>Native</u> · ||Spoiler||\n```json\n{\"status\": \"ready\"}\n```", "10:01"}};
    static MessageComposerState draft, edit;
    static ItemId next = 3, editing = 0;
    if (messages.size() > 1 && messages[1].attachments.empty() && resources.texture) {
        messages[1].own = false;
        messages[1].attachments.push_back({1, "landscape.png", "gallery/landscape.png", 1024, true});
    }
    TranscriptOptions conversation;
    conversation.document = options;
    conversation.editing = editing;
    conversation.editor = [&](const TranscriptMessage &message, float width) {
        MessageComposerOptions input;
        input.attachments = false;
        input.editing = true;
        auto action = messageComposer("##edit", edit, {width, 0}, input);
        if (action.submitted) {
            for (auto &row : messages) if (row.id == message.id) { row.content = edit.text; row.edited = true; }
            editing = 0;
        }
        if (action.cancelled) editing = 0;
    };
    const auto action = transcript("##chat", chat, messages, dp(0, 240), conversation);
    if (action.edit) for (const auto &message : messages) if (message.id == action.edit) {
        editing = message.id; edit.text = message.content; edit.focus = true;
    }
    if (action.erase) messages.erase(std::remove_if(messages.begin(), messages.end(), [&](const auto &message) { return message.id == action.erase; }), messages.end());
    MessageComposerOptions input;
    input.attachments = false; // Gallery resources are supplied by its host; no OS file operations here.
    const auto composed = messageComposer("##composer", draft, {}, input);
    if (composed.submitted) {
        messages.push_back({next++, tr("User"), draft.text, "10:02"});
        draft.text.clear(); draft.focus = true; chat.jumpToBottom = true;
    }
    static bool largeHistory = false;
    ImGui::Checkbox(trLabel("Virtual history (1,000,000 messages)").c_str(), &largeHistory);
    if (largeHistory) {
        static TranscriptState historyState;
        static TranscriptMessage row;
        TranscriptSource history;
        history.id = 1;
        history.count = 1'000'000;
        history.message = [](std::size_t index) -> const TranscriptMessage * {
            row = {ItemId(index + 1), tr("User"), "**Markdown** #" + std::to_string(index + 1), "10:00"};
            row.own = false;
            return &row;
        };
        history.indexOf = [](ItemId id) { return id ? std::size_t(id - 1) : std::string::npos; };
        transcript("##million", historyState, history, dp(0, 300));
    }
}
#endif
#if defined(CYUI_HAS_DOCUMENTEDITOR)
void documentEditorPage(const GalleryResources &resources) {
    static DocumentEditor editor;
    static std::string lastLocale;
    if (lastLocale != locale()) {
        editor.setMarkdown(galleryMarkdown());
        lastLocale = locale();
    }
    static int lastMode = -1;
    if (resources.previewDocumentMode >= 0 && resources.previewDocumentMode <= 3 &&
        resources.previewDocumentMode != lastMode)
        editor.setMode(DocumentMode(resources.previewDocumentMode));
    lastMode = resources.previewDocumentMode;
    auto services = galleryDocumentServices(resources);
    DocumentEditorOptions options;
    options.document.services = &services;
    options.document.imageHeight = 180;
    editor.draw("##documentEditor", dp(0, 570), options);
}
#endif
#if defined(CYUI_HAS_MEDIA)
void mediaPage(const GalleryResources &resources) {
    textDim("%s",
            tr("Native playback controls with a simulated host backend; no codec is loaded by the gallery.")
                .c_str());
    struct Example : MediaBackend {
        MediaSnapshot value;
        MediaSnapshot snapshot() const override {
            return value;
        }
        void play(bool v) override {
            value.playing = v;
        }
        void seek(double v) override {
            value.position = v;
        }
        void volume(float v) override {
            value.volume = v;
        }
        void mute(bool v) override {
            value.muted = v;
        }
    };
    static Example backend;
    static MediaPlayerState state;
    backend.value.ready = backend.value.seekable = true;
    backend.value.duration = 120;
    backend.value.title = tr("Media player");
    backend.value.frame = {resources.imagePreviews[0] ? resources.imagePreviews[0] : resources.texture, 256,
                           160};
    if (backend.value.playing)
        backend.value.position = std::fmod(backend.value.position + ImGui::GetIO().DeltaTime, 120.);
    mediaPlayer("##media", state, backend, dp(0, 470));
}
#endif
#if defined(CYUI_HAS_DATAGRID)
void dataGridPage() {
    static GridModel model(12, 5);
    static GridState state;
    static bool ready = false;
    if (!ready) {
        model.paste(
            {0, 0},
            "Resource\tType\tCount\tStatus\tNotes\nLandscape\tImage\t4\tReady\tNative textures\nEditor "
            "panel\tInterface\t12\tReview\tShared controls\nSignal graph\tGraph\t3\tReady\tReusable views");
        model.format({{0, 0}, {0, 4}}, 1);
        model.clearHistory();
        ready = true;
    }
    textDim("%s",
            tr("Select a range, double-click or press F2 to edit. Right-click for row and column actions.")
                .c_str());
    dataGrid("##standaloneGrid", state, model, dp(0, 520));
}
#endif
#if defined(CYUI_HAS_SPREADSHEET)
void spreadsheetPage() {
    static Workbook book;
    static SpreadsheetState state;
    static bool ready = false;
    if (!ready) {
        auto &m = book.sheets[0].grid;
        book.sheets[0].name = "Budget";
        m.resize(14, 6);
        m.paste({0, 0}, "Item\tUnit price\tQuantity\tTotal\tNotes\nCable\t12.5\t4\t=B2*C2\tReusable "
                        "equipment\nMicrophone\t99\t2\t=B3*C3\tStudio\nHeadphones\t49\t3\t=B4*"
                        "C4\tMonitoring\nTotal\t\t\t=SUM(D2:D4)\nAverage\t\t\t=AVERAGE(D2:D4)\nTax "
                        "(20%)\t\t\t=ROUND(D5*0.2,2)\nGrand total\t\t\t=D5+D7");
        m.format({{0, 0}, {0, 5}}, 1);
        m.format({{1, 1}, {7, 3}}, 0, GridAlignment::right, 2);
        m.clearHistory();
        SpreadsheetSheet summary;
        summary.name = "Summary";
        summary.grid.setText(0, 0, "=Budget!D8");
        book.sheets.push_back(std::move(summary));
        state.grid.hasSelection = true;
        state.grid.anchor = state.grid.current = {4, 3};
        ready = true;
    }
    textDim("%s",
            tr("Native worksheets, formulas and shared grid editing. Office formats are an optional module.")
                .c_str());
    spreadsheet("##workbook", state, book, dp(0, 560));
}
#endif
#if defined(CYUI_HAS_RICHDOCUMENT)
void richDocumentPage(const GalleryResources &resources) {
    static RichDocumentEditor editor;
    static std::string lastLocale;
    if (lastLocale != locale()) {
        auto document = richDocumentFromMarkdown(
            locale() == "fr"
                ? "# Documents natifs\n\nUn espace de travail paginé avec du **texte enrichi**, des images "
                  "et des tableaux éditables.\n\n## Une base commune\n\nDouble-cliquez sur un paragraphe ou "
                  "un tableau pour le modifier. Les marges et les en-têtes se règlent dans la configuration "
                  "de page.\n\n| Élément | Fonction |\n| --- | --- |\n| Tableaux | Cellules et plages |\n| "
                  "Formats | ODT, DOCX, ODS, XLSX |"
                : "# Native documents\n\nA paginated workspace with **rich text**, images and editable "
                  "tables.\n\n## One shared foundation\n\nDouble-click a paragraph or table to edit it. Set "
                  "margins and headers in page setup.\n\n| Element | Function |\n| --- | --- |\n| Tables | "
                  "Cells and ranges |\n| Formats | ODT, DOCX, ODS, XLSX |");
        document.page.header = "CyUI";
        document.page.footer = locale() == "fr" ? "Page {page} sur {pages}" : "Page {page} of {pages}";
        RichBlock image;
        image.kind = RichBlockKind::image;
        image.image = "gallery/landscape.png";
        image.caption = tr("Image");
        image.imageWidthMM = 100;
        image.imageHeightMM = 55;
        document.blocks.push_back(image);
        editor.setDocument(document);
        editor.setZoom(.6f);
        lastLocale = locale();
    }
    auto services = galleryDocumentServices(resources);
    RichDocumentOptions options;
    options.services = &services;
    editor.draw("##writer", dp(0, 570), options);
}
#endif

#if defined(CYUI_HAS_HIERARCHY)
void hierarchyPage() {
    textDim("%s",
            tr("Drag to reparent; Shift-drop inserts before a row. Ctrl/Shift selects; F2 renames.").c_str());
    static std::vector<HierarchyItem> items{{1, 0, "Scene", "Scene"},   {2, 1, "Environment", "Group"},
                                            {3, 2, "Terrain", "Mesh"},  {4, 2, "Sun", "Light"},
                                            {5, 1, "Camera", "Camera"}, {6, 1, "Effects", "Group"},
                                            {7, 6, "Fog", "Volume"}};
    static HierarchyState state;
    static ItemId next = 100;
    for (const auto &r : hierarchy("##hierarchy", state, items, dp(0, 460))) {
        if (r.action == HierarchyAction::remove) {
            auto removed = r.ids;
            bool more = true;
            while (more) {
                more = false;
                for (const auto &item : items)
                    if (selected(removed, item.parent) && !selected(removed, item.id)) {
                        removed.push_back(item.id);
                        more = true;
                    }
            }
            items.erase(std::remove_if(items.begin(), items.end(),
                                       [&](const auto &item) { return selected(removed, item.id); }),
                        items.end());
            continue;
        }
        if (r.action == HierarchyAction::duplicate) {
            std::vector<HierarchyItem> copies;
            for (const auto &item : items)
                if (selected(r.ids, item.id)) {
                    auto copy = item;
                    copy.id = next++;
                    copy.label += " (copy)";
                    copies.push_back(copy);
                }
            items.insert(items.end(), copies.begin(), copies.end());
            continue;
        }
        for (auto &item : items)
            if (selected(r.ids, item.id)) {
                if (r.action == HierarchyAction::rename)
                    item.label = r.text;
                if (r.action == HierarchyAction::visibility)
                    item.visible = r.value;
                if (r.action == HierarchyAction::locking)
                    item.locked = r.value;
                if (r.action == HierarchyAction::reparent)
                    item.parent = r.parent;
            }
        if (r.action == HierarchyAction::reparent && r.before) {
            std::vector<HierarchyItem> moved;
            for (const auto &item : items)
                if (selected(r.ids, item.id))
                    moved.push_back(item);
            items.erase(std::remove_if(items.begin(), items.end(),
                                       [&](const auto &item) { return selected(r.ids, item.id); }),
                        items.end());
            auto at = std::find_if(items.begin(), items.end(),
                                   [&](const auto &item) { return item.id == r.before; });
            items.insert(at, moved.begin(), moved.end());
        }
    }
}
#endif
#if defined(CYUI_HAS_DATAVIEW)
void dataViewPage() {
    textDim(
        "%s",
        tr("10,000 rows. Sort headers, filter columns, Shift-select cells, Ctrl+C/V; double-click to edit.")
            .c_str());
    static std::vector<DataRow> rows;
    static DataViewState state;
    static std::vector<DataColumn> columns{{1, "Name", true, false, 0, 1, 220},
                                           {2, "Value", true, true, 0, 10, 160},
                                           {3, "Enabled", true, false, 0, 1, 100},
                                           {4, "ID", false, false, 0, 1, 150}};
    if (rows.empty())
        for (int n = 1; n <= 10000; ++n)
            rows.push_back({ItemId(n),
                            {std::string("Object ") + std::to_string(n), double(n % 100) / 10., true,
                             std::int64_t(9007199254740990LL + n)}});
    for (const auto &e : dataView("##dataView", state, columns, rows, dp(0, 470)))
        if (e.edit.committed())
            for (auto &row : rows)
                if (row.id == e.row)
                    for (std::size_t c = 0; c < columns.size(); ++c)
                        if (columns[c].id == e.column)
                            row.cells[c] = e.edit.value;
}
#endif
#if defined(CYUI_HAS_FILEPICKER)
void filePickerPage() {
    textDim("%s", tr("A virtual project supplied by the host. Selecting or saving here performs no "
                     "filesystem operation.")
                      .c_str());
    static FilePickerState state;
    static bool initialized = false;
    static int mode = 0;
    const char *modes[]{"Open file", "Open files", "Save file", "Select folder"};
    if (segmented("##pickerMode", modes, 4, &mode)) {
        state.selection.clear();
        state.filename.clear();
    }
    if (!initialized) {
        state.directory = state.pathText = "/project";
        initialized = true;
    }
    FilePickerOptions options;
    options.root = "/project";
    options.mode = FilePickerMode(mode);
    options.filters = {{"All files", {}}, {"Images", {"png", "jpg"}}, {"Source", {"cpp", "h"}}};
    FileListing listing;
    listing.directory = state.directory;
    if (state.directory == "/project")
        listing.entries = {{1, "Images", true},
                           {2, "Source", true},
                           {3, "README.md", false, 2300},
                           {4, "scene.json", false, 48000}};
    else if (state.directory == "/project/Images")
        listing.entries = {{5, "Terrain.png", false, 250000}, {6, "Preview.jpg", false, 80000}};
    else if (state.directory == "/project/Source")
        listing.entries = {{7, "main.cpp", false, 3200}, {8, "App.h", false, 1200}};
    else
        listing.error = tr("Folder not found");
    const auto result = filePicker("##filePicker", state, listing, dp(0, 490), options);
    if (result.accepted && !result.paths.empty())
        notify(result.paths.front().c_str(), Tone::success);
}
#endif
#if defined(CYUI_HAS_TIMELINE)
void timelinePage() {
    textDim("%s", tr("Drag clips/edges and markers. Drag lane names to reorder. S: split; Ctrl+D: duplicate; "
                     "right-click: actions.")
                      .c_str());
    static TimelineState state;
    static std::vector<TimelineLane> lanes{{1, "Video", 70}, {2, "Audio", 80}, {3, "Events", 48}};
    static std::vector<TimelineClip> clips{{1, 1, 1, 3, "Opening", 0},
                                           {2, 1, 5, 3, "Sequence", 1},
                                           {3, 2, .5, 7, "Sound", 2},
                                           {4, 3, 2, 1, "Marker interval", 3}};
    static std::vector<TimelineMarker> markers{{1, 4, "Cue"}};
    static ItemId next = 100;
    TimelineOptions options;
    options.playhead = 3.5;
    options.snap = .25;
    options.allocateId = []() { return ++next; };
    auto edit = timeline("##timeline", state, lanes, clips, markers, dp(0, 380), options);
    if (edit.clips.committed())
        clips = edit.clips.value;
    if (edit.lanes.committed())
        lanes = edit.lanes.value;
    if (edit.markers.committed())
        markers = edit.markers.value;
    for (auto key : edit.remove)
        clips.erase(std::remove_if(clips.begin(), clips.end(), [&](const auto &c) { return c.id == key; }),
                    clips.end());
    if (!state.error.empty())
        textColoured(palette().error, "%s", tr(state.error.c_str()).c_str());
}
#endif
#if defined(CYUI_HAS_COMMANDS)
void commandsPage(const GalleryResources &resources) {
    static CommandRegistry registry;
    static bool init = false, open = false;
    static CommandState state;
    static std::string error;
    if (!init) {
        registry.add({1, "Open document", "Ctrl+O", "File"});
        registry.add({2, "Save document", "Ctrl+S", "File"});
        registry.add({3, "Frame selection", "Home", "View"});
        registry.add({4, "Unavailable command", "", "Example", false});
        init = true;
    }
    textDim(
        "%s",
        tr("Menus, toolbar and command palette share one registry. Click a shortcut to rebind it.").c_str());
    if (primaryButton(trLabel("Open command palette").c_str()) || resources.previewCommands)
        open = true;
    if (registry.toolbarButton(2, icon::save))
        notify(tr("Save requested").c_str());
    if (beginProperties("##bindings")) {
        for (const auto &command : registry.commands()) {
            ItemScope key(command.id);
            property(tr(command.label.c_str()).c_str());
            auto chord = parseShortcut(command.shortcut);
            if (shortcutEditor("##binding", chord) && !registry.rebind(command.id, chord))
                error = tr("Shortcut conflicts with another command");
        }
        endProperties();
    }
    if (ghostButton(trLabel("Copy shortcuts as JSON").c_str()))
        ImGui::SetClipboardText(registry.bindingsJson().c_str());
    ImGui::SameLine();
    if (ghostButton(trLabel("Paste shortcuts").c_str())) {
        auto text = ImGui::GetClipboardText();
        registry.loadBindingsJson(text ? text : "", error);
    }
    const auto commands = registry.commands();
    const auto action = commandPalette("Commands###examplePalette", open, state, commands);
    if (action || registry.poll())
        notify(tr("Command requested (handled by the host)").c_str());
    if (!error.empty())
        textColoured(palette().error, "%s", tr(error.c_str()).c_str());
}
#endif
#if defined(CYUI_HAS_INSPECTOR) && defined(CYUI_HAS_DATAVIEW) && defined(CYUI_HAS_HIERARCHY) &&              \
    defined(CYUI_HAS_COMMANDS) && defined(CYUI_HAS_FILEPICKER)
void editorWorkspacePage() {
    static examples::EditorWorkspace model;
    static ItemId selected = 1, shown = 0, pendingObject = 0, pendingProperty = 0;
    static std::uint64_t inspectorRevision = 0, shownRevision = UINT64_MAX;
    static InspectorState details;
    static HierarchyState tree;
    static DataViewState grid;
    static CommandRegistry commands;
    static bool initialized = false, openPicker = false;
    static FilePickerState picker;
    if (!initialized) {
        commands.add({1, "Undo", "Ctrl+Z", "Edit"});
        commands.add({2, "Redo", "Ctrl+Y", "Edit"});
        tree.selection = {1};
        picker.directory = picker.pathText = "/Game/Textures";
        initialized = true;
    }
    commands.find(1)->enabled = model.canUndo();
    commands.find(2)->enabled = model.canRedo();
    const auto action = commands.poll();
    {
        ScopedDisabled disabled(!model.canUndo());
        if (button(trLabel("Undo").c_str()) || action == 1)
            model.undo();
    }
    ImGui::SameLine();
    {
        ScopedDisabled disabled(!model.canRedo());
        if (button(trLabel("Redo").c_str()) || action == 2)
            model.redo();
    }
    ImGui::SameLine();
    textDim("%s", tr("One host model, shared selection and atomic Undo/Redo across Inspector, DataView and "
                     "asset selection.")
                      .c_str());
    if (ImGui::BeginTable("##workspaceLayout", 2,
                          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(trLabel("Hierarchy").c_str(), ImGuiTableColumnFlags_WidthFixed, dp(250));
        ImGui::TableSetupColumn(trLabel("Inspector").c_str(), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        std::vector<HierarchyItem> items;
        for (const auto &object : model.objects())
            items.push_back({object.id, 0, std::get<std::string>(object.properties[0].value), "Object"});
        HierarchyOptions readOnlyTree;
        readOnlyTree.readOnly = true;
        hierarchy("##workspaceObjects", tree, items, dp(0, 260), readOnlyTree);
        if (!tree.selection.empty() && selected != tree.selection.back()) {
            selected = tree.selection.back();
            grid.active = grid.anchor = selected;
        }
        ImGui::TableNextColumn();
        if (shown != selected || shownRevision != model.revision()) {
            ++inspectorRevision;
            shown = selected;
            shownRevision = model.revision();
        }
        if (const auto *object = model.find(selected)) {
            InspectorOptions options;
            options.browseAssetAt = [&](ItemId field, const std::vector<std::string> &) {
                pendingObject = selected;
                pendingProperty = field;
                openPicker = true;
            };
            std::vector<examples::WorkspaceChange> commits;
            for (const auto &event :
                 inspector("##workspaceDetails", details, object->properties, inspectorRevision, {}, options))
                if (event.edit.committed())
                    commits.push_back({selected, event.id, event.edit.value});
            if (!commits.empty())
                model.apply(commits);
        }
        ImGui::EndTable();
    }
    std::vector<DataColumn> columns{
        {1, "Name", true, false, 0, 1, 160},     {2, "Position", true, false, 0, 1, 220},
        {3, "Tint", true, false, 0, 1, 160},     {4, "Texture", true, false, 0, 1, 270},
        {5, "Strength", true, true, 0, 10, 120}, {6, "Settings", true, false, 0, 1, 150}};
    std::vector<DataRow> rows;
    for (const auto &object : model.objects()) {
        DataRow row;
        row.id = object.id;
        for (const auto &p : object.properties)
            row.cells.push_back(p.value);
        rows.push_back(std::move(row));
    }
    DataViewOptions table;
    table.revision = model.revision();
    table.editors.browseAssetAt = [&](ItemId field, const std::vector<std::string> &path) {
        pendingObject = path.empty() ? selected : ItemId(std::stoull(path.front()));
        pendingProperty = field;
        openPicker = true;
    };
    std::vector<examples::WorkspaceChange> commits;
    for (const auto &event : dataView("##workspaceData", grid, columns, rows, dp(0, 220), table))
        if (event.edit.committed())
            commits.push_back({event.row, event.column, event.edit.value});
    if (!commits.empty())
        model.apply(commits);
    if (grid.active && grid.active != selected) {
        selected = grid.active;
        tree.selection = {selected};
    }
    if (openPicker) {
        ImGui::OpenPopup("##workspaceAsset");
        openPicker = false;
    }
    ImGui::SetNextWindowSize(dp(640, 410), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("##workspaceAsset", nullptr, ImGuiWindowFlags_NoSavedSettings)) {
        textDim("%s", tr("Choose texture").c_str());
        FileListing listing{"/Game/Textures", {{1, "Terrain.png"}, {2, "Stone.png"}, {3, "Water.png"}}};
        FilePickerOptions options;
        options.root = "/Game/Textures";
        options.filters = {{"Images", {"png"}}};
        const auto result = filePicker("##workspacePicker", picker, listing, dp(0, 300), options);
        if (result.accepted && !result.paths.empty()) {
            model.apply({{pendingObject, pendingProperty, PropertyAsset{"Texture", result.paths.front()}}});
            ImGui::CloseCurrentPopup();
        }
        if (result.cancelled)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    if (!model.error.empty())
        textColoured(palette().error, "%s", tr(model.error.c_str()).c_str());
}
#endif

#if defined(CYUI_HAS_DATAVIEW)
void richDataViewPage() {
    textDim(
        "%s",
        tr("Double-click a cell to use the Inspector editor. Rich cells keep their type when copied as TSV.")
            .c_str());
    static std::vector<DataColumn> columns{
        {1, "Name", true, false, 0, 1, 150},      {2, "Position", true, false, 0, 1, 240},
        {3, "Tint", true, false, 0, 1, 180},      {4, "Texture", true, false, 0, 1, 250},
        {5, "Quality", true, false, 0, 1, 130},   {6, "Tags", true, false, 0, 1, 140},
        {7, "Overrides", true, false, 0, 1, 160}, {8, "Optional", true, false, 0, 1, 160},
        {9, "Angle", true, false, 0, 1, 100}};
    static std::vector<DataRow> rows;
    static DataViewState state;
    if (rows.empty()) {
        for (int n = 1; n <= 1000; ++n)
            rows.push_back(
                {ItemId(n),
                 {std::string("Object ") + std::to_string(n), PropertyVector{{2.56, double(n), 0}},
                  PropertyColor{{.35f, .65f, .8f, 1}}, PropertyAsset{"Texture", "/Game/Textures/Terrain"},
                  PropertyEnum{"Quality", 1, {{0, "Low"}, {1, "Medium"}, {2, "High"}}},
                  PropertySet{{std::string("Static"), std::string("Outdoor")},
                              std::make_shared<const PropertyValue>(std::string{})},
                  PropertyMap{{{std::string("Gain"), 1.}},
                              std::make_shared<const PropertyValue>(std::string{}),
                              std::make_shared<const PropertyValue>(0.)},
                  makePropertyOptional(2.56), PropertyCustom{"Angle", "45"}}});
        state.editing = 1;
        state.editingColumn = 1;
    }
    DataViewOptions options;
    options.editors.editCustom = [](ItemId, const std::vector<std::string> &, const PropertyCustom &value) {
        return button((value.json + " deg").c_str())
                   ? Edit<PropertyCustom>{EditPhase::commit,
                                          {value.type,
                                           std::to_string(std::fmod(std::stod(value.json) + 15, 360))}}
                   : Edit<PropertyCustom>{};
    };
    for (const auto &event : dataView("##richData", state, columns, rows, dp(0, 450), options))
        if (event.edit.committed())
            for (auto &row : rows)
                if (row.id == event.row)
                    for (std::size_t column = 0; column < columns.size(); ++column)
                        if (columns[column].id == event.column)
                            row.cells[column] = event.edit.value;
}
#endif
void moduleGallery(int selectedTab, const GalleryResources &resources) {
    (void)resources;
    auto page = [&](const char *label, int index, auto body) {
        if (ImGui::BeginTabItem(trLabel(label).c_str(), nullptr,
                                selectedTab == index ? ImGuiTabItemFlags_SetSelected : 0)) {
            body();
            ImGui::EndTabItem();
        }
    };
#if defined(CYUI_HAS_CANVAS)
    page("Canvas",4,[]{textDim(tr("Wheel: zoom at pointer. Middle drag: pan. Space + drag: pan.").c_str());static CanvasState state;CanvasScope canvas("##canvas",state,dp(0,320));const auto&f=canvas.frame();auto*d=ImGui::GetWindowDrawList();const auto a=state.view.screen({40,40},f.bounds.min),b=state.view.screen({220,160},f.bounds.min);d->AddRectFilled(a,b,palette().accentDim,dp(4));d->AddText(a,palette().text,"World-space content");});
#endif
#if defined(CYUI_HAS_PLOTS)
    page("Plots",5,plotsPage);
#endif
#if defined(CYUI_HAS_CURVES)
    page("Curves",6,[]{textDim(tr("Double-click: add/remove. Drag: edit. Ctrl: multi-select. Escape: cancel.").c_str());static std::vector<CurvePoint>points{{1,0,.2},{2,2,.8,CurveShape::smooth},{3,5,.3,CurveShape::bezier,{-1,.1},{1,.2}},{4,8,.7}};static CurveState state;static bool init=false;if(!init){state.view.x.range={0,10};state.view.y.range={0,1};init=true;}CurveOptions options;options.snap=.25;const auto edit=curveEditor("##curves",state,points,dp(0,320),options);if(edit.committed())points=edit.value;});
#endif
#if defined(CYUI_HAS_NODES)
    page("Nodes",7,[&]{nodesPage(resources);});
#endif
#if defined(CYUI_HAS_TIMELINE)
    page("Timeline", 8, timelinePage);
#endif
#if defined(CYUI_HAS_AUDIOVIEWS)
    page("Audio views",9,audioViewsPage);
    page("Audio analysis",22,audioAnalysisPage);
    page("Spectral displays",23,spectralViewsPage);
    page("Envelopes",38,envelopesPage);
#endif
#if defined(CYUI_HAS_CODE)
    page("Code",10,[&]{codePage(resources.previewPagedCode);});
#endif
#if defined(CYUI_HAS_DIFF)
    page("Code / Diff",11,diffPage);
#endif
#if defined(CYUI_HAS_INSPECTOR)
    page("Inspector",12,[&]{inspectorPage(resources);});
    page("Virtual arrays", 33, virtualArraysPage);
    page("Sets", 34, [] { associativeCollectionsPage(false); });
    page("Maps", 35, [] { associativeCollectionsPage(true); });
#endif
#if defined(CYUI_HAS_ASSETS)
    page("Assets",13,[&]{assetsPage(resources);});
#endif
#if defined(CYUI_HAS_IMAGES)
    page("Images",14,[&]{static GradientState gradient;static std::vector<GradientStop>stops; if(stops.empty())stops={{1,0,vec(palette().info)},{2,.5,vec(palette().accent)},{3,1,vec(palette().success)}};auto e=gradientEditor("##gradient",gradient,stops,dp(0,56));if(e.committed())stops=e.value;static ImageState state;ImageData image{resources.texture,resources.width?resources.width:256,resources.height?resources.height:256};imageViewport("##image",state,image,dp(0,330),ImageTool::crop);});
#endif
#if defined(CYUI_HAS_VIEWPORT)
    page("Viewport",15,[&]{textDim(tr("Alt-left drag: orbit. Middle drag: pan. Wheel: dolly. Renderer and scene remain host-owned.").c_str());static ViewportState state;ImageData image{resources.texture,resources.width?resources.width:256,resources.height?resources.height:256};viewport("##viewport",state,image,dp(0,220));textDim(tr("2D transform overlay: move center, scale corners, rotate top handle.").c_str());{CanvasState canvas;CanvasOptions o;o.grid=true;o.navigation=false;CanvasScope scope("##transform",canvas,dp(0,200),o);static TransformState transformState;static Transform2D value{{150,90},{150,70},.2};TransformOptions options;const auto origin=scope.frame().bounds.min;options.project=[=](Point p){return ImVec2(origin.x+float(p.x),origin.y+float(p.y));};options.unproject=[=](ImVec2 p){return Point{p.x-origin.x,p.y-origin.y};};auto edit=transformGizmo("##gizmo",transformState,value,options);if(edit.committed())value=edit.value;}
#if defined(CYUI_HAS_IMGUIZMO)
        {CanvasState three;CanvasScope area("##3d",three,dp(0,200));static Gizmo3DState gizmo;static std::array<float,16>matrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};OrbitCamera camera;const auto view=cameraViewMatrix(camera);const std::array<float,16>projection{1.5f,0,0,0,0,2,0,0,0,0,-1.002f,-1,0,0,-.2002f,0};const auto edit3d=gizmo3D("##scene",gizmo,matrix,view,projection,area.frame().bounds);if(edit3d.committed())matrix=edit3d.value;}
#endif
        });
#endif
#if defined(CYUI_HAS_COMMANDS)
    page("Commands", 16, [&] { commandsPage(resources); });
#endif
#if defined(CYUI_HAS_DIAGNOSTICS)
    page("Diagnostics",17,[]{static LogBuffer buffer;static LogState state;static bool init=false;if(!init){buffer.append(LogLevel::info,"Application initialized");buffer.append(LogLevel::warning,"Example warning");buffer.append(LogLevel::error,"Example diagnostic — no file is modified");init=true;}logView("##log",state,buffer,dp(0,210));std::vector<Diagnostic>rows{{1,LogLevel::warning,"example.cpp","Unused value",8,4},{2,LogLevel::error,"shader.hlsl","Example compiler diagnostic",12,3}};diagnosticList("##diagnostics",rows,dp(0,140));});
#endif
#if defined(CYUI_HAS_DOCUMENTVIEWS)
    page("Documents",18,[&]{documentsPage(resources);});
#endif
#if defined(CYUI_HAS_DOCUMENTEDITOR)
    page("Document editor",24,[&]{documentEditorPage(resources);});
#endif
#if defined(CYUI_HAS_MEDIA)
    page("Media player",25,[&]{mediaPage(resources);});
#endif
#if defined(CYUI_HAS_DATAGRID)
    page("Data grid",28,dataGridPage);
#endif
#if defined(CYUI_HAS_SPREADSHEET)
    page("Spreadsheet",26,spreadsheetPage);
#endif
#if defined(CYUI_HAS_RICHDOCUMENT)
    page("Paged document",27,[&]{richDocumentPage(resources);});
#endif
#if defined(CYUI_HAS_WEB)
    page("Web",19,[&]{textDim(tr("Optional CEF rendering in an ImGui texture. HTML/compiled TypeScript; host-owned messages.").c_str());
        struct Preview:WebBackend{void place(const WebPlacement&)override{}bool navigate(const std::string&)override{return false;}bool postJson(const std::string&)override{return false;}bool poll(WebEvent&)override{return false;}bool ready()const override{return false;}};static Preview preview;
        if(!resources.web)callout(Tone::info,tr("Gallery preview: attach a CyUI::WebCEF backend in GalleryResources.web to load an interactive page.").c_str());
        WebBackend&backend=resources.web?*resources.web:preview;static std::string lastWebTheme;const auto currentWebTheme=webThemeJson();if(currentWebTheme!=lastWebTheme&&backend.postJson(currentWebTheme))lastWebTheme=currentWebTheme;
        for(const auto&e:webPanel("##web",backend,dp(0,340))){if(e.kind==WebEventKind::navigated)backend.postJson(webThemeJson());if(e.kind==WebEventKind::message&&(e.data.find("page.ready")!=std::string::npos||e.data.find("chat.message")!=std::string::npos))backend.postJson("{\"protocol\":\"cyui.web\",\"version\":1,\"type\":\"host.reply\",\"payload\":{\"status\":\"Message received by CyUI\"}}");if(e.kind==WebEventKind::error)textColoured(palette().error,tr("%s").c_str(),e.data.c_str());}
    });
#endif

#if defined(CYUI_HAS_HIERARCHY)
    page("Hierarchy", 30, hierarchyPage);
#endif
#if defined(CYUI_HAS_DATAVIEW)
    page("Data view", 31, dataViewPage);
    page("Rich data view", 36, richDataViewPage);
#endif
#if defined(CYUI_HAS_INSPECTOR) && defined(CYUI_HAS_DATAVIEW) && defined(CYUI_HAS_HIERARCHY) && defined(CYUI_HAS_COMMANDS) && defined(CYUI_HAS_FILEPICKER)
    page("Editor workspace", 37, editorWorkspacePage);
#endif
#if defined(CYUI_HAS_FILEPICKER)
    page("File picker", 32, filePickerPage);
#endif
}

}  // namespace

void showGallery(bool* open, const int selectTab) { showGallery(open,selectTab,GalleryResources{}); }

void showGallery(bool* open, const int selectTab,const GalleryResources& resources)
{
    ImGui::SetNextWindowSize(dp(760.0f, 680.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(trLabel("CyUI Gallery").c_str(), open)) {
        ImGui::End();
        return;
    }
    themeBar();
    palettePreview();
    divider(2.0f);
    if (ImGui::BeginTabBar("##galleryTabs",ImGuiTabBarFlags_FittingPolicyScroll)) {
        // Brings selectTab to the front once (and again when it changes): the
        // tabs stay free to click.
        auto* storage = ImGui::GetStateStorage();
        const auto key = ImGui::GetID("##selectedTab");
        const auto selected = selectTab != storage->GetInt(key, -1) ? selectTab : -1;
        storage->SetInt(key, selectTab);
        if (ImGui::BeginTabItem(trLabel("Text & status").c_str(), nullptr, selected == 0 ? ImGuiTabItemFlags_SetSelected : 0)) {
            textPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Buttons").c_str(), nullptr, selected == 1 ? ImGuiTabItemFlags_SetSelected : 0)) {
            buttonsPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Controls").c_str(), nullptr, selected == 2 ? ImGuiTabItemFlags_SetSelected : 0)) {
            controlsPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Containers").c_str(), nullptr, selected == 3 ? ImGuiTabItemFlags_SetSelected : 0)) {
            containersPage();
            ImGui::EndTabItem();
        }
        moduleGallery(selected,resources);
        if (ImGui::BeginTabItem(trLabel("Layout").c_str(), nullptr, selected == 20 ? ImGuiTabItemFlags_SetSelected : 0)) {
            layoutPage();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(trLabel("Filtering").c_str(), nullptr, selected == 21 ? ImGuiTabItemFlags_SetSelected : 0)) {
            filteringPage(resources);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void showThemeEditor(bool* open)
{
    ImGui::SetNextWindowSize(dp(420.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(trLabel("CyUI Theme editor").c_str(), open)) {
        ImGui::End();
        return;
    }
    themeBar();
    static char path[260] { "my_theme.cytheme" };
    textField("##path", path, sizeof(path), "File", -dp(120.0f));
    ImGui::SameLine();
    if (button(trLabel("Save").c_str())) {
        if (savePalette(path, palette()))
            notify(tr("Theme saved").c_str(), Tone::success);
        else
            notify(tr("Could not save the theme").c_str(), Tone::error);
    }
    ImGui::SameLine();
    if (button(trLabel("Load").c_str())) {
        auto loaded = palette();
        if (loadPalette(path, loaded)) {
            setPalette(loaded);
            notify(tr("Theme loaded").c_str(), Tone::success);
        } else {
            notify(tr("Could not read the theme").c_str(), Tone::error);
        }
    }
    static const char* groups[] { "Grounds", "Lines", "Text", "Accent", "Meaning" };
    auto edited = palette();
    auto changed = false;
    for (auto group = 0; group < 5; ++group) {
        if (!beginSection(trLabel(groups[group]).c_str()))
            continue;
        if (beginProperties(groups[group])) {
            for (const auto& field : paletteFields()) {
                if (field.group != group)
                    continue;
                property(tr(field.label).c_str());
                auto colour = vec(edited.*field.member);
                ImGui::PushID(field.key);
                if (ImGui::ColorEdit4("##c", &colour.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayHex)) {
                    edited.*field.member = ImGui::ColorConvertFloat4ToU32(colour);
                    changed = true;
                }
                ImGui::PopID();
            }
            endProperties();
        }
        endSection();
    }
    if (changed)
        setPalette(edited);
    ImGui::End();
}

}  // namespace cyui
