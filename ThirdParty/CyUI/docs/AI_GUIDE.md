# CyUI — guide for AI agents

Human documentation is maintained in `.CyDocBuild/manual/en/` and `manual/fr/`
and exported to `Documentation/Documentation.html`. This file remains the agent
integration contract; it is not a page in the human reader. See
`../.CyDocBuild/README.md` for regeneration. Ship the complete generated reader
for local help; never point user-facing help at this file or Markdown sources.

You are restyling, or building, a Dear ImGui desktop interface. Use **CyUI**
(for example, in `external/CyUI`). Read this whole file first. The code of
every example is in `src/Gallery.cpp`.

CyUI provides a common visual language and optional modules: one semantic
palette, consistent sizes, fonts, controls and reusable specialized views.
It sits **on top of** Dear ImGui. It does not replace it, so
stock ImGui calls keep working and pick up the theme.

---

## 1. Integrate (once per app)

**Localization is required for interface text:** keep English as the default and
French as the second built-in language. Read [LOCALIZATION.md](LOCALIZATION.md).
Use `cyui::trLabel("English label")` for controls/tabs/windows and
`cyui::tr("English text")` for display text. Register the application's messages
and provide a language choice; do not translate user content, paths or IDs.
Core includes localization automatically, without an extra module dependency.

**Requirements:** Dear ImGui **1.92 or newer** (the docking branch is
recommended) and C++17 (C++20 when CodeEditor or the optional CEF adapter is selected).

### CMake

```cmake
set(CYUI_IMGUI_TARGET <the app's imgui target>)   # e.g. cydaw_imgui
add_subdirectory(external/CyUI ${CMAKE_BINARY_DIR}/cyui)
target_link_libraries(<app> PRIVATE CyUI::CyUI)
```

`CyUI::CyUI` is the compatibility umbrella. For selective integration use
`<cyui/Core.h>` and `CyUI::Core`. Audio controls live in
`<cyui/modules/Audio.h>` / `CyUI::Audio`; the gallery and theme editor live in
`<cyui/Gallery.h>` / `CyUI::Gallery`. Audio and Gallery default to enabled;
disable `CYUI_BUILD_AUDIO` / `CYUI_BUILD_GALLERY` when not needed, and disable
`CYUI_BUILD_DEMO` for a selective build. Core never links those modules.
See [MODULES.md](MODULES.md) for complete examples, the application survey and
the implemented optional targets and their exact dependency boundaries.

For example, a source-reading application selects only Code and Diff:

```cmake
set(CYUI_BUILD_DEMO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(CYUI_BUILD_GALLERY OFF CACHE BOOL "" FORCE)
set(CYUI_MODULES "Code;Diff" CACHE STRING "" FORCE)
# add_subdirectory(...) after these options
target_link_libraries(app PRIVATE CyUI::Diff)
```

### Without CMake

