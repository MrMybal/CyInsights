# CyUI — guide for AI agents

You are restyling, or building, a Dear ImGui interface in one of the Cy* apps
(CyDAW, Shaderixel, CyImgView, CyGPUInspector, ...). Use **CyUI**
(`ThirdParty/CyUI`). Read this whole file first. The code of
every example is in `src/Gallery.cpp`.

CyUI is the look of CyDAW 2, Shaderixel and CyEngineHub, turned into a
library: one semantic palette, one set of sizes, the fonts, and the controls
they share. It sits **on top of** Dear ImGui. It does not replace it, so
stock ImGui calls keep working and pick up the theme.

---

## 1. Integrate (once per app)

**Requirements:** Dear ImGui **1.92 or newer** (the docking branch is
recommended) and C++17.

### CMake

```cmake
set(CYUI_IMGUI_TARGET <the app's imgui target>)   # e.g. cydaw_imgui
add_subdirectory(ThirdParty/CyUI ${CMAKE_BINARY_DIR}/cyui)
target_link_libraries(<app> PRIVATE CyUI::CyUI)
```

### Without CMake

Add `include/` to the include path. Compile `src/Theme.cpp`, `src/Widgets.cpp`,
`src/Layout.cpp` and `src/Gallery.cpp` alongside the app's ImGui sources.

### Startup

Do this after `ImGui::CreateContext()`, before the first `NewFrame`, and
**instead of** the app's old theme function (`ImGui::StyleColorsDark`,
`ApplyTheme`, a hand-written `style.Colors[...]` block):

```cpp
#include <cyui/CyUI.h>

cyui::Config config;
config.themeKey = "mid-grey";            // see the theme table below
config.density  = cyui::Density::compact; // or comfortable
config.dpiScale = dpiScale;              // 1.0 at 96 dpi
cyui::apply(config);
cyui::loadFonts();                       // Segoe UI + Semibold + Consolas + MDL2 icons
```

When the DPI changes, call `cyui::applyDpi(newScale)`. At the end of every
frame, after everything else, call `cyui::renderNotifications()`. It draws the
toasts.

| themeKey      | origin      | look                                       |
|---------------|-------------|--------------------------------------------|
| `mid-grey`    | CyDAW 2     | Bitwig-like mid greys, orange accent (default) |
| `light-grey`  | CyDAW 2     | Ableton-like light greys, orange accent    |
| `amber-night` | CyDAW 2     | deep blue-greys, amber accent              |
| `violet`      | Shaderixel  | dark slate, violet accent, cyan info       |
| `navy`        | CyEngineHub | very dark navy, blue accent, flat          |

Use `compact` density for dense pro tools (DAW, editors). Use `comfortable`
for launchers, hubs and settings-heavy apps.

---

## 2. The rules (what makes it look right)

1. **Never hard-code a colour.** Take every colour from `cyui::palette()`,
   by its role:
   - `bg0` is the deepest ground: canvases, timelines, wells, the status bar.
   - `bg1` is the panel ground.
   - `bg2` is for headers, cards and popups.
   - `bg3` is for fields and buttons at rest; `bg4` when hovered; `bg5` when
     pressed or active.
   - `line` and `lineStrong` are for lines.
   - `text`, `textDim` and `textFaint` are the three text levels.
   - `accent`, `accentHover`, `accentDim`, `accentText` and `selection`: there
     is **one** accent, used only for what is active, selected or the main
     action.
   - `success`, `warning`, `error` and `info` carry meaning.

   For a variant, use `cyui::alpha / mix / brighter / darker`. For series of
   things (tracks, layers, tags), use `cyui::categorical(i)`.
2. **Never hard-code a size in pixels.** Wrap it: `cyui::dp(12.0f)`,
   `cyui::dp(8, 4)`. Prefer sizes derived from the style:
   `ImGui::GetFrameHeight()`, `ImGui::GetFontSize()`, `ImGui::GetStyle()...`.
3. **Text hierarchy.**
   - `cyui::heading(t, 1|2|3)` for titles.
   - `cyui::sectionTitle` for groups in a panel.
   - `cyui::overline("UPPERCASE")` for CyEngineHub-style labels.
   - `cyui::textDim` for labels and secondary info.
   - `cyui::textFaint` for hints.

   Do not make titles with `ImGui::Text` plus a colour push.
4. **One primary button per view.** Use `cyui::primaryButton` for the main
   action, `cyui::button` (secondary) for the rest, `cyui::ghostButton` or
   `cyui::iconButton` in toolbars, and `cyui::dangerButton` for destructive
   actions.
