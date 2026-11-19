<table width="100%">
  <tr>
    <td width="300" align="left" valign="top">
      <a href="https://mrmybal.github.io/CyUI/">Showcase</a><br>
      <a href="Documentation/Documentation.html#/en/overview">Manual (offline)</a><br>
      <a href="https://github.com/MrMybal/CyUI">GitHub</a><br><br>
      <sub>README</sub><br>
      <a href="README.md">English</a> · <a href="README.fr.md">Français</a>
    </td>
    <td width="400" align="center" valign="middle">
      <a href="https://mrmybal.github.io/CyUI/"><img src="site/assets/mark.svg" alt="CyUI" width="128"></a>
    </td>
    <td width="300" align="right" valign="top">
      <p><sub>Library license</sub><br><a href="LICENSE"><strong>MIT</strong></a></p>
      <p><sub>Versions</sub><br><a href="VERSION.md"><strong>Library 1.2.0</strong><br><strong>Demo 1.2.0</strong></a><br><strong>IN DEVELOPMENT</strong></p>
      <p><a href="https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip"><strong>Download the demo</strong></a><br><sub>Windows x64 · portable</sub></p>
    </td>
  </tr>
</table>

<h1 align="center">CyUI</h1>

A themed, modular UI library on top of **Dear ImGui 1.92+** for developers of
desktop editors, creative software and technical tools. Build a consistent C++
or Python interface, starting with Core and selecting the modules your application uses.

> **IN DEVELOPMENT · Library 1.2.0 · Demo 1.2.0**
>
> Try the published Windows gallery, then integrate the library into your own
> application. The current sources include the bilingual CyDoc manual; the
> published demo is an earlier snapshot. Read the [version notes and documented limitations](VERSION.md).

[![CyUI widgets, audio controls, docking layout and inspector in the Mid grey theme](docs/screenshots/mid-grey.png)](docs/screenshots/mid-grey.png)

## About CyUI

Use the same visual foundation for forms, content browsers, details panels,
audio displays, editable curves, node graphs, code editors and timelines.
The application keeps its data, renderer, processing engines, file operations,
language services and Undo history. CyUI supplies the interface components.

CyUI targets **Windows, Linux and macOS** through the host application's ImGui
backends. The available portable demo is Windows x64; the Linux/macOS library
targets are supplied as source. CEF browsing and Unreal Editor embedding are
optional integrations with separately supplied runtimes.

### Module showcase

These are captures of the running C++ gallery. The examples use synthetic data;
applications supply their own models, rendering, processing and services.

#### Audio displays

Stereo waveforms, logarithmic spectra, oscilloscopes and stereo scopes share the
same palette as the knobs, faders and meters. `AudioViews` displays prepared data;
audio decoding, FFTs and DSP remain in the application.

![AudioViews: stereo waveform, spectrum, oscilloscope and stereo scope](docs/screenshots/audio-views.png)

AudioViews also includes a spectrogram, LUFS/LRA and true-peak displays, correlation history, editable EQ handles with gain/phase overlays, and dynamics transfer with gain reduction. These additions are available in the current sources and the gallery’s Audio analysis tab.