Add `include/` to the include path. Compile `src/Theme.cpp`, `src/Widgets.cpp`,
`src/Layout.cpp`, `src/Filter.cpp`, `src/Numeric.cpp` alongside the app's ImGui sources for Core. Add
`src/modules/Audio.cpp` for audio controls, and `src/Gallery.cpp` for the gallery.
Define `CYUI_HAS_AUDIO=1` when compiling Gallery with audio examples.

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
cyui::loadFonts();                       // Segoe UI + Semibold + JetBrains Mono + MDL2 icons
```

When the DPI changes (`WM_DPICHANGED`), call `cyui::applyDpi(newScale)`. It
does nothing when the scale is unchanged. `apply` and a real DPI change rebuild
the whole `ImGuiStyle`, so make your own style tweaks after them. At the end of
every frame, after everything else, call `cyui::renderNotifications()`. It
draws the toasts.

`Config` also has `squareWindows`: square, opaque windows for apps whose ImGui
windows are OS windows (multi-viewports, plug-in editors). `FontConfig` has
`size` (0: the density's font size), `fallbackFolder` (a folder of Inter,
Roboto `.ttf` files, tried when Windows interface fonts are missing) and
`icons` (merge Segoe MDL2 Assets into interface fonts; on by default).
`monoFont()` is bundled JetBrains Mono Regular 2.304 (SIL OFL 1.1); keep
`third_party/jetbrains-mono/OFL.txt` with your distribution. No system font
installation is needed for code. Native Code/CodeEditor/Diff support per-view
Ctrl + wheel zoom; see the code guide for textScale and wheelZoom controls.

**Several ImGui contexts** (plug-in windows, editor panels): everything CyUI
keeps is per context and released when the context is destroyed. That covers
the palette, theme key, density, DPI, fonts, typed entry and toasts. Call
`apply` and `loadFonts` with each context current. Theme calls change the
current context only. With **no** context current, `setPalette`, `setTheme`
and `setDensity` change every live context, and the defaults new contexts
start from. Make such calls while no other thread draws with those contexts.
`apply` and `applyDpi` with no context current change the defaults only.

| themeKey      | look                                       |
|---------------|--------------------------------------------|
| `mid-grey`    | Mid greys, orange accent (default)          |
| `light-grey`  | Light greys, dark text, orange accent       |
| `amber-night` | Deep blue-greys, amber accent               |
| `violet`      | Dark slate, violet accent, cyan info        |
| `navy`        | Deep navy, blue accent, flat                |

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
   - `codeText`, `codeKeyword`, `codeType`, `codeNumber`, `codeString`,
     `codeComment`, `codePreprocessor` and `codeFunction` are shared syntax
     foregrounds inspired by the supplied Rider references: strings orange,
     comments green, keywords/directives blue, types mauve and functions turquoise.
     They are independent of the interface accent and apply to every language.

   For a variant, use `cyui::alpha / mix / brighter / darker`. For series of
   things (tracks, layers, tags), use `cyui::categorical(i)`.
2. **Never hard-code a size in pixels.** Wrap it: `cyui::dp(12.0f)`,
   `cyui::dp(8, 4)`. Prefer sizes derived from the style:
   `ImGui::GetFrameHeight()`, `ImGui::GetFontSize()`, `ImGui::GetStyle()...`.
3. **Text hierarchy.**
   - `cyui::heading(t, 1|2|3)` for titles.
   - `cyui::sectionTitle` for groups in a panel.
   - `cyui::overline("UPPERCASE")` for compact uppercase labels.
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

**Units.** Widget `size` and `width` parameters are screen pixels: wrap design
values in `dp()`, as in `cyui::button("OK", cyui::dp(80, 0))`. 0 means
automatic, and a negative width fills the row minus that much. These are
design units, scaled inside:
- `beginCard` height
- `beginProperties` label width
- `beginSidebar` and `beginModal` widths
- `space` height, `divider` spacing, `spinner` radius
- `KnobStyle::diameter`

`beginTopBar`'s height is in pixels.

**Disabled and edited.** Inside `ScopedDisabled`, CyUI's hand-drawn widgets
fade and stop reacting, like stock ImGui. Every CyUI control that changes a
value marks it edited, so `ImGui::IsItemEdited()` is true after it on that
frame. Clicks and drags also give `ImGui::IsItemDeactivatedAfterEdit()` on
release: one undo step per gesture. Wheel nudges and typed values are one-shot
edits that only set `IsItemEdited()`. `markEdited(id)` does the same for your
own controls.

### Theme (`Theme.h`)

- **Setup:** `apply(Config)`, `applyDpi(s)`, `dpi()` (the current scale),
  `loadFonts(FontConfig)`.
- **Palette:** `palette()`, `setPalette(p)`, `setTheme("key")`,
  `builtInThemes()`, `currentThemeKey()`. `paletteFields()` lists each role
  with its key, label, group and member pointer, for editors and theme files.
- **Density and metrics:** `setDensity(d)`, `density()`, `metrics()`. The
  metrics give the font size, paddings, roundings, `cardPadding` and
  `toolbarHeight`.
- **Fonts:** `uiFont()`, `boldFont()`, `monoFont()`, `iconsAvailable()`. The
  fonts are null until `loadFonts` filled the context's atlas.
  `ScopedFont(nullptr, factor)` then keeps the current font.
- **Colour helpers:** `rgb(0xRRGGBB, a)`, `alpha`, `mix`, `brighter`,
  `darker`, `vec`, `categorical(i)`. `categorical` gives 12 hues derived from
  the palette, toned for light or dark grounds.
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
on Windows. Without the icon font, the CyUI widgets fall back:
- `iconButton` shows the first letter of its tooltip when the current font
  lacks the glyph (a host's own merged icon font still works).
- `iconTextButton` shows the label alone.
- `sidebarItem` and `emptyState` leave the icon out.
- `helpMarker` shows `(?)`.

Guard your own icon labels with `iconsAvailable()`.

### Text (`CoreWidgets.h`)

- `heading(t, level)`, `sectionTitle(t)`, `overline(T)`
- `textDim(fmt, ...)`, `textFaint(fmt, ...)`, `textColoured(c, fmt, ...)`,
  `textMono(fmt, ...)`
- `kbd("Ctrl+S")`, `helpMarker(t)`, `tooltip(t)`
- `clippedText(list, pos, w, c, t)`, `centredText(list, min, max, c, t)`

### Buttons

- `toneColour(Tone)`: the palette colour of a tone (neutral is `textDim`).
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
- `dragValue(id, &v, speed, min, max, fmt, width)`. `speed` is per pixel
  dragged. Once the field has the focus (clicked or navigated to), a wheel
  notch moves 10 × `speed`; before that, the wheel scrolls the panel.
  `min >= max` is unbounded.
- `sliderBar(id, &v, min, max, fmt, width, colour)`. A click or a drag sets the
  value. A wheel notch moves 1 % of the range.
- `valueEntry(...)`: the typed number field these controls open.
  `valueEntryOpen()` says whether one is open (to hold back shortcuts while
  the user types).
- `itemWheel()`: the wheel for your own control, claimed in time so the panel
  under it does not scroll. Also `takeWheel()`, `wheelTaken()` and
  `valueSpeed()`.

The value controls share these interactions:
- drag to change; Ctrl is fine (× 0.15), Shift is fast (× 4)
- the mouse wheel nudges the value and does not scroll the panel under it
  (a `dragValue` only once it has the focus)
- a double click types the value (`dragValue`, `sliderBar`) or resets it (a
  knob)
- a click on a knob's value types it

### Fields

- `searchField(id, buf, size, hint, width)`
- `textField(id, buf, size, hint, width)`
- `combo(id, &i, items, n, width)`

### Quick filtering (`Filter.h`, included in Core)

Provide a quick filter in menus and panels where users need to find actions or
properties. Keep a `TextFilter` in each view's state. `filterBox(id, state)` draws
the query field; `panelHeader(title, subtitle, &state)` includes it in the heading.
Use `state.matches(englishLabel, keywordsAndSectionPath)` before drawing a row.
Search uses all words, ignoring ASCII/Latin case and accents, and checks both the
English key and the current translation. Hidden ImGui IDs are excluded.

`beginFilteredSection(label, state, hasMatchingChildren, defaultOpen)` reveals a
matching section without changing its saved collapse preference; pair with
`endSection()` when true. `filterMenu(id, state, entries)` draws a searchable menu
inside an already open ImGui menu/popup. Its `FilterMenuItem` tree retains action
IDs, disabled/checked states and shortcuts. Searching lists matching descendants
with breadcrumbs. The host handles the returned action ID.

`InspectorOptions{&state, sectionTitle}` enables filtering in `inspector()`;
`propertyMatches(state, property, sectionTitle)` helps choose visible sections.
Call Inspector for a section's full property model so hidden active edits can be
cancelled; when omitting an entire panel, cancel its host previews explicitly.
Read [FILTERING.md](FILTERING.md) for complete C++ and Python examples and limits.
These helpers do not intercept raw ImGui calls or patch Dear ImGui.

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
- `notify(text, Tone, seconds)` shows a toast in the current ImGui context.
  With no context current, it goes to the one context CyUI knows. When there
  are several or none, it asserts in debug builds and drops the toast.
  `notify(context, text, Tone, seconds)` posts to a given context, from any
  thread. Call `renderNotifications()` once per frame. At most six toasts
  show at a time.

### Layout (`Layout.h`)

The application shell. Call these in the frame in this order: menu bar, top
bar, status bar, sidebar, then dock space.

- `beginTopBar(id, h)` / `endTopBar()`
- `beginStatusBar(id)` / `endStatusBar()`, with `statusText(Tone, fmt, ...)`
- `beginSidebar(id, w)` / `endSidebar()`, with `sidebarHeader(title, badge)`,
  `sidebarGroup(LABEL)` and `sidebarItem(icon, label, active, badge)`
- `dockSpace(id, flags)` (docking branch)
- `panelHeader(title, subtitle)`
- `panelHeader(title, subtitle, &filter)` to add a quick filter
- `beginToolbar(id)` / `endToolbar()`
- `beginModal(title, &open, width)` / `endModal()`. Call `endModal` only
  when `beginModal` returned true. `modalButtons(confirm, cancel, danger)`
  returns 1 for confirm and 2 for cancel. Enter confirms and Escape cancels.
  Modals can nest: each `modalButtons` closes its own modal.
- `alignRight(width)`, `buttonWidth(label)`

All `begin*Bar` / `beginSidebar` / `beginToolbar` must be paired with their
`end*`, whatever they returned. Popups and tooltips opened from a bar keep the
theme's style.

### Gallery (`Gallery.h`)

- `showGallery(&open, selectTab)`: every widget, a theme switcher and a
  density switcher. `selectTab` brings a page to the front once:
  - 0 text, 1 buttons, 2 controls, 3 containers
  - 4–19 the optional modules
  - 20 layout (the application shell), 21 filtering (menus and panels)
  - 38 envelopes (ADSR/AHDSR and reusable curve fills)
- `showGallery(&open, selectTab, GalleryResources)`: the same, with host
  resources for the module pages: `texture`, `width` and `height` (Images,
  Viewport), `web` (a live Web backend) and `previewCommands`.
- `showThemeEditor(&open)`: live palette editing and `.cytheme`
  save and load.

`GalleryResources::openDocumentation(page, section)` is an optional host callback
for contextual help. The Filtering example's Help action requests
`filtering/searchable-menus`. Keep the callback owner alive during drawing.
Gallery and Core never open a browser themselves. The standalone demo resolves
the complete local `Documentation/Documentation.html` tree beside its executable.

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

- `build_markdown_demo.bat` builds `build/cyui_markdown_test.exe`, a standalone
  Markdown playground with a local chat and Live/Split/Source/Reading document
  modes on Windows/Linux/macOS. CMake: `CYUI_BUILD_MARKDOWN_DEMO=ON`. It links
  DocumentEditor, DocumentIO and their dependencies, plus an example-only SDL3 host.
  `CYUI_BUILD_PORTABLE_DEMO=ON` builds the same host with the full widget gallery
  (`cyui_demo_sdl3`). Installed SDL3 >= 3.4 is preferred; otherwise a SHA-256-verified
  pinned release is fetched. `CYUI_FETCH_DEMO_DEPENDENCIES=OFF` requires installed SDL3.
  System file dialogs are asynchronous; callbacks reach app state on the main thread.
  The CI matrix builds/runs the native apps on all three OSes (Xvfb on Linux).
  Local Windows verification does not establish Linux/macOS runtime validation.
  See [its guide](../examples/markdown/README.md); use
  `python tests/capture_markdown_demo.py` for native smoke/visual checks.
- `build_demo.bat` builds `build/cyui_demo.exe` with MSVC 2022 (no CMake
  needed).
- `cyui_demo.exe --capture out.bmp --theme navy --tab 2 [--comfortable]`
  renders 40 frames, writes the window to a BMP and quits. Use it to check a
  theme visually without a person at the screen. `--tab` takes a gallery
  page, 0 to 21.
- Add `--gallery-only` to show the gallery across the window instead of the
  full demo's docking layout. This also works interactively and does not change
  the library's rendering backend.
- `--gallery-only --tab 7 --image-graph` selects the image-processing node
  example, whose previews are generated by the demo host. The Assets and Inspector
  pages (13 and 12) show a content browser and grouped editor details panel.
- `python tools/capture_readme.py` regenerates the README's native screenshots
  from the built demo (requires Pillow). It captures synthetic fixtures for code,
  diff, audio monitors, a multi-node processing graph, frequency response,
  automation curves and a timeline, plus theme previews.
- Screenshots are in `docs/screenshots/`.


## 8. Optional module API and recipes

All headers below use the `cyui/` prefix. Link the corresponding target described
in [MODULES.md](MODULES.md). The examples for every widget are in Gallery.cpp,
tabs 4–19; the optional 3D adapter example appears in Viewport when enabled.
Use the same palette/dp/style rules as Core for host callbacks and overlays.

### IDs, state and edits

`Interaction.h` defines `ItemId` (uint64), `Point` (double x/y), `Range` (min/max),
`Rect` (screen bounds), borrowed `ArrayView<T>` and `Edit<T>`. Zero is reserved for
no object. `ItemScope` includes all 64 ID bits in the ImGui stack. Own one state
per panel, not one static gesture shared by every document. `snapValue` rounds
symmetrically; `zoomRange` keeps the pointer anchor fixed.

Interactive model editors return begin/update/commit/cancel proposals. Apply a
commit to the application model and record one Undo transaction. A revision
change cancels a stale active edit; advance it when the external model is replaced.
Atomic actions can commit immediately. Do not overwrite the external model with
all preview updates unless the application deliberately owns that preview layer.

```cpp
cyui::CurveState state;                   // persist with the panel
std::vector<cyui::CurvePoint> points;      // sorted, finite, stable nonzero IDs
cyui::CurveOptions options;
options.revision = document.revision;
options.snap = 0.25;
auto edit = cyui::curveEditor("##automation", state, points, cyui::dp(0, 260), options);
if (edit.committed()) document.commitCurve(edit.value);
```

The diagram/model types contain no engine pointers, JUCE strings or mandatory
JSON types. Keep arrays and callback captures alive for the duration of a draw.

### Canvas and numeric plots

| API | Use |
| --- | --- |
| `CanvasView::world/screen/zoomAt/fit` | Double world coordinates and anchored transforms |
| `CanvasScope(id, state, size, options)` / `frame()` | Reserve a canvas, draw its background/grid and scope clipping; `frame().wheel` when navigation or `claimWheel` takes the wheel |
| `drawCanvasGrid(rect, view, step)` | Grid overlay in the current window draw list |
| `Axis::fraction/value` | Inverse linear/log mapping; logarithmic values require positive bounds |
| `plot(id, PlotState&, ArrayView<PlotSeries>, size, options)` | Line/step/points/bars/area, ticks, legend, pan/zoom, crosshair and nearest sample ID/index |
| `reducePlot(points, budget)` | Prepare min/max buckets, retaining spikes and conservative gaps |
| `histogram(values, range, bins)` | uint64 counts, inclusive endpoints; ignores NaN/outside values |
| `heatmap(id, values, columns, rows, valueRange, size)` | Bounded cells, missing-data color and hovered row/column |

Canvas: middle drag or Space+left pans, wheel zooms at the pointer. Plot: middle
drag pans, wheel zooms X, Ctrl-wheel also zooms Y. `PlotOptions` controls grid,
axes, legend, crosshair/navigation, maxPoints and optional formatX/formatY callbacks
for host units. Ticks and legend are drawn inside the plot bounds. Reported
`skippedSeries` must prompt host reduction/querying; oversized data is not silently
reduced every frame. Prepare/cache a bounded visible dataset outside draw.

```cpp
cyui::PlotState state{{{20, 20000}, true}, {{-90, 0}, false}};
cyui::PlotSeries line{1, "Spectrum", preparedPoints, cyui::PlotStyle::line, 1};
cyui::PlotOptions options;
options.formatX = [](double hz) { return formatFrequency(hz); };
auto hover = cyui::plot("##response", state, {&line, 1}, cyui::dp(0, 220), options);
```

### Envelopes and reusable curve fills

C++ `drawCurveFill(points, baseline, CurveFillOptions)` belongs to **Plots**. It
fills caller-prepared **screen coordinates** in the current window and clip,
without reserving an item or drawing an outline. Use `CanvasScope` or your own
clipped canvas, then draw the outline once. `color=0` selects `palette().info`;
otherwise supply a palette-derived color. `opacity` is clamped to 0..1 (non-finite
means no fill); `gradient` fades to transparent at the baseline. Finite X must
increase within a run. Repeated X and zero tails add no area; NaN/Inf break runs,
backwards edges are skipped and baseline crossings are split. No internal AA
fringes or overlapping strips are generated. PlotStyle::area shares this renderer.

C++ `envelope(id, EnvelopeData, size, EnvelopeOptions)` belongs to **AudioViews**,
with no new dependency. It displays ADSR, AHDSR and custom envelopes from the
host's sampled time/level points. The overload taking an `EnvelopeEditState`
also proposes handle edits. See gallery tab **38, Envelopes**.

```cpp
std::vector<cyui::Point> samples{{0, 0}, {.02, 1}, {.3, .4}, {.7, .4}, {1, 0}};
const cyui::EnvelopeStage phases[]{
    {{0, .02}, "A"}, {{.02, .3}, "D"}, {{.3, .7}, "S"}, {{.7, 1}, "R"}};