5. **Toolbars are icon buttons.** Use `cyui::iconButton(icon::x, "Tooltip
   (Shortcut)", active)`. Separate groups with `cyui::toolbarSeparator()`.
   Every icon button **must** have a tooltip that names the action and its
   shortcut.
6. **Settings and inspectors are property tables**
   (`beginProperties` / `property` / `endProperties`), inside collapsible
   sections (`beginSection`). Do not use free-floating
   `ImGui::Text("Label:"); ImGui::SameLine(); ImGui::SliderFloat(...)` lines.
7. **Group related content in cards** (`beginCard` / `endCard`). Use toned
   cards (`Tone::accent / warning / error`) only to draw attention.
8. **Empty panels say why.** Use `cyui::emptyState("No layers yet", "Drop an
   image or press Ctrl+N", icon::photo)`. Never leave a blank panel.
9. **Feedback is visible.**
   - `cyui::notify("Saved", Tone::success)` for events.
   - `cyui::callout(Tone::warning, ...)` for standing conditions.
   - `progressBar` / `spinner` for work in progress.
   - `badge` and `statusDot` for state.
10. **Tooltips are delayed and wrapped.** Use `cyui::tooltip(text)` after an
    item. Use `cyui::helpMarker(text)` to explain an option.
11. Keep stock ImGui wherever it already reads well: `Checkbox`, `InputText`,
    `ColorEdit`, tables, trees, menus, tab bars, `DragFloat2`. The theme
    styles them. Do not wrap or reinvent them.
12. Prefer the RAII helpers `ScopedColor`, `ScopedVar`, `ScopedFont`,
    `ScopedID` and `ScopedDisabled` over manual Push/Pop pairs.

---

## 3. Migrating an existing ImGui interface

Work panel by panel. Change the look, not the behaviour. Keep IDs, labels
and logic as they are.

| Stock / old code | CyUI |
|---|---|
| `ImGui::StyleColorsDark(); style.Colors[...] = ...` (theme function) | delete it; `cyui::apply(config); cyui::loadFonts();` |
| `IM_COL32(40,42,50,255)` or `ImVec4(...)` literals in drawing code | `cyui::palette().bg3` (pick by role) |
| `10.0f` pixel literals in custom drawing | `cyui::dp(10.0f)` |
| `ImGui::Button("OK")` main action of a dialog/panel | `cyui::primaryButton("OK")` |
| `ImGui::Button("Delete")` | `cyui::dangerButton("Delete")` |
| `ImGui::Button("X")` / `ImGui::SmallButton` in toolbars | `cyui::iconButton(cyui::icon::close, "Close")` |
| `ImGui::Button(icon)` + `if (selected) PushStyleColor(...)` tool buttons | `cyui::iconButton(icon, tip, selected)` |
| `ImGui::Checkbox` for an on/off **setting** | `cyui::toggleSwitch("Label", &v)` (keep `Checkbox` in lists/tables) |
| row of `RadioButton`s / buttons for a mode | `cyui::segmented("##mode", labels, n, &mode)` |
| `ImGui::Text("Section"); ImGui::Separator();` | `cyui::sectionTitle("Section")` or `cyui::beginSection("Section")` |
| `ImGui::TextColored(gray, ...)` | `cyui::textDim(...)` / `cyui::textFaint(...)` |
| `ImGui::Text("Name:"); SameLine(); InputText(...)` stacks | `beginProperties` + `property("Name")` + the widget |
| `ImGui::SliderFloat` in an inspector | `cyui::sliderBar(id, &v, min, max, "%.2f", -1)` (or keep SliderFloat) |
| `ImGui::InputText` used as a filter | `cyui::searchField("##filter", buf, size, "Search")` |
| `ImGui::BeginChild(..., true)` used as a box | `cyui::beginCard(id, "Title")` ... `cyui::endCard()` |
| `ImGui::ProgressBar` | `cyui::progressBar(f, size, overlay, tone)` |
| `ImGui::OpenPopup` + `BeginPopupModal` confirm dialog | `cyui::beginModal` + `cyui::modalButtons` |
| status messages printed in a panel | `cyui::notify(...)` |
| custom main-window bars with `SetNextWindowPos` hacks | `cyui::beginTopBar` / `beginStatusBar` / `beginSidebar` |
| audio knobs / faders / meters drawn by hand | `cyui::knob` / `cyui::fader` / `cyui::meter` |

Custom-drawn views (canvases, node editors, timelines, curves) keep their
drawing code. Swap their colour constants for palette roles:
- grounds: `bg0`
- grid: `grid` / `gridStrong`
- selection: `selection` plus an `accent` outline
- text: `text` / `textDim`
- handles: `bg5`, or `accent` when active