The **Envelopes** gallery tab (38) demonstrates host-prepared ADSR/AHDSR curves,
phase labels and live markers. `cyui::envelope`, in AudioViews, uses the shared Plots fill
renderer, also available as `cyui::drawCurveFill` for custom canvases. Zero sustain,
repeated points and baseline crossings are supported. See the [C++ recipe](docs/AI_GUIDE.md#envelopes-and-reusable-curve-fills).

![Audio analysis: spectrogram, LUFS, EQ and phase, dynamics, true peak and correlation](docs/screenshots/audio-analysis.png)

The Spectral displays tab adds filled/gradient spectra, frequency-band bars with a dB floor, scrolling spectrograms with color legends, a spatial stereo view and persistent vectorscopes.

![Spectral displays: filled spectra, RTA bands, scrolling spectrograms, spatial stereo and persistent vectorscopes](docs/screenshots/audio-spectral.png)

#### Blueprint-style node graphs

Colored pins, curved connections, selection, dragging, pan/zoom and automatic
layout provide a foundation for visual editors. The application defines pin
compatibility, graph validation and execution.

![Nodes: a connected voice graph with trigger, oscillator, filter, gain and stereo output nodes](docs/screenshots/nodes.png)

#### Image-processing graphs

Node cards can display host-supplied textures alongside their pins and connections.
This example shows a source image, color grading, blur and output previews. The
demo prepares these images on the host; `Nodes` draws the graph and its contents.

![Image-processing graph with image previews in source, color grade, blur and output nodes](docs/screenshots/image-graph.png)

#### Content browsers and details panels

Combine `Assets` with folder navigation, thumbnail previews and selection metadata
to build a content browser. Asset storage, imports and file operations stay in the
application.

![Content browser with folders, image thumbnails and selected asset metadata](docs/screenshots/content-browser.png)

`Inspector` provides typed, editable properties inside collapsible sections:
strings, choices, toggles, exact integers, numeric values and read-only fields.
The host owns validation and Undo transactions.

![Editor details panel with material preview and grouped identity, surface, texture and rendering properties](docs/screenshots/details-inspector.png)

#### Code editing and comparison

`Code` provides syntax-colored source views and annotations. `CodeEditor` adds
native editing with Undo/Redo and advanced editor hooks. `Diff` presents unified
or side-by-side comparisons; repository access stays in the host application.
The native Diff viewer uses one scrolling canvas for both columns, keeping rows
aligned during wheel, trackpad and scrollbar movement. Long lines share a
horizontal text offset while their line-number gutters stay visible. See
[the Diff guide](Documentation/Documentation.html#/en/diff).

![Code and CodeEditor: annotated source and an editable C++ document](docs/screenshots/code-editor.png)

![Diff: side-by-side C++ changes with line numbers and highlighted additions and removals](docs/screenshots/code-diff.png)

#### Plots, automation and timelines

| Frequency response, distributions and heatmaps | Editable automation curves |
| --- | --- |
| ![Plots with logarithmic frequency axes, multiple series, a histogram and a heatmap](docs/screenshots/plots.png) | ![Curves with keyframes and Bezier handles](docs/screenshots/curves.png) |

![Timeline: clip lanes, markers, a playhead and snapping](docs/screenshots/timeline.png)

#### Find actions and properties quickly

Core includes optional quick filters for panels, property sections and nested
menus. Search English or translated captions, ignore Latin accents, and reveal
matching submenu actions with their breadcrumbs. Each view keeps its own query.
See [filtering APIs and examples](Documentation/Documentation.html#/en/filtering).

![Quick filters in a properties panel and a nested export menu](docs/screenshots/filtering.png)

#### Native Markdown and document editing

Build readers, chat transcripts and editors with native styled text, task lists,
tables, syntax-colored code, spoilers and zoomable images. The optional editor
offers Live, Source, Split and Reading modes with shared undo history. Add file
import/export, playback controls or web embeds only when needed; image textures,
clipboard import and media engines stay under the application's control.
See the [native document guide](Documentation/Documentation.html#/en/native-markdown).

![Native Markdown editor with source and rendered text, tasks, tables and an image](docs/screenshots/markdown-editor.png)

#### Editable grids, spreadsheets and paginated documents

Optional C++ modules add shared cell editing, range selection/clipboard, worksheet
formulas and a native paginated document workspace with margins, headers/footers,
images and tables. OfficeIO reads/writes **ODT/ODS** for LibreOffice and **DOCX/XLSX**,
plus flat FODT/FODS. Inspect conversion issues; supported formatting and formulas
are a documented subset. See the [Office guide](Documentation/Documentation.html#/en/office).

![Native spreadsheet with formulas, formatting and multiple worksheets](docs/screenshots/spreadsheet.png)

![Native paginated document with rich paragraphs, table and image](docs/screenshots/paged-document.png)

## English-first multilingual interfaces

Core includes English and French catalogs, live language switching and an API
for application translations and additional languages. Use `trLabel()` for stable
widget identities and `tr()` for text. The Python API and embedded Web bridge use
the same built-in messages. See [localization and integration rules](Documentation/Documentation.html#/en/localization).

![The same details panel displayed in French, with English remaining the default](docs/screenshots/localization-fr.png)

## Themes and sizing

Five built-in themes share semantic color roles and two densities, **compact**
and **comfortable**. Use `cyui::palette()` for interface colors and `cyui::dp()`
or ImGui style metrics for sizes. Themes can be edited and saved as `.cytheme`.

| Key | Appearance |
| --- | --- |
| `mid-grey` | Mid greys with an orange accent; the default |
| `light-grey` | Light surfaces with dark text and an orange accent |
| `amber-night` | Dark blue-grey surfaces with an amber accent |
| `violet` | Dark slate surfaces with a violet accent |
| `navy` | Deep navy surfaces with a blue accent |

| Light grey | Violet | Navy |
| --- | --- | --- |
| ![CyUI controls in Light grey](docs/screenshots/light-grey.png) | ![CyUI controls in Violet](docs/screenshots/violet.png) | ![CyUI controls in Navy](docs/screenshots/navy.png) |

Fonts and icons are loaded from the OS or host-provided files, rather than
redistributed. Supply suitable fonts and fallbacks for the platforms and scripts
your application supports. Theme, DPI, fonts and notifications are per ImGui
context; initialize each context separately.

## Choose your modules

Each C++ module is a separate static-library target. Dependencies between modules
are selected automatically; for example, `Diff` brings in `Code` and Core, without
adding a code editor, audio engine or browser.

| Target | What it provides |
| --- | --- |
| `CyUI::Core` | Themes, localization, quick filters, fonts, icons, buttons, fields, cards, tables, layout and notifications |
| `CyUI::Audio` | Knobs, faders and peak/RMS meters |
| `CyUI::Canvas` | Grid, clipping, double-precision coordinates, pan and zoom |
| `CyUI::Plots` | Linear/log plots, reusable curve fills, lines, points, steps, bars, histograms and heatmaps |
| `CyUI::Curves` | Keyframes, Bezier handles, snapping, selection and edit transactions |
| `CyUI::Nodes` | Connected cards and pins, graph navigation and layout |
| `CyUI::Timeline` | Clip lanes, trimming, markers, playhead and range selection |
| `CyUI::AudioViews` | ADSR/AHDSR envelope displays, waveforms, spectrum, spectrogram, LUFS/true peak, correlation, EQ/phase and dynamics |
| `CyUI::Code` | Source views, syntax colors, annotations and search |
| `CyUI::CodeEditor` | Editable source using the bundled MIT TextEditor backend |
| `CyUI::Diff` | Unified/split comparisons, inline changes and hunk navigation |
| `CyUI::Inspector` | Typed properties, virtual Array/Set/Map editors, optional/enum/flags, validation and draft edits |
| `CyUI::Assets` | Folder trees, grids/lists, thumbnails, filtering and drag/drop |
| `CyUI::Images` | Image navigation, checkerboard, crop and gradient stops |
| `CyUI::Viewport` | Host-rendered scene surfaces, orbit camera and 2D transform controls |
| `CyUI::Commands` | Searchable keyboard command palette |
| `CyUI::Diagnostics` | Bounded logs, filters and diagnostic navigation |
| `CyUI::DocumentViews` | Native Markdown/GFM, images, spoilers, tasks, tables and transcripts |
| `CyUI::DocumentEditor` | Live/source/split/reading editor |
| `CyUI::DocumentIO` | Markdown/CyMD buffers, HTML clipboard |
| `CyUI::DocumentWeb` | Optional existing web backend adapter |
| `CyUI::Media` | Native playback controls with a host engine |
| `CyUI::DataGrid` | Editable native cells, ranges and table structure |
| `CyUI::Spreadsheet` | Worksheets, formulas and workbook history |
| `CyUI::RichDocument` | Paginated native document workspace |
| `CyUI::OfficeIO` | ODT/ODS, DOCX/XLSX and flat ODF exchange |
| `CyUI::Web` | Browser surface, input, origin policy, messages and theme bridge |
| `CyUI::Hierarchy` | Scene/layer trees, reparenting, renaming and visibility/locking |
| `CyUI::DataView` | Virtual sorted/filtered tables, all Inspector editors and typed TSV clipboard (depends on Inspector) |
| `CyUI::FilePicker` | Host/VFS open/save/directory selection |
| `CyUI::Gallery` | Widget/module examples and the theme editor |

The optional `CyUI::WebCEF` and `CyUI::ImGuizmo` adapters use host-supplied
dependencies. Neither is downloaded or enabled by `CYUI_MODULES=All`.
See [module dependencies and APIs](Documentation/Documentation.html#/en/modules).

---

## Installation

### Try the portable Windows demo

1. [Download CyUI Demo 1.2.0 for Windows x64](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip).
2. Extract the **entire archive** to a writable folder and run `cyui_demo.exe`.
3. Use `Launch-French.cmd` for French, or change the language and theme in the gallery.

The executable uses **Windows x64 and DirectX 11**. No build tools, Python or
separate Visual C++ runtime installation are needed. Read the
[release notes and checksums](https://github.com/MrMybal/CyUI/releases/tag/v1.2.0).
There are no published Linux/macOS demo binaries. The ordinary demo does not
bundle CEF or Unreal Engine; its Web page needs a CEF-enabled build for live browsing.

### Read the local manual

Open [Documentation/Documentation.html](Documentation/Documentation.html#/en/overview)
from your source checkout or a current source-built distribution. Keep the
entire `Documentation/` folder together. The genuine CyDoc reader provides
English/French navigation, search, API recipes and image enlargement inside the
window. Reading needs no server, CDN, account or Internet connection.

In current source builds, **Help → Documentation** or **F1** opens the manual in
the demo's current language. The Python gallery also provides contextual help.
On GitHub, the HTML file link opens its source preview; open the downloaded HTML
file in a browser to read the manual. The older published 1.2.0 ZIP predates this help integration.

## Source, build and required libraries

### Requirements

- **C++17** and **Dear ImGui >= 1.92** for Core; **C++20** for CodeEditor and the tested CEF adapter.
- **CMake >= 3.16** for library integration. The Windows batch demo build uses **Visual Studio 2022 C++ x64 tools**, without CMake.
- **Python >= 3.10** and `imgui-bundle >= 1.92` for the Python interface.
- **Node.js >= 22**, npm and Python >= 3.10 only when regenerating the documentation. The exported manual needs none of these.
- **Pillow** only for the optional screenshot capture scripts.

### Get the source

```sh
git clone https://github.com/MrMybal/CyUI.git
cd CyUI
```

For local development, install [Gitleaks](https://github.com/gitleaks/gitleaks/releases)
and enable the supplied pre-commit hook in your clone:

```sh
git config --local core.hooksPath .githooks
```

### Integrate the C++ library

<details>
<summary>Module selection, initialization and compatibility</summary>

Requirements: **Dear ImGui >= 1.92**, **C++17** and **CMake >= 3.16**.
CodeEditor and the optional CEF adapter require C++20. The docking branch of
ImGui is recommended for applications that use docking.

Add CyUI to your source tree, for example as `external/CyUI`. Supply your existing
ImGui target so the application and CyUI share one ImGui instance:

```cmake
set(CYUI_IMGUI_TARGET app_imgui)
set(CYUI_BUILD_DEMO OFF)
set(CYUI_BUILD_AUDIO OFF)
set(CYUI_BUILD_GALLERY OFF)
set(CYUI_MODULES "Diff;Inspector")
add_subdirectory(external/CyUI)

target_link_libraries(app PRIVATE CyUI::Diff CyUI::Inspector)
```

Use `CYUI_MODULES "Core"` and `CyUI::Core` for the foundation alone. Include
`<cyui/Core.h>` and the headers of the modules you use:

```cpp
#include <cyui/Core.h>
#include <cyui/modules/Diff.h>
#include <cyui/modules/Inspector.h>

// After ImGui::CreateContext(), before the first frame:
cyui::Config config;
config.themeKey = "mid-grey";
config.density = cyui::Density::compact;
config.dpiScale = dpiScale;
cyui::apply(config);
cyui::loadFonts();

// During an ImGui frame:
if (cyui::primaryButton(cyui::trLabel("Save").c_str())) {
    saveDocument();
    cyui::notify(cyui::tr("Saved").c_str(), cyui::Tone::success);
}
cyui::renderNotifications(); // At the end of the UI frame.
```

`CyUI::CyUI` and `<cyui/CyUI.h>` remain compatibility umbrellas for the modules
selected in the build. Audio and Gallery keep their original ON defaults;
disable their options explicitly when unused. Host builds default to no other
optional modules. Ordinary ImGui widgets continue to work with the active theme.

CyUI integrates through `add_subdirectory` or `FetchContent`. It does not provide
an installed `find_package(CyUI)` package: it compiles against the host's ImGui
target. See the [integration guide](Documentation/Documentation.html#/en/integration).

</details>

### Python interface

```sh
python -m pip install -e .
```

The package installs `imgui-bundle`. `cyui` provides the theme and ordinary
widgets; import optional families explicitly from `cyui_modules`, such as
`cyui_modules.plots`, `cyui_modules.diff` or `cyui_modules.audioviews`.
Models can be imported without loading a renderer or media engine. Python's
editor uses multiline editing with a syntax preview; it does not expose every
feature of the C++ TextEditor backend.

See the [Python gallery](examples/python_modules.py) and [Python integration guide](Documentation/Documentation.html#/en/python).

### Optional Web and Unreal Editor integrations

- **Web:** embed HTML/JavaScript interfaces, including compiled TypeScript,
  through a host backend or the optional off-screen CEF adapter. The `web/`
  bridge provides JavaScript messaging, TypeScript declarations and theme CSS
  variables. CEF runtimes are supplied separately per OS/architecture. See
  [Web integration, lifecycle and deployment](Documentation/Documentation.html#/en/web).
- **Unreal Editor:** embed CyUI in Slate panels using the optional Editor module.
  Select the included modules when packaging, then use a shared provider or a
  uniquely named embedded copy. Loading it adds no menus or tool windows.
  It is an Editor integration, not a runtime/UMG backend. See
  [Unreal integration and supported configurations](Documentation/Documentation.html#/en/unreal).

### Try Markdown in a standalone app

The SDL3 app uses the same source on Windows, Linux and macOS:

```sh
cmake -S . -B build/markdown -DCYUI_BUILD_DEMO=OFF -DCYUI_BUILD_AUDIO=OFF -DCYUI_BUILD_GALLERY=OFF -DCYUI_BUILD_MARKDOWN_DEMO=ON
cmake --build build/markdown --config Release --parallel
```

Run `build/markdown/cyui_markdown_test` (on Windows, `build/markdown/Release/cyui_markdown_test.exe`
with Visual Studio). The Windows shortcut remains `build_markdown_demo.bat`, then
`build\cyui_markdown_test.exe`. Enable `CYUI_BUILD_PORTABLE_DEMO=ON` for the SDL3
widget gallery, `cyui_demo_sdl3`. SDL3 is used only by these examples; an installed
SDL3 >= 3.4 is preferred, otherwise a pinned archive is fetched and verified.
Use Discord-style local chat: Enter sends, Shift+Enter inserts a line, small
hover/context actions edit/copy/reply/delete, and real images/files can be
pasted or dropped as attachments. The document defaults to direct rich editing,
with Source/Side by side/Reading modes and plain `.md` + local media saves.
Live typing preserves the caret; Shift+Enter inserts a line without opening the
formatting toolbar. Native input counts trailing empty lines; soft breaks keep their block style and
remain in the document after focus changes and save/reload.
Chat rows are virtualized, with a bounded parsed cache and
a paged source API for large histories (`--scenario million-chat` tests one million messages).
See the [playground guide](examples/markdown/README.md) for controls,
CMake and visual checks.

### Build the Windows demo

<details>
<summary>Compile, package and capture the gallery</summary>

On Windows, with the Visual Studio 2022 C++ tools:

```bat
build_demo.bat
build\cyui_demo.exe
```

The demo builds the bundled Dear ImGui and all ordinary modules. Its gallery
contains interactive examples, a theme selector and a theme editor.

To rebuild and package the portable Windows demo (including licenses and SHA-256
checksums), run `python tools/package_demo.py`. This always recompiles the demo
and checks the extracted application in every theme and both built-in languages.
The ZIP and its `.sha256` file are written to `build/releases/`.

```bat
build\cyui_demo.exe --capture out.bmp --theme violet --tab 7 --gallery-only
```

`--gallery-only` shows the selected gallery page across the window for module
previews. Add `--comfortable` to switch density. To regenerate the README images
from the built demo, install Pillow and run:

```sh
python -m pip install Pillow
python tools/capture_readme.py
```

See [the development guide](docs/AI_GUIDE.md#7-build-and-check)
and the repository's native, Python, Web and Unreal tests.

</details>

### Regenerate the documentation

```sh
npm ci --prefix .CyDocBuild --omit=optional
python .CyDocBuild/build.py
```

The build uses the pinned local CyDoc archive. Only **image** and **gallery** are
included in the reader; Three.js is neither required nor shipped. See the
[documentation workspace](.CyDocBuild/README.md) for generation, translation
tracking and copying the reader into distributions. The
[showcase guide](site/README.md) describes the static site's generation and preview.

---

## License

**CyUI's own code, examples, Python modules, browser bridge and engine adapters
use the [MIT license](LICENSE), including for commercial projects.** Dependencies
retain their own licenses and notices. The repository includes no CEF or Unreal
Engine binaries, and does not redistribute operating-system fonts.

### Native interface and optional integrations

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | [MIT](third_party/imgui/LICENSE.txt) | Native UI; bundled for the demo or replaced by the host's ImGui target. |
| [SDL3](https://github.com/libsdl-org/SDL) | [zlib](examples/markdown/SDL-LICENSE.txt) | Optional native SDL3 examples only; installed or fetched at a pinned revision. |
| [stb_image / stb_image_write](https://github.com/nothings/stb) | [MIT or public domain](third_party/stb/LICENSE) | Portable image codecs in the optional Markdown example only. |
| [stb_truetype, stb_rect_pack, stb_textedit](https://github.com/nothings/stb) | MIT or public domain, at your choice; [bundled notices](third_party/imgui) | Dear ImGui's bundled `imstb_*.h` components. |
| [ImGuiColorTextEdit](https://github.com/goossens/ImGuiColorTextEdit) | [MIT](third_party/texteditor/LICENSE) | Bundled C++ CodeEditor backend only. |
| [Lexilla](https://github.com/ScintillaOrg/lexilla) | [HPND-style permissive license](third_party/lexilla/License.txt) | Selected lexical engines for Code and its dependent modules. |
| [Scintilla](https://github.com/mirror/scintilla) | [Permissive license](third_party/scintilla/License.txt) | Public headers used by Lexilla; no Scintilla editor UI. |
| [SciTE properties](https://github.com/mirror/scite) | [Permissive license](third_party/SciTE-Properties-License.txt) | Keyword/property data for generated syntax profiles. |
| [MD4C](https://github.com/mity/md4c) | [MIT](third_party/md4c/LICENSE.md) | Bundled native CommonMark/GFM parser for DocumentViews. |
| [miniz](https://github.com/richgel999/miniz) | [MIT](third_party/miniz/LICENSE) | Optional DocumentIO DEFLATE archive imports. |
| [pugixml 1.15](https://github.com/zeux/pugixml) | [MIT](third_party/pugixml/LICENSE.md) | Optional OfficeIO XML parser for ODF and OOXML documents. |
| [markdown-it-py](https://github.com/executablebooks/markdown-it-py), [linkify-it-py](https://github.com/tsutsu3/linkify-it-py) | MIT | Pure Python document parsing dependencies. |
| [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) | [MIT](https://github.com/CedricGuillemet/ImGuizmo/blob/master/LICENSE) | Optional host-supplied transform-gizmo dependency. |
| [CEF / Chromium](https://bitbucket.org/chromiumembedded/cef) | [BSD-style and component notices](https://bitbucket.org/chromiumembedded/cef/src/master/LICENSE.txt) | Optional WebCEF SDK/runtime; retain the selected distribution's license, credits and component notices. |
| [Unreal Engine](https://www.unrealengine.com/) | [Epic's separate engine terms](https://www.unrealengine.com/en-US/eula/unreal) | Optional Editor host; CyUI's connector remains MIT. |
| [imgui-bundle](https://github.com/pthom/imgui_bundle) | [MIT and component notices](https://github.com/pthom/imgui_bundle/blob/main/LICENSE) | External Python UI dependency; components retain separate notices. |

### Documentation and generation tools

| Library · source | License · terms | Use / scope |
| --- | --- | --- |
| [CyDoc 0.1.0](https://github.com/MrMybal/CyDoc) | [MIT](Documentation/DocData/CyDoc-LICENSE.txt) | Distributed offline reader, with image and gallery modules. |
| [marked 17.0.6](https://github.com/markedjs/marked) | [MIT](https://github.com/markedjs/marked/blob/v17.0.6/LICENSE.md) | Build-time Markdown compilation. |
| [PrismJS 1.30.0](https://github.com/PrismJS/prism) | [MIT](https://github.com/PrismJS/prism/blob/v1.30.0/LICENSE) | Build-time syntax highlighting. |
| [esbuild 0.25.12](https://github.com/evanw/esbuild) | [MIT](https://github.com/evanw/esbuild/blob/v0.25.12/LICENSE.md) | Build-time reader bundling. |
| [Pillow](https://github.com/python-pillow/Pillow) | [MIT-CMU](https://github.com/python-pillow/Pillow/blob/main/LICENSE) | Optional development tool for screenshot conversion. |

This table indexes the main components. Preserve the complete
[third-party notices](THIRD_PARTY_NOTICES.md), bundled license texts and the notices
of each selected Python, browser or engine distribution. The table does not
replace those notices. See the [licensing guide](Documentation/Documentation.html#/en/licensing).

## References and links

- [English offline manual](Documentation/Documentation.html#/en/overview) · [French offline manual](Documentation/Documentation.html#/fr/overview).
- [Product showcase](https://mrmybal.github.io/CyUI/) · [Présentation en français](https://mrmybal.github.io/CyUI/fr/).
- [Source repository](https://github.com/MrMybal/CyUI) · [Releases](https://github.com/MrMybal/CyUI/releases) · [Issue tracker](https://github.com/MrMybal/CyUI/issues).
- [Version notes and documented limitations](VERSION.md).

<details>
<summary><strong>AI Usage</strong></summary>

- [Start here: repository instructions](AGENTS.md) — project scope and widget development rules.
- [Build with CyUI](docs/AI_GUIDE.md) — integration, API, style rules, migration and build instructions.
- [Maintain the documentation](.CyDocBuild/README.md) — regeneration, translations and offline distribution.
- [Maintain the showcase](site/README.md) — bilingual static pages, genuine captures and local preview.

</details>