cyui::EnvelopeData data{samples, {phases, 4}};
data.playhead = .5; data.level = .4; // optional live host measurements
cyui::EnvelopeOptions options;
options.time = {0, 1}; options.levels = {0, 1}; options.timePower = .6;
const auto hit = cyui::envelope("##env", data, cyui::dp(0, 180), options);
// hit.mouse is time/level, even with a nonlinear time display.
```

For an instrument panel, set `curveColor`, `levelLineColor` and `levelDotColor` from
the instrument palette. The frame accepts `backgroundColor`, `borderColor`,
`borderWidth` (zero hides it) and `frameRounding`. For a Prisme-style display
use `margins = {4, 6, 4, 4}` (left, top, right, bottom in dp), `timePower = .6`,
`frameRounding = 4`, `borderWidth = 0`, `levelDotRadius = 3.5f` and `levelDotInset = 1`.
Disable phase labels and lines for the compact instrument view.

For custom phase widths, supply `data.timeKnots` as pairs of host time and normalized
display X. Include both endpoints from `{time.min, 0}` to `{time.max, 1}`, with
both coordinates strictly increasing. Omit repeated boundaries of zero-length
phases. Knots take precedence over the power curve for samples, markers and picking.

```cpp
// Keep this state with the instrument UI. Samples and handle positions come from the host.
const cyui::Point phaseWidths[]{{0, 0}, {.02, .20}, {.3, .50}, {.7, .75}, {1, 1}};
data.timeKnots = {phaseWidths, 5};
cyui::EnvelopeEditState& gesture = instrumentUi.decayGesture;
const cyui::EnvelopeHandle decay{1, {.3, .4}, cyui::EnvelopeAxes::both};
const auto result = cyui::envelope("##env", gesture, data, {&decay, 1}, cyui::dp(0, 100), options);
if (result.edit.committed()) {
    const auto p = result.edit.value.position;
    instrument.setDecayAndSustain(p.x - attackEndTime, p.y); // one host undo transaction
}
```

A handle can move in time, level, or both. Use `EditPhase::update` for live
previews and `EditPhase::cancel` to restore the original parameters. Advance
`options.revision` on an external edit or preset switch. Keep it stable during
the current drag. A gesture retains its initial input scale so a live preview can
resize the phases without moving the parameter under a stationary pointer.
The caller owns interpolation, audio and undo.

Stage intervals must be finite, ordered and non-overlapping; zero-duration stages
are allowed and have no hit/label area. Labels are caller-localized and appear
only when they fit. Points are ordered within each finite run; gaps remain gaps.
`timePower>0` controls only the display; 1 is linear and .6 expands short phases.
`baseline` is in level units, clamped to `levels`. Options also select colorIndex,
fillAlpha, gradient, grid, phaseLines and phaseLabels. NaN hides optional playhead
or live-level markers; finite out-of-view markers are hidden as well.

The result reports valid/hovered/clicked, inverse-mapped mouse values, the hovered
stage index (`size_t(-1)` for none) and data bounds excluding padding/labels.
Clicks do not edit the model. The parent retains wheel scrolling. Invalid ranges,
stage/point order or budgets produce valid=false with no partial curve. Default
budgets are 65,536 points and 64 stages, with hard caps of 262,144 and 1,024.
Arrays are borrowed for the call; prepare a bounded preview outside the draw loop.

### Curves, graphs and timeline

| API | Use / result |
| --- | --- |
| `evaluateCurve(points, time, fallback)` | Linear, step, smooth or Bezier evaluation of sorted keys |
| `curveEditor(id, CurveState&, points, size, options)` | Editable points/handles and vector edit proposals |
| `arrangeNodes(nodes, links, spacing)` | Deterministic layered positions; returns proposals, tolerates cycles |
| `nodeEditor(id, NodeState&, nodes, links, size, options)` | Move vectors, connectFrom/connectTo, removeNodes/removeLink requests |
| `timeline(id, TimelineState&, lanes, clips, markers, size, options)` | Clip move/trim edits, seek, range selection and remove IDs |

Curve: double-click adds/removes keys; drag a point/selected Bezier handle; Ctrl
extends selection, box-select blank space; Ctrl+A/C/V, Delete, Escape are scoped
to the focused panel. Ctrl controls host/application shortcuts through ImGui's
focus state. CurveOptions.evaluate evaluates the preview with host interpolation;
snapTime can override the default snapping grid.

Nodes: drag cards or pins; Ctrl/Shift multi-select; blank drag box-selects;
Home frames the graph; Delete proposes removing selected nodes; right-click a
link in empty space proposes unlinking. `canConnect(from,to)` validates host pin
types and connection rules before emitting a connect request. `drawBody` draws
host details inside a card; it does not run the graph. The host removes dangling
links when deleting nodes and allocates IDs for accepted new links.

The gallery's image graph uses `NodeOptions::drawBody` and a taller `row` to draw
an image below a card's pin labels. Texture IDs come from the host through
`GalleryResources::imagePreviews` (four optional previews); the host must keep
them alive while the gallery draws. Assets uses the same textures as thumbnails,
and Inspector uses one as an example object preview. These resources are optional:
other hosts and tests show placeholders when no image is supplied. The demo's
generated image pixels are sample data, independent of UI palette colors.

Timeline: wheel zooms at the pointer, Shift-wheel pans, Ctrl-wheel scrolls lanes;
drag clips or their edges to move/trim. Locked clips/readOnly are respected.
Blank drag selects a time interval; the ruler requests seeking. TimelineOptions
supplies playhead, snap/minDuration, revision, formatTime, snapTime and drawClip.
Time units and frame/sample rounding remain the application's responsibility.

### Audio controls and prepared signal views

`modules/Audio.h` retains knob/fader/meter and meterPosition. A fader spreads its
own minimumDb..maximumDb over its travel (size 0 or negative follows ImGui sizing).
The separate AudioViews target offers:

| API | Input |
| --- | --- |
| `makePeaks(samples, columns)` | Min/max envelope preparation, preserving missing-only buckets as NaN |
| `waveform(id, WaveformState&, WaveformData, playhead, size)` | Coverage interval plus prepared per-channel peak arrays |
| `spectrum(id, PlotState&, series, size[, PlotOptions])` | Frequency/value series, logarithmic X (invalid X resets to 20..20000) |
| `oscilloscope(id, PlotState&, series, size[, PlotOptions])` | Prepared time/amplitude series |
| `stereoScope(id, PlotState&, series, size[, PlotOptions])` | Square X/Y scope; both axes share one range symmetric around 0 |

Waveform emits seek/range-selection state, supports wheel zoom, Shift-wheel pan
and middle drag. Decode audio, query envelope caches and compute FFT/DSP on the
host side. Keep raw float audio ownership and thread synchronization out of draw.

### Audio analysis displays

These views belong to **AudioViews**, with the same dependency closure (`Plots`,
`Canvas`, `Core`); linking Audio controls, Curves or an audio SDK is unnecessary.
The native gallery's **Audio analysis** tab (22) and Python gallery show every view.

| C++ / Python API | Display and inputs |
| --- | --- |
| `spectrogram` | Time/frequency/intensity matrix, linear or logarithmic bins and frequency view, palette heat colors, intensity scale, crosshair and exact hovered frame/bin/value |
| `loudness` | Momentary, short-term and integrated LUFS, LRA in LU from absolute low/high LUFS bounds, configurable target marker and overlaid histories |
| `truePeak` / `true_peak` | Per-channel dBTP, held peaks, overs, warning/ceiling markers, scrollable channel list and reset request |
| `correlation` | Signed -1..+1 gauge and optional history; use `stereoScope` / `stereo_scope` alongside it for L/R or prepared M/S phase traces |
| `filterResponse` / `filter_response` | Overlaid Hz/dB gain curves, independent right-side degree axis for phase, editable stable-ID frequency/gain handles |
| `dynamics` | Input/output dB transfer curves, unity reference, current operating point and positive-dB gain-reduction bar |

![Audio analysis gallery](screenshots/audio-analysis.png)

**Spectrogram layout:** `SpectrogramData.values[frame * bins + bin]` uses increasing
frames and bins over `time` and `frequency`. `logarithmicBins` describes stored bin
spacing, independently of `PlotState.y.logarithmic`. Values are dB by default;
`SpectrogramOptions.intensity/unit` configure the color range and displayed unit.
Initialize the view to the coverage, e.g. `PlotState{{{0, 8}, false}, {{20, 20000}, true}}`.
Wheel zooms time, Ctrl-wheel also zooms frequency, and middle drag pans both axes.
`SpectrogramResult` reports `valid`, `hovered`, `clicked`, `viewChanged`, `mouse`,
`frame`, `bin`, `intensity`. Malformed dimensions/ranges are rejected before indexing.
NaN cells stay blank. Rendering samples screen-cell centers with at most `maxCells`
(default 65,536, clamped to 1..262,144); it is not a peak-preserving reduction.
Prepare the desired visible resolution when small transients must remain visible.

**Meter values:** unavailable readings use NaN (`missingAudioValue`); negative
infinity represents silence. Supply LUFS, LRA bounds, dBTP holds/overs and history
series as measurements. `LoudnessOptions` and `TruePeakOptions` customize targets,
level ranges and the reset button. Reset returns `AudioMeterResult.resetRequested`
without changing input measurements. `clippedChannels` counts channels currently
above the ceiling or with recorded overs. The host decides which accumulated values
and histories to clear. Loudness/correlation history Y ranges remain fixed to their
meter scale; their X view supports plot navigation. Give history X a time range.

```cpp
cyui::ResponseState state; // persist per panel
std::vector<cyui::ResponsePoint> bands{{1, 120, 4}, {2, 1800, -7}};
cyui::ResponseOptions options;
options.revision = revision;
auto result = cyui::filterResponse("##eq", state, gainCurves, phaseCurves,
                                  bands, cyui::dp(0, 240), options);