Check your work: open `cyui::showGallery()` from a debug menu, switch themes
and densities, and look at the app in **all five themes**. Nothing may stay
dark-on-dark or light-on-light.

---

## 4. API reference (short)

All functions are in `namespace cyui`. The headers carry the full comments.

### Theme (`Theme.h`)

- **Setup:** `apply(Config)`, `applyDpi(s)`, `loadFonts(FontConfig)`.
- **Palette:** `palette()`, `setPalette(p)`, `setTheme("key")`,
  `builtInThemes()`, `currentThemeKey()`.
- **Density and metrics:** `setDensity(d)`, `density()`, `metrics()`. The
  metrics give the font size, paddings, roundings, `cardPadding` and
  `toolbarHeight`.
- **Fonts:** `uiFont()`, `boldFont()`, `monoFont()`, `iconsAvailable()`.
- **Colour helpers:** `rgb(0xRRGGBB, a)`, `alpha`, `mix`, `brighter`,
  `darker`, `vec`, `categorical(i)`.
- **Sizes:** `dp(v)` / `dp(x, y)`.
- **Theme files:** `savePalette(path, p)`, `loadPalette(path, p)`,
  `paletteToText`, `paletteFromText`. The `.cytheme` format is
  `key = #RRGGBB[AA]`; CyDAW's JSON `.cytheme` files also load.

### Icons (`Icons.h`)

`cyui::icon::play, pause, stop, record, add, remove, trash, settings, search,
folder, save, undo, redo, edit, close, check, chevronDown, more, refresh,
zoomIn, lock, filter, loop, library, star, copy, paste, info, warning, home,
globe, people, mail, calendar, clock, cloud, upload, download, code, photo,
document, ...` (104 icons). They are UTF-8 strings of Segoe MDL2 Assets, so
they work in any label: `std::string(icon::save) + "  Save"`. They draw only
on Windows. Guard with `iconsAvailable()` when a text fallback matters (the
CyUI widgets already do).

### Text (`Widgets.h`)

- `heading(t, level)`, `sectionTitle(t)`, `overline(T)`
- `textDim(fmt, ...)`, `textFaint(fmt, ...)`, `textColoured(c, fmt, ...)`,
  `textMono(fmt, ...)`
- `kbd("Ctrl+S")`, `helpMarker(t)`, `tooltip(t)`
- `clippedText(list, pos, w, c, t)`, `centredText(list, min, max, c, t)`

### Buttons

- `button(label, Variant, size)`. The variants are `secondary`, `primary`,
  `ghost`, `outline`, `danger` and `success`.
- Shortcuts: `primaryButton`, `dangerButton`, `ghostButton`.
- `iconTextButton(icon, label, variant)`
- `iconButton(icon, tooltip, active, activeColour, size)`
- `toggleButton(label, &b, onColour, size, tooltip)`: compact pills such as
  M, S and R.
- `toggleSwitch(label, &b)`
- `segmented(id, labels, n, &current, width)`. A width of `-1` fills the row.
- `linkText(label)`

### Values

- `knob(id, &v, min, max, default, KnobStyle{diameter, colour, label, format,
  unit})`. Set `KnobStyle.flags` to `knobBipolar`, `knobLogarithmic`,
  `knobInteger` or `knobNoLabel`. `.steps` and `.modulation` are also
  available.
- `fader(id, &dB, minDb, maxDb, size, colour)`
- `meter(id, size, peakL, peakR, rmsL, rmsR, vertical)`
- `dragValue(id, &v, speed, min, max, fmt, width)`
- `sliderBar(id, &v, min, max, fmt, width, colour)`
- `valueEntry(...)`: the typed number field these controls open.
- `takeWheel()`, `wheelTaken()`, `valueSpeed()`

The value controls share the CyDAW behaviour:
- drag to change; Ctrl is fine, Shift is fast
- the mouse wheel nudges
- a double click resets (a knob) or types the value (a slider bar)
- a click on a knob's value types it

### Fields

- `searchField(id, buf, size, hint, width)`
- `textField(id, buf, size, hint, width)`
- `combo(id, &i, items, n, width)`

### Containers

- `beginCard(id, title, height, tone)` / `endCard()`. **Always** call
  `endCard`, whatever `beginCard` returned.
- `beginSection(label, defaultOpen)` / `endSection()`. Call `endSection`
  **only when** `beginSection` returned true.
- `beginProperties(id, labelWidth)` / `property(label, help)` /
  `endProperties()`. Call `endProperties` only when `beginProperties`
  returned true. `property()` sets up full width for the next widget.
- `toolbarSeparator()`, `divider(spacing)`, `space(h)`

### Status

- `badge(t, Tone)`, `statusDot(c, tip)`
- `progressBar(f, size, overlay, Tone)`. A fraction `f < 0` gives an
  indeterminate bar.
- `spinner(id, r, c)`
- `emptyState(title, msg, icon)`
- `callout(Tone, text)`
- `colourChip(id, c, size)`
- `notify(text, Tone, seconds)` and, once per frame,
  `renderNotifications()`.

### Layout (`Layout.h`)

The application shell. Call these in the frame in this order: menu bar, top
bar, status bar, sidebar, then dock space.

- `beginTopBar(id, h)` / `endTopBar()`
- `beginStatusBar(id)` / `endStatusBar()`, with `statusText(Tone, fmt, ...)`
- `beginSidebar(id, w)` / `endSidebar()`, with `sidebarHeader(title, badge)`,
  `sidebarGroup(LABEL)` and `sidebarItem(icon, label, active, badge)`
- `dockSpace(id, flags)` (docking branch)
- `panelHeader(title, subtitle)`
- `beginToolbar(id)` / `endToolbar()`
- `beginModal(title, &open, width)` / `endModal()`. Call `endModal` only
  when `beginModal` returned true. `modalButtons(confirm, cancel, danger)`
  returns 1 for confirm and 2 for cancel. Enter confirms and Escape cancels.
- `alignRight(width)`, `buttonWidth(label)`

All `begin*Bar` / `beginSidebar` / `beginToolbar` must be paired with their
`end*`, whatever they returned.

### Gallery (`Gallery.h`)

- `showGallery(&open)`: every widget, a theme switcher and a density switcher.
- `showThemeEditor(&open)`: live palette editing and `.cytheme`
  save and load.

---

## 5. Recipes

**Inspector panel**
```cpp
if (ImGui::Begin("Inspector")) {
    cyui::panelHeader("Layer", layer.name.c_str());
    cyui::divider();
    if (cyui::beginSection("Transform")) {
        if (cyui::beginProperties("##transform")) {
            cyui::property("Position");  ImGui::DragFloat2("##pos", &layer.x);
            cyui::property("Opacity");   cyui::sliderBar("##opacity", &layer.opacity, 0, 1, "%.2f", -1);
            cyui::property("Visible");   cyui::toggleSwitch("##visible", &layer.visible);
            cyui::property("Blend", "How the layer mixes with the ones under it.");
            cyui::combo("##blend", &layer.blend, blendNames, blendCount);
            cyui::endProperties();
        }
        cyui::endSection();
    }
}
ImGui::End();
```

**Tool palette**
```cpp
for (int i = 0; i < toolCount; ++i) {
    if (cyui::iconButton(tools[i].icon, tools[i].tooltip, current == i))
        current = i;
    ImGui::SameLine(0, cyui::dp(2));
}
cyui::toolbarSeparator();
```

**Confirm dialog**
```cpp
if (cyui::beginModal("Discard changes?", &askDiscard)) {
    ImGui::TextWrapped("The project has unsaved changes.");
    if (cyui::modalButtons("Discard", "Cancel", true) == 1)
        discard();
    cyui::endModal();
}
```

**Custom canvas colours**
```cpp
const auto& p = cyui::palette();
draw->AddRectFilled(min, max, p.bg0);
for (...) draw->AddLine(a, b, major ? p.gridStrong : p.grid);
draw->AddRectFilled(selMin, selMax, p.selection);
draw->AddRect(selMin, selMax, p.accent, 0, 0, cyui::dp(1.5f));
```

---

## 6. Do not

- Do not edit `third_party/imgui`. It is a copy used only by the demo; apps
  use their own ImGui.
- Do not add a second accent colour, gradients or drop shadows. The look is
  flat, with one accent.
- Do not call `ImGui::StyleColors*` or write `style.Colors[...]` after
  `cyui::apply`; that undoes the theme. To change colours, use `setPalette`.
- Do not use `ImGui::SetWindowFontScale`. Use
  `cyui::ScopedFont(font, factor)`, which calls
  `PushFont(font, FontSizeBase * factor)`.
- Do not put a CyUI widget's ID in its visible label when the label changes.
  Use `"Label###stableId"`.

## 7. Build and check

- `build_demo.bat` builds `build/cyui_demo.exe` with MSVC 2022 (no CMake
  needed).
- `cyui_demo.exe --capture out.bmp --theme navy --tab 2 [--comfortable]`
  renders 40 frames, writes the window to a BMP and quits. Use it to check a
  theme visually without a person at the screen.
- Screenshots are in `docs/screenshots/`.