if (result.points.committed()) commitBands(result.points.value); // one Undo entry
// During a drag, evaluate preview curves from state.preview.
```

EQ handles are Hz/dB positions, with unique nonzero 64-bit IDs (at most 1,024).
The result follows begin/update/commit/cancel; drag updates a preview, release
commits, Escape cancels. Changed revision, input model, read-only mode or lost
window focus cancels an active gesture. Invalid/duplicate handles disable editing.
Frequency/gain bounds constrain edits; plot navigation controls the visible view.
Phase series exceeding `PlotOptions.maxPoints` appear in `result.plot.skippedSeries`,
as gain series do. Gain/phase curves remain separately supplied, including previews.

Python uses the same data classes with snake_case fields: `SpectrogramData`,
`LoudnessData`, `TruePeakChannel`, `ResponsePoint`, `ResponseState`, `DynamicsData`.
Pass display options as keyword arguments. Results use snake_case dictionary keys;
`filter_response(...)["points"]` is an `Edit` with `.phase`, `.value`, `.committed`.
The `gain` curves argument uses a different name from the `gains=Range(-24, 24)`
editing bounds. `dynamics` expects a linear dB plot view, e.g. both axes -60..0;
`DynamicsOptions.reduction` / Python `reduction=` sets its attenuation scale.

### Spectral and spatial displays

The **Spectral displays** gallery tab (23) demonstrates filled spectra, frequency
bands, scrolling spectrograms, spatial stereo density and persistent vectorscopes.
All remain in AudioViews/Plots with no additional dependency.

| API / setting | Purpose |
| --- | --- |
| `PlotStyle::area` / Python `Series(style="area")` | Area under a curve with transparency, vertical fade and optional frequency coloring |
| `PlotSeries.bands` / Python `Series.bands` | Explicit X edges for bars, including logarithmic frequency bands |
| `bandSpectrum` / `band_spectrum` | Frequency bands with dB floor, colored/gradient bars and overlaid FFT curves |
| `ColorRamp`, `sampleRamp`, `drawColorScale` | Shared palette ramps, interpolation and labeled vertical/horizontal color scales |
| `SpectrogramOptions.orientation/reverseTime/legend/ramp` | Both axis orientations, both time directions and optional intensity legend |
| `SpectrogramHistory` and `SpectrogramState` | Bounded scrolling history and pausable latest-frame tracking |
| `stereoDensity` / `stereo_density` | L/R input displayed as an XY or normalized M/S vectorscope with glow and persistence |
| `spatialScope` / `spatial_scope` | Frequency × stereo position density, colored by frequency or intensity |

**Filled curves and bands.** Set `baseline` in data units, e.g. -90 dB, together
with `fillAlpha` (0..1), `gradient` and `colorByX`. NaN samples break the filled
curve. Supply increasing frequency points for an ordinary spectral area.
`bands`, when nonempty, must have one finite, increasing-edge `Range` per point.
`bandSpectrum` takes `SpectrumBand{frequencyRange, levelDb}` plus optional overlay
series; its default floor is -90 dB. `BandSpectrumOptions` supplies seriesId, label,
ramp, gradient, colorByFrequency and ordinary PlotOptions. Keep overlay IDs distinct
from the band series ID. `PlotResult.series/index` identifies the nearest sample;
oversized bands are reported in `skippedSeries`, without preparing an unbounded array.

**Color scales.** `RampStyle` offers themed, intensity, frequency and monochrome
ramps; all built-in colors come from the active semantic palette. Intensity goes
from a quiet background toward hot levels; frequency grading progresses from low
to high frequencies. `ColorRamp.colors` optionally borrows 1..64 evenly spaced
palette colors. Invalid counts fall back to the selected built-in ramp.
`sampleRamp` clamps fractions and interpolates; missing fractions select its low end.
`drawColorScale(rect, ramp, range, unit, vertical, logarithmic)` draws inside the current canvas
clip without reserving an item. A vertical scale has its minimum at the bottom; `logarithmic=true` positions its frequency ticks logarithmically.

**Scrolling spectrograms.** `timeHorizontal` is the default; `timeVertical` places
frequency horizontally. `reverseTime` reverses only the time direction. State axes
remain semantic **X=time, Y=frequency** in every orientation, and mouse results
remain time/Hz. `colorByFrequency=true` uses frequency for hue and intensity for opacity; its legend then shows Hz. `SpectrogramResult.bounds` excludes the optional color legend,
so legend pixels cannot select a data cell. `legend` defaults to false for existing
layouts, and hides itself when the widget is too narrow to keep the data readable.

```cpp
cyui::SpectrogramHistory history; // persist; reset once when the stream format changes
history.reset(256, 128, 0.01, {20, 20000}, true); // capacity, bins, hop seconds, Hz, log bins
// Once per new analysis frame, outside draw: history.append(preparedBins);
cyui::SpectrogramState state; // persist per panel
state.window = 2.0;
cyui::SpectrogramOptions options;
options.orientation = cyui::SpectrogramOrientation::timeVertical;
options.reverseTime = true;
options.legend = true;
options.ramp.style = cyui::RampStyle::intensity;
cyui::spectrogram("##history", state, history.view(), cyui::dp(0, 240), options);
```

History validates its format, keeps chronological order through wraparound, and
accepts only a complete column with the configured bin count. The fixed hop defines
cell coverage from the configured start time; append NaN columns for missing frames.
Invalid resets/appends preserve the preceding state. Capacity × bins is capped at
4,194,304 float cells (up to 32 MiB for ring plus ordered snapshot). `view()` caches
a borrowed ordered snapshot until data changes. Synchronize producer/UI access in
the host. `clear(start)` starts a new time origin. The state overload follows the
latest `window` seconds; zoom/pan pauses following, double-click resumes it, or set
`followLatest=true` explicitly. The original PlotState overload remains manual.

**Persistence and spatial views.** Supply `stereoDensity` with prepared L/R points
and a revision that changes only with new input. Its default M/S projection is
S=(L-R)/2 and M=(L+R)/2; `midSide=false` displays L/R directly. It reserves a square.
`spatialScope` takes `SpatialSample{frequencyHz, normalizedPan, levelDb}` with pan
in [-1,+1]. Its view is pan × frequency, logarithmic in frequency by default.
`SpatialScopeOptions.levels` normalizes level intensity; `colorFrequency` sets the
frequency grading domain, independently of zoom. `colorByFrequency=false` selects
intensity grading instead. Invalid/nonfinite samples are skipped.

Each state owns a `DensityState` with `clear()`. Repeated input revisions decay
without reinjection; new revisions add energy. `PersistenceOptions` controls
resolution (96 by default, clamped to 16..256), halfLife (0.5 seconds), exposure,
pointRadius (design pixels), diffusion (per second), axes/navigation, ramp and
maxPoints (65,536). A nonpositive half-life shows only the current frame. View,
projection and resolution changes clear incompatible history. Oversized input is
reported as series 1 in `skippedSeries`. Use one state per panel/ImGui context.
Dense plots require the renderer's normal ImGui VtxOffset support; reduce point,
cell or density budgets for renderers with a smaller index budget.

Python mirrors these models with snake_case fields. Use `ColorRamp('intensity')`,
`spectrogram(..., orientation='time_vertical', reverse_time=True, legend=True)`,
`band_spectrum(..., floor=-90)`, `stereo_density(..., revision, persistence=options)`
and `spatial_scope(..., revision, persistence=options)`. `PersistenceOptions` is a
dataclass. `draw_color_scale(top, bottom, ramp, values, unit='', vertical=True, logarithmic=False)`
uses screen coordinates within the current clip.

![Spectral displays gallery](screenshots/audio-spectral.png)

### Code, editing and Git-style diff presentation

`CodeDocument::setText(text, language, revision)` preserves original UTF-8 bytes,
tabs, CRLF and final-newline state. Display lines expand tabs to four columns and
carry lexical spans. Languages include plain, C++, C#, Python, JavaScript,
TypeScript, Rust, GLSL, HLSL, JSON, Lua, SQL, INI and Markdown. Lexical coloring is a
presentation heuristic; supply semantic parser/LSP results as overlays when needed.

- `setOverlays(SourceSpan[], revision)` accepts zero-based lines and raw UTF-8
  **byte** offsets. Stale revisions and offsets inside a code point are rejected.
- `tokenColor(role, background)` maps token roles through the current palette,
  adjusting toward the text role for contrast. `paintCodeLine` draws prepared spans.
- `codeViewer(id, CodeViewState&, document, size, annotations)` provides line
  numbers, search/next, selected/focused line, annotation hover and navigation IDs.
- `CodeEditor::setText/text/setLanguage/setReadOnly/draw` wraps the optional MIT
  native TextEditor. draw returns document-changed. `native()` exposes advanced
  TextEditor hooks for completion, annotations, cursors, wrapping and Undo/Redo;
  include TextEditor.h and handle document storage/revisions in the host.
- `PagedCodeEditor` (same CodeEditor target, `modules/PagedCodeEditor.h`) draws
  virtual line fragments without copying an entire file into TextEditor. Code's
  `CodePaging.h` provides the C++17 contracts, UTF-8 validator and streaming line
  window builder. Keep stable nonzero document IDs/revisions and one widget per
  UI thread. Process `draw().read` outside draw, then deliver `supplyPage` on the
  UI thread. Discard/cancel stale reads. Edit results are **proposals**: validate
  and commit one host Undo transaction, then `acceptEdit`; otherwise `rejectEdit`.
  Recover `cancelledInput` instead of dropping user text after an external change.
  The host supplies contextual byte spans, file I/O, search, clipboard extraction
  across uncached pages, Undo and semantic services. UTF-16 positions, UTF-8 byte
  spans and display cells are separate. Basic single-selection editing is not
  the native TextEditor's multicursor/completion/folding feature set. No bidi,
  grapheme shaping/navigation or soft wrap is provided. See
  [CODE_PAGING.md](CODE_PAGING.md) and Gallery Code → Paged editor (editable host
  fixture plus virtual million-line source). No Core, LSP, Python or Web adapter
  is introduced by this library API.
- `DiffDocument::setTexts(before, after, language, workBudget)` prepares a bounded
  comparison; `setUnified(text, language)` reads a unified patch. `rows`, `pairs`,
  `hunks`, `columns`, `revision`, `approximate` expose prepared presentation data.
- `diffViewer(id, DiffState&, document, size)` offers unified/side-by-side modes,
  one shared vertical canvas for both columns, a shared horizontal text offset
  with fixed line-number gutters, hunk/search navigation, inline changed ranges
  and old/new line navigation results. Final-newline-only changes are represented;
  CRLF and LF lines compare equal, with a notice row when line endings differ.
  `DiffState::scroll` reports the current vertical offset; assigning it requests
  a scroll. Hosts should call this viewer directly, without mirroring child-window
  offsets. See [DIFF.md](DIFF.md) for details and native regression coverage.
- `expandTabs(text, tabSize)` widens tabs to the next stop as displayed;
  `displayOffset(raw, byteOffset, tabSize)` maps a raw byte offset to that display text.

```cpp
cyui::DiffDocument comparison;
comparison.setTexts(beforeText, afterText, cyui::CodeLanguage::cpp);
cyui::DiffState view;
auto clicked = cyui::diffViewer("##comparison", view, comparison, cyui::dp(0, 400));
if (clicked.newLine > 0) host.openNewDocumentAt(clicked.newLine);
```

Do not call setTexts on unchanged large documents every frame. Code and Diff do
not depend on Git, LSP, a compiler or CodeEditor. Host repository/authentication,
review comments, merge resolution and actions stay outside the renderer.

### Inspector and assets

`Property` supports bool, int64, double, strings, choices, readOnly/password/
multiline flags and optional bounds. Set integerMinimum/integerMaximum for exact
integer bounds; do not convert int64 through double. `inspector(id, state,
properties, revision, validator)` returns PropertyEdit entries. The validator
returns an error string; invalid drafts remain visible without committing.

`assetBrowser(id, AssetState&, items, size)` uses AssetItem{id,parent,label,type,
thumbnail,folder,favorite}. State owns query, folder, selected IDs and grid/list
mode. It returns open/favorite/rename/remove requests. It performs no filesystem
operation. `CYUI_ASSET` carries the stable numeric ID for host drag/drop handling.
Folder parents and thumbnails are host data; querying/scanning/loading stays outside.

### Images, viewport and optional 3D gizmo

| API | Use |
| --- | --- |
| `imageViewport(id, ImageState&, ImageData, size, ImageTool)` | Texture/UV crop, checkerboard, fit/zoom/pan, pixel hover and crop edit proposal |
| `sampleGradient(stops, position, linearLight)` | Sample unordered stops using sRGB or explicit linear-light interpolation |
| `gradientEditor(id, GradientState&, stops, size, linearLight, revision)` | Select/drag/add/remove stops; returns edits |
| `cameraPosition/cameraViewMatrix(OrbitCamera)` | Arbitrary up axis, column-major right-handed camera |
| `viewport(id, ViewportState&, ImageData, size, navigation)` | Aspect-preserving scene texture, UV picking, camera interaction |
| `transformGizmo(id, TransformState&, Transform2D, options)` | 2D move/scale/rotation overlay, projection/unprojection callbacks |
| `gizmo3D(id, Gizmo3DState&, matrix, view, projection, rect, options)` | Optional host ImGuizmo adapter for column-major scene transforms |

Image: wheel zooms, middle drag pans, double-click fits; the crop tool returns a
rectangle, and Escape restores the previous selection. Gradient: double-click
adds an interpolated stop, drag moves it, right-click removes a stop when more
than two remain. Host color editors change stop colors. Texture lifetime, color
management and document transforms remain host-owned.

The transform gizmo's handles are ImGui items: over a canvas (imageViewport,
viewport, CanvasScope) they take the hover and the click, so nothing underneath reacts.
Host items drawn over a canvas get the same by calling `markCanvasOverlay()` each frame.
Viewport: Alt-left drag orbits, middle drag pans, wheel changes distance. Host
renderers produce the texture; CyUI does not render a 3D scene. Transform2D rotation
is radians. Ctrl snaps rotation to 15-degree increments. Gizmo3DOptions selects
ImGuizmo operation/mode, snap, orthographic view, revision and enabled state.
Link CyUI::ImGuizmo only when a compatible host target is supplied.

### Commands, logs and documents

- `rankCommands(commands, query)` matches every search word and returns stable
  ranked indices. `commandPalette(id, open, state, commands, size)` displays a modal
  palette with keyboard navigation and returns a requested command ID. Disabled
  entries do not execute; the host maps IDs to actions.
- `LogBuffer(capacity,maxBytes)` provides synchronized append/clear/snapshot.
  `logView(id, LogState&, buffer, size)` filters/follows bounded cached rows and
  returns a selected ID; LogState.levels masks debug/info/warning/error.
  `diagnosticList(id, rows, size)` displays source/line/column/message and returns
  a requested diagnostic ID. The host resolves source navigation.
- `Document::setMarkdown` caches CommonMark/GFM blocks, nested inline styles,
  tasks, sized native images, spoilers, tables, anchors and code. `documentView`
  returns link/media/task/resize requests and selectable visible text. Assign
  nonblocking `DocumentServices` for cached native images and link previews.
- `transcript` uses the same renderer and stable message IDs for streamed content.
  `TranscriptMessage::attachments` contains host-owned image/file references;
  image services return native textures. Hover/right-click actions return
  `edit`, `erase`, `reply`, attachment index and download requests. The host
  owns mutations and file operations. `own=false` suppresses edit/delete.
  `TranscriptOptions::editing/editor` inserts a host editor in the message row.
  `messageComposer` provides growing native text input, Enter submit,
  Shift+Enter newline, Escape cancel, formatting and host clipboard import.
  Clear `MessageComposerState::text` after a successful send; set `focus=true`
  to return keyboard focus. Use stable nonzero IDs, including attachment IDs.
  Native example: `examples/markdown/` uses SDL3 on Windows/Linux/macOS, portable
  image codecs and isolated clipboard adapters for images/HTML/files, drag/drop,
  attachments and plain `.md` media. No OS or SDL headers enter CyUI modules.
- Optional `DocumentEditor` provides Live/Source/Split/Reading modes, native block
  rich input, shared C++ Undo, toolbars, search, clipboard/drop hooks and spelling.
  Live supports document-wide Ctrl+A, cross-paragraph dragging, Enter paragraphs,
  Shift+Enter line breaks, Backspace joins, caret navigation and typed Markdown recognition.
  Inline `<br>` serialization preserves breaks and block styles after blur/reload;
  native layout includes trailing empty lines and floating formatting requires a selection. `focus()`
  requests keyboard focus at the current source caret. Web/CEF is optional;
  native images, file attachments and rich clipboard import need no web runtime.
- `transcript` virtualizes variable-height message rows with sparse height indexing,
  a bounded parsed cache and 64-bit logical scrolling. `TranscriptSource` supplies
  `count`, a cached `message(index)` (nullptr when unloaded), nonblocking page
  `request(first,count)` and optional indexed `indexOf(id)`. Increment its revision
  on prepend/reorder/removal. Arrays share this renderer; only explicit ID jumps
  may scan the array. `scrollToIndex` jumps without a scan. See the native guide's
  `virtualization` section and the gallery's million-message source.
- Optional `DocumentIO` reads/writes `.md`/`.cymd` buffers (UTF-8/UTF-16/BOM,
  stored/DEFLATE ZIP, resource budgets) and converts clipboard HTML.
- Optional `mediaPlayer` draws native controls around an existing `MediaBackend`.
- Optional `DocumentWebAdapter` connects activated embeds to an existing
  `WebBackend`, from CyUI Web/CEF or the host. The host still pumps its runtime.

See the offline `native-markdown` guide for contracts, recipes and limits.
Unknown HTML stays text. No renderer runs scripts, opens links or fetches URLs.
Gallery: 18 reader/chat, 24 editor, 25 simulated host playback.

### Optional Web / CEF

`webPanel`, WebBackend/WebPlacement/WebInput, WebPolicy/webOrigin, webThemeJson
and webEventJson form the engine-independent browser module. `createWebCEF`,
`webCEFApp` and `webCEFBrowser` provide the optional CEF implementation. The
browser draws in a host-owned texture and shares CyUI theme roles through CSS.
See [WEB.md](WEB.md) for the complete lifecycle, native/Python recipes, TypeScript
bridge, CyMD iframe protocol, origin checks, process sandbox and OS packaging.
CEF is separate from Core and from the module's neutral API. The local validated
runtime is Windows; Linux/macOS build/deployment validation is explicitly pending.

Rules when using it:

- Pump `CefDoMessageLoopWork()` (Python `runtime.pump()`) outside the ImGui frame,
  before `NewFrame`. `WebCEFOptions::upload` and the optional `release` callback run
  inside it; `release(texture)` comes once after the browser closed: free that
  texture only after the frames that used it (Python: `create(..., release=)`).
- `external` events are page requests, never commands. Only
  `WebPolicy::externalSchemes` (default http/https) are reported; check
  `userGesture`, `mainFrame` and `sourceOrigin`, apply your own allow-list and ask
  the user before opening anything with the shell. Sub-frame popups without a
  gesture are dropped.
- `WebEventKind::dropped` (data = count) reports events discarded by the queue caps
  (`maxQueuedEvents`, `maxQueuedErrors` for errors/console) or by policy.
- `WebPanelOptions::focus` is one-shot (taken on a false -> true change). A focused
  panel owns the keys it forwards, so ImGui nav and other widgets do not act on them.
- Backend calls must run on the CEF UI thread; others are ignored. The supplied
  bridge, helper and demo run CEF unsandboxed on Windows (WEB.md "Process sandbox").

### Python examples

```python
from cyui_modules.curves import CurvePoint, CurveState, curve_editor
from cyui_modules.common import Range

state = CurveState()
state.view.x.view = Range(0, 10)
state.view.y.view = Range(0, 1)
points = [CurvePoint(1, 0, .2), CurvePoint(2, 10, .8)]
# Each ImGui frame:
edit = curve_editor('automation', state, points, (0, 260), revision=revision)
if edit.committed:
    points = edit.value
```


Python optional APIs are snake_case equivalents in the packages listed in
[MODULES.md](MODULES.md). Python source offsets for SourceSpan are raw UTF-8
bytes; displayed lexer spans use Python character indices. Do not mix those two
coordinate systems. `CodeEditor` in Python offers native ImGui multiline Undo
plus a syntax preview; C++ TextEditor advanced callbacks are not exposed there.
`CEFRuntime` is an explicit optional browser loader; importing model modules is
independent of imgui-bundle, NumPy, audio/video or browser engines.

## 9. Unreal Editor connector (optional)

Prepare `integrations/unreal/CyUI` with `tools/package_unreal.py --layout module`,
selecting only the required modules. Copy the resulting folder to Source/CyUI
inside an existing plugin and declare its Editor module and Build.cs dependency.
For several consumers, use one shared provider (`--layout plugin`) and add the
provider to each consumer's .uplugin Plugins array. Never define CyUI twice in a
project. The module adds no menu or demo window when loaded. It owns ImGui frames,
renderer-managed fonts, clipping, input and clipboard. A separate Editor module depends on `CyUI`
and uses `CyUIModule.h` / `FCyUIModule` to embed MakePanel or register an
`FOnDrawCyUITool` with RegisterTool; unregister it before the owner unloads.
Draw contents without another root Begin/End/frame. A plugin redistributed to
others embeds the module under its own name (`--module-name MyToolCyUI`, then
depend on `MyToolCyUI`) or uses the shared provider; two modules named CyUI
cannot load together. Check `FCyUIModule::IsAvailable()` before `Get()` at shutdown.

`MakePanel` embeds a callback in another Slate layout. `RegisterTexture` returns
an ImTextureID-compatible handle for a pinned Unreal texture; release it after
stopping its use. Module feature definitions and DLL exports are propagated by
UBT. No Unreal code enters CMake's Core or other standalone builds.

Panels use separate ImGui contexts. Each has its own CyUI palette, density and
fonts (see "Several ImGui contexts" in section 1). Numeric entry and
notifications stay local to their context. Outside the draw callback (Slate
handlers, async results) use `SCyUIPanel::RunInContext([] { cyui::setTheme(...); })`
and `SCyUIPanel::Notify(text, Tone)` (or `cyui::notify(context, ...)`). Each key goes
to one side: ImGui only while it takes keyboard input (active item, text field, or a
focused view calling `ImGui::SetNextFrameWantCaptureKeyboard(true)`) or for plain
nav keys; otherwise Unreal, so editor shortcuts never run twice. All drawing runs on the
editor game thread; host transactions and expensive data preparation remain the
tool's responsibility. See [UNREAL.md](UNREAL.md) for complete recipes, lifecycle,
automation tests, platform validation and current input/backend limitations.


## Shared C++ syntax catalogue and plugin engines

`CyUI::Code` owns 102 built-in syntax modes, including HLSL, GLSL and Verse. CodeEditor,
CodeDocument and Diff share it. Read [CODE.md](CODE.md) before implementing a host
syntax extension. Use `codeLanguages()` for menus and `codeLanguageForPath()` for
associations; avoid maintaining a second list in the application.

Register `CodeLanguageDefinition` with declarative rules, a built-in base and word
lists, or a host callback returning validated UTF-8 byte spans. Registration and
removal increment `codeLanguageRevision()`; cached views re-highlight on that
revision. Registration never edits the document or adds an Undo transaction.
CyUI does not load plugin DLLs or parse application manifests. Hosts retain those
responsibilities, pin callbacks and their libraries during calls, and unregister
definitions before unloading plugins. Built-in IDs cannot be overwritten.

Gallery's Code page includes a syntax selector for the shared registry. The
native backend accepts shared display spans without replacing its lexical state;
hosts with richer syntax overlays can use `CodeEditor::setSyntaxEnabled(false)`.

### Native grids, spreadsheets and paginated documents

- Link `CyUI::DataGrid` for `GridModel` and `dataGrid`: cell edits, rectangular
  selection, TSV/CSV APIs, row/column structure, common formats and undo. Core only.
- `DocumentEditor` uses DataGrid for Live Markdown tables. Cell input accepts
  inline Markdown; source undo covers table changes.
- `CyUI::Spreadsheet` provides `Workbook`, `SpreadsheetState`, `spreadsheet`,
  `evaluateWorkbook`, structural A1 rewrites and relative copy/paste. Calculation
  is a bounded subset, not a complete Excel/OpenFormula engine.
- `CyUI::RichDocument` provides `RichDocumentData`, `RichDocumentEditor` and native
  page layout. Double-click a paragraph/table/image to edit that block. Page setup
  controls dimensions, margins, header/footer and page fields. No browser is used.
- `CyUI::OfficeIO` imports/exports ODT/DOCX, ODS/XLSX and flat FODT/FODS. APIs use
  memory buffers, including packaged image bytes, and never launch another app.
  Inspect `OfficeIssue`; `strict=true` rejects recognized lossy conversions.
  Conversion is not a document-fidelity guarantee.
- These new Office APIs/widgets are C++ APIs. The existing Python editor retains
  its previously documented capabilities.
- Gallery: 26 spreadsheet, 27 paginated document, 28 standalone grid.

The bilingual `office` manual documents selection, examples, supported formulas,
format limits and real LibreOffice validation commands.

## 10. Editor tools: Inspector, Hierarchy, DataView, FilePicker and Commands

`Hierarchy` and `FilePicker` depend on Core; `DataView` also depends on Inspector
to reuse all typed editors. These optional targets use C++17. Select them in
`CYUI_MODULES`; include their matching `modules/*.h`.
Gallery tabs 30, 31 and 32 demonstrate them. Inspector tab 12 defaults to its
advanced example; Nodes (7), Timeline (8) and Commands (16) demonstrate extensions.
The maintained [editor tools manual](../Documentation/Documentation.html#/en/editor-tools)
contains the complete data/transaction contract in English and French.

### Numeric expressions

Core includes `Numeric.h` and `evaluateNumericExpression(text, originalValue)`.
It parses arithmetic, never code: `Value`, `+ - * / % ^`, parentheses, `pi`, `e`,
the `k` suffix and `abs/sqrt/min/max/clamp/round/floor/ceil`. Syntax, depth,
division by zero and non-finite results are checked. `Value` means the value at
edit start, not an incrementally updated result. Input is limited to 4096 bytes
and 32 nested operations. Function arguments use commas, decimals use a dot.

Core's typed `valueEntry` (used by dragValue, sliderBar and audio controls) accepts
expressions and clamps the result to its bounds. Invalid input stays open with
an error. Inspector numbers open an expression editor on double-click, focused
Enter or the context menu; vector components share Core's numeric entry.

```cpp
cyui::Property strength;
strength.id = 1;
strength.key = "strength";       // portable key, independent of label/translation/object ID
strength.label = "Strength";
strength.category = "Material";
strength.value = 2.56;
strength.bounded = true;
strength.minimum = 0;
strength.maximum = 10;
cyui::PropertyValue result;
std::string error;
cyui::evaluatePropertyExpression(strength, "Value*100", result, error); // double(10)
```

### Typed property clipboard and multi-object editing

PropertyValue retains its original alternatives and adds `float`,
`PropertyVector{{x,y[,z[,w]]}}`, `PropertyColor{{r,g,b,a}}`,
`PropertyAsset{assetType,path}`, recursive `PropertyArray`, typed `PropertyObject`
records and host-defined `PropertyCustom` values. Colors are data values in [0,1].
Arrays are homogeneous and use a fixed-height virtual ListView: only visible rows
and an active offscreen row are submitted. There is no 1024-element display cap;
the ImGui row index limit is INT_MAX. `InspectorOptions::arrayHeight` is in logical
units (default 180). Nested arrays and records open detail popups, preserving fixed
row heights and independent scrollbars. Numeric elements and vector components
accept expressions, including float and exact int64 entry. Element context menus
support typed JSON copy/paste, duplicate, move up/down and removal.

```cpp
cyui::Property points;
points.id = 1;
points.label = "Points";
// Explicit immutable element template preserves the schema when empty and supplies Add defaults.
points.value = cyui::makePropertyArray(cyui::PropertyVector{{0,0,0}}, 10000);
cyui::PropertyObject spawn{"SpawnPoint", {
    {"Name", std::string("Spawn")},
    {"Position", cyui::PropertyVector{{1,2,3}}},
    {"Weights", cyui::makePropertyArray(.5f, 3)}
}};
points.value = cyui::makePropertyArray(spawn, 10000);
// PropertyArray{{values...}} remains supported; its first element supplies the schema.
// An empty array without a template starts with a string when Add is clicked.
```

`PropertyObject{type,fields}` uses stable untranslated field keys and validates the
entire recursive schema. `PropertyCustom{type,json}` carries any strict JSON payload;
the stable type identifier must match on paste. Provide `InspectorOptions::editCustom`
to adapt application-specific C++ values. Its callback receives the root property ID,
an array-index/object-key path and the custom value, and returns `Edit<PropertyCustom>`.
Draw one frame-height row respecting the current item width; report begin/update/
commit/cancel and let the host apply parent property commits as usual. Without a
callback, a JSON editor is available. `browseAssetAt(id,path)` identifies array and
record resource fields; `browseAsset(id)` is the legacy root-only fallback.

Python exposes equivalent `PropertyObject`, `PropertyCustom`, `make_property_array`,
and `inspector(..., array_height=180, edit_custom=..., browse_asset=...)`; callbacks
use `Edit` and the same root ID/path contract. Nested numeric entries accept
expressions directly with Enter. `InspectorState::lists` (`state.lists` in Python)
reports last-frame `renderedRows` (`rendered_rows`) for profiling. Large collection
drafts are cached: increment revision whenever the host changes/replaces the model
externally, including changes while idle. Commits and structural edits invalidate
the affected draft automatically. Full model validation/copying happens on edits,
not on each idle frame. Property nesting is limited to 12 container levels.

Right-click a property or category header for JSON copy/paste. `propertyToJson`
and `propertyFromJson` also work outside drawing. The clipboard schema is versioned:

```json
{"format":"cyui.inspector","version":1,"kind":"property","property":{"type":"double","value":2.56}}
```

A category replaces `property` with `properties`, a map keyed by `Property::key`
(label fallback), and uses `kind:"category"`. Category names and object IDs are
not part of the match: another object/category with exactly the same keys and
types is a valid target. Vector dimensions and asset classes must match. Paste
normalizes/clamps each value, enforces read-only and choice rules and invokes
the host validator before returning any commits. A category paste is atomic:
apply the complete edit vector as one host Undo transaction. JSON is limited to
1 MiB / 32 JSON levels / 65536 nodes, independently of the number of displayed rows.
Version 1 adds optional typed `element` templates to arrays, `object`/`objectType`
records and `custom`/`customType` payloads; existing scalar/array envelopes still work.
Export enforces the same budgets as import. int64 values are JSON integers and never converted to double.
Exact int64 literals are supported in expression entry; arithmetic on int64
originals outside ±(2^53-1) is refused on platforms without wider floating point.

`commonProperties(objects)` keeps compatible fields, marks `mixed`, intersects
bounds and respects read-only fields. Apply each commit to every matching object
in one Undo transaction. `defaultValue` enables Reset to default. Advance revision
when an external replacement makes an active edit stale.

### Sets, maps, optional values and field metadata

`PropertySet` and `PropertyMap` provide Unreal-style Set/Map editing without an
engine dependency. They share the virtual list and every recursive property editor
with arrays. Sets contain unique typed values; maps contain unique typed keys and
typed values. Keys may themselves be vectors, records or other compound values.
Membership ignores order; presentation retains insertion order. Uniqueness is
checked after normalization/clamping, including edits, Add and JSON paste. A
collision rejects the entire edit. Add stages a candidate before inserting it.

```cpp
auto tags = cyui::makePropertySet(std::string("Tag"));
tags.values = {std::string("Player"), std::string("Visible")};
auto weights = cyui::makePropertyMap(std::string("Key"), .5f);
weights.entries = {{std::string("Walk"), .7f}, {std::string("Run"), .3f}};
auto maybeCount = cyui::makePropertyOptional(std::int64_t(0)); // initially absent
cyui::PropertyEnum quality{"Quality", 1, {{0,"Low"},{1,"Medium"},{2,"High"}}};
cyui::PropertyEnum channels{"Channels", 3, {{1,"R"},{2,"G"},{4,"B"}}, true};
cyui::PropertyObject point{"Point", {{"ID", std::int64_t(1)}, {"X", 2.56}}};
point.metadata["ID"].readOnly = true;
point.metadata["X"].bounded = true;
point.metadata["X"].minimum = 0;
point.metadata["X"].maximum = 10;
```

`PropertyMetadata` also supplies labels, help, exact integer bounds, choices,
password and multiline settings. It belongs to the destination schema and cannot
be replaced by clipboard data. Array structural edits preserve read-only record
fields. `PropertyOptional` owns a nullable value and its immutable element template;
`PropertyEnum` uses a stable type name and int64 values, with an optional flags mode.
Unknown enumerators or flag bits are rejected.

Ctrl/Shift selects collection rows; Ctrl+A selects filtered rows and Delete removes
the selection. Search filters the virtual list. The context menu applies typed JSON
or a numeric expression to the whole selection atomically. Each expression's
`Value` is that element's original value. Maps apply bulk edits to values. Arrays
also duplicate and reorder selections by dragging their row indices; Sets/Maps
retain unique membership. `editPropertyCollection` exposes these operations without
drawing. `propertyEditor` embeds the same editor and transaction phases in a host UI.

JSON version 1 adds `set` with typed `value` elements and an optional `element`
template; `map` with `value:[{"key":typed,"value":typed}]`, `keyTemplate` and
`valueTemplate`; `optional` with null or a typed `value` and `element`; and
`enum`/`flags` with `enumType`, an int64 `value` and `{value,label}` items.
Clipboard budgets remain independent of collection display size.

Python mirrors these models and helpers with snake_case names. Map `entries` are
`(key,value)` pairs, allowing compound keys; metadata bounds use `minimum`/`maximum`
or `None`. `property_value_editor` is the standalone editor. Galleries **33–35**
demonstrate arrays, Sets and Maps with up to 100000 entries and all property types.

### Hierarchy and tables

`hierarchy(id,state,items,size,options)` renders scenes/layers supplied as flat
`HierarchyItem{id,parent,label,kind,visible,locked}` data with nonzero IDs, root 0.
It preserves ancestors during filtering, supports Ctrl/Shift selection and
rename/duplicate/remove/visibility/lock requests. Drag onto a row to reparent;
Shift-drop inserts before it; the root drop zone detaches to root. Cycles, locked
sources/targets and host `canReparent` vetoes are enforced. The host applies edits,
allocates IDs and decides subtree duplication/deletion semantics.

Arrow keys navigate visible rows: Right expands or enters a child; Left collapses
or selects the parent. Home/End/PageUp/PageDown, Ctrl/Shift selection, Space and
Ctrl+A are supported. F2/Enter requests rename and Delete requests removal.
`navigateHierarchy` exposes the navigation model without rendering.

`dataView` uses ImGui tables with reorderable/hideable/resizable columns,
multi-column numeric sorting, per-column filters and clipped rows. `DataCell` is
`PropertyValue`: every Inspector type works, including vectors, colors, assets,
arrays, Sets, Maps, optionals, enums, flags, records and custom types. Double-click
opens the corresponding editor. `DataColumn::metadata` supplies field rules and
`DataViewOptions::editors` supplies custom/resource callbacks; callback IDs are
column IDs and paths begin with the row ID. Shift-click selects a rectangle;
Ctrl+C/V exchanges quoted TSV. Rich cells contain typed property JSON; primitive
cells retain plain text. Numeric cells accept arithmetic and bounds. Paste is validated atomically, including
read-only rows/columns and the host callback. State/options revision cancels stale
cell editing. `copyDataCells` and `pasteDataCells` expose the same model contract.

### FilePicker and command routing

`filePicker` accepts a cached `FileListing` for the requested directory. The result
reports navigation, cancellation or accepted paths for openFile/openFiles/saveFile/
directory modes. Supply async/VFS results on the next frame; stale/loading/error
snapshots cannot be accepted. Filters are case-insensitive extensions, and save
asks before replacing an existing entry. Paths are lexically restricted to the
configured root; OS permissions, symlinks, existence checks at write time and
actual filesystem operations belong to the host. No OS picker is created.

`CommandRegistry` owns common command metadata for palette snapshots, menuItem,
toolbarButton and keyboard poll. IDs are nonzero and contexts are ordered from
most specific to global. Rebinding rejects same-context collisions; the same
chord may serve distinct contexts. Poll once per frame in the focused host window;
typing suppresses commands unless allowWhileTyping is set. `bindingsJson` /
`loadBindingsJson` exchange versioned bindings atomically; shortcutEditor captures
a chord (Escape cancels, Backspace clears). The registry returns action IDs;
execution and persistence belong to the host.

### Nodes and Timeline extensions

NodeOptions accepts borrowed `groups`, `reroutes` and a fresh nonzero `allocateId`
callback. Comment groups move/resize/rename, reroutes move, and context menus
create/remove them. Ctrl+C/V/D copies/pastes/duplicates a selection with internal
links and reroutes. copyGraphSelection/copyNodeSelection/nodesToJson/nodesFromJson/cloneNodes expose
the bounded JSON contract. Clone remaps every node/pin/link/group/reroute ID;
original ID namespaces remain independent. The host applies `created`, committed
overlay edits and removals alongside the existing node edit requests.

Nodes, comment groups, links and reroutes support Ctrl/Shift multiselection,
Ctrl+A, context **Copy/Paste/Duplicate** and Ctrl+C/V/D. Right-click an unselected
element targets it; right-click a selected element keeps the mixed selection.
Box selection includes contained cards/groups and reroutes. Groups copy their
fully contained subgroups recursively and the nodes whose anchors lie inside.
Pins, internal links and their reroutes travel with the copied nodes. Explicitly
copying a link or reroute also includes its endpoint nodes, making the fragment
self-contained for another graph. Unrelated external links are excluded.

`NodeState::selection` contains node IDs; `selectedGroups`, `selectedLinks` and
`selectedReroutes` hold the other kinds separately. Use `NodeSelection{nodes,groups,
links,reroutes}` with `copyGraphSelection` for model-only operations; the legacy
`copyNodeSelection` keeps its combined node/group ID contract. Separate namespaces
allow legacy node/link/group IDs to overlap without selecting unrelated elements.
Python mirrors this with `NodeSelection`, `copy_graph_selection` and snake_case
state fields. The Python module gallery supplies the complete host application.

Paste anchors the whole fragment at the cursor/context position; Duplicate uses
a small offset. Every node, pin, link, group and reroute receives a fresh nonzero
ID, with references remapped and relative layout preserved. Apply `created` as one
host Undo transaction. Copy is available in read-only mode; Paste/Duplicate require
`allocateId`. Invalid JSON or allocator conflicts preserve the model and selection.
JSON v1 import/export uses the same 1 MiB/32-level/65536-node limits as Inspector.
Delete returns separate `removeNodes`, `removeGroups`, `removeLinks` and
`removeReroutes`; the host also removes incident links and orphaned reroutes.
The single-link context action retains `linkRemoved`/`removeLink` for compatibility.

Timeline accepts allocateId for S (split at playhead), Ctrl+D (duplicate selected
unlocked clips) and new markers. Marker dragging/renaming/removal and lane-name
drag reordering return separate markers/lanes Edit vectors. splitTimelineClips,
duplicateTimelineClips and reorderTimelineLane are usable without drawing. IDs,
locked clips and minimum durations are preserved. Host revision/read-only/Escape
cancel stale gestures; the host owns each Undo transaction.

Moving a comment group also moves nodes whose anchors were inside it at gesture
start and their contained reroutes. Apply all returned `groups`, `moved` and
`reroutes` commits together. Escape/revision cancels the entire gesture. Set
`NodeOptions::moveGroupContents=false` for overlay-only movement; resizing keeps
contents fixed. Python provides the same behavior and Timeline's **Add marker**
context action (`add_timeline_marker` is also usable without drawing).

The **Rich data view** gallery (36) embeds all typed cell editors. **Editor
workspace** (37) shares one model/selection across Hierarchy, Inspector, DataView
and FilePicker, with Ctrl+Z/Ctrl+Y and atomic Undo/Redo. The reusable host example
is `include/cyui/examples/EditorWorkspace.h`; adapt its snapshots to the application's
history system. It validates the whole batch before changing the model or history.

### Python

`cyui_modules.hierarchy`, `dataview`, `filepicker`, `command_registry`, `numeric`
and Inspector's typed clipboard models import without a renderer. The UI functions
load imgui-bundle lazily. Python Inspector uses PropertyFloat for float32 and
PropertyVector/Color/Asset/Array for compound values. property_to_json /
property_category_from_json and CommandRegistry.bindings_json share the native
schemas. nodes.py exposes graph fragment helpers and timeline.py exposes clip
operations. All I/O, resource loading and committed model updates stay in the host.
