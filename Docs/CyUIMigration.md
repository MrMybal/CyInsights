# Modular CyUI integration

CyInsights vendors an unmodified selection of [CyUI](https://github.com/MrMybal/CyUI)
1.2.0 at revision `ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3`.
The upstream HEAD was checked on 2 November 2026. No external checkout or
download is needed to build the application. See the vendored
[integration guide](../ThirdParty/CyUI/docs/AI_GUIDE.md),
[module contract](../ThirdParty/CyUI/docs/MODULES.md) and
[snapshot manifest](../ThirdParty/CyUI/SNAPSHOT.md).

## Production modules

| Module | Desktop use | Quest use |
| --- | --- | --- |
| Core | Theme, fonts, localization, filtering, layout, controls, notifications | Same native foundation |
| Canvas | Measurement surface and double precision image transforms | Transitive dependency of Plots/Images |
| Plots | Counter/frame-stat curves, distributions/CDF, network/storage/memory densities | Metric history curves |
| Timeline | GPU pass overview and selected task lifecycle/range selection | Not compiled |
| Nodes | Derived GPU pass/resource graph, selection and pan/zoom | Not compiled |
| Inspector | Validated palette properties, exact lifecycle timestamps and measured tile stages | Measured tile stages |
| Images | Recorded image inspection and the shared tile heatmap viewport | Tile heatmap viewport |
| Diagnostics | Filtered, clipped application messages and follow mode | Not compiled |

`CYUI_MODULES` is set before `add_subdirectory`. Each application links explicit
`CyUI::<Module>` targets and supplies its existing `CYUI_IMGUI_TARGET` (`CyImGui`
on Windows, `OvrImGui` on Quest). Canvas dependencies are resolved by upstream
CMake. There is one ImGui implementation per application, currently 1.93 WIP
(`IMGUI_VERSION_NUM=19297`), above CyUI's 1.92 minimum. The integration remains
C++17. No C++20 editor/browser runtime was added.

Audio, Gallery, demo, upstream test programs and WebCEF are explicitly disabled.
The desktop no longer compiles the Gallery/theme-editor implementation or links
the compatibility umbrella. The replacement palette editor uses Core's palette
schema and file format with Inspector's validation/edit phases. It retains a
color picker, `.cytheme` load/save, and application-owned Undo/Redo. Repeated
previews do not become history entries; accepted property edits commit once.
Changing the external palette advances the inspector revision and cancels stale
drafts. The developer Gallery menu has been removed.

AudioViews, Curves, Assets, Viewport, Code, CodeEditor, Diff, Commands,
DocumentViews, Web, CEF, Lexilla and TextEditor are not required. The AI workflow
is structured application controls/JSON, not a Markdown/code editor or browser.
Trace A/B comparisons are numerical results, not source-code diffs. No new
external dependency, bundled font or browser process is introduced.

The Unreal plugin does not embed a CyUI/ImGui Editor panel, and Python does not
render this interface. Their existing engine/SDK integration stays independent;
there is no second Unreal CyUI module to package or rename.

## Data and interaction boundaries

Readers, caches, counters, availability flags, exact int64 values, histograms,
task identities, capture settings and asynchronous analysis remain application
data. CyUI receives prepared, bounded presentation arrays; it does not scan the
trace, calculate GPU attribution, perform capture operations or own history.

- Counter queries still use the indexed native model. A cached adapter retains
  first/min/max/last observations in chronological order, preserves source-frame
  gaps as NaN and normalizes only display coordinates to avoid overflow. Exact
  values and provenance remain in the table/tooltips. At most 8,192 visible
  buckets produce fewer than Plots' default 65,536 points per series. This is a
  display budget, not an input-trace size or event-count limit.
- Activity chart preparation happens when an asynchronous visible result is
  published. Range selection still reuses the visible plot query. Stale results
  keep their original time projection and updating notice.
- Histogram bars and CDF use the existing 64-bin results, logarithmic count
  transform, bin tooltips and paging. Clicking a bin still requests exact sample
  examples; dragging the measurement ruler still filters the analysis interval.
- Metric history on Quest is prepared only after new source observations arrive.
  Missing observations and gaps of three seconds break the curve. Polling does
  not invent faster measurements.
- GPU passes, nodes and lifecycle endpoints have stable, nonzero IDs. Derived
  graphs and recorded tasks are read-only; navigation/selection never rewrites
  a recorded command or dependency. Missing lifecycle endpoints stay unavailable.
- Image textures remain owned and released by the existing renderer. Heatmap
  tiles use the exact same Images transform for drawing and picking. Scroll to
  zoom, middle-drag to pan, double-click to fit. State is owned per document/view.
- UI colors use the shared palette; category/heatmap fills describe source data.
  New controls use CyUI/style metrics. Appearance offers English by default and
  the shared French locale, with stable translated IDs. Existing application
  content falls back to English where no translation exists.

## Deliberate application adapters / upstream gaps

The main CPU/GPU flame tracks and frame-cause overview retain their specialized
renderer. The current generic Timeline has a fixed lane-label gutter, receives
complete clip arrays and has no nested-scope depth packing, indexed range
provider, asynchronous display aggregation or configurable right-button XY pan.
It cannot replace this renderer while preserving aligned frame/counter/range
axes, millions of events and existing gestures. The measurement surface already
uses Canvas; minor time ticks, exact time units, frame attribution and the
application's drag state remain host logic. The generic Timeline is used for the
bounded GPU pass and task lifecycle views where its contract is appropriate.

The generic Plots heatmap assumes a uniform rectangular grid. Meta's measured
tiles have explicit, potentially irregular/clipped rectangles and missing stages.
The common Images viewport handles navigation/background/coordinate transforms;
the thin tile overlay retains measured geometry and relative cost colors. It
does not resample tiles, infer an eye/mesh or claim exact image/frame alignment.
These are domain adapters, not modified copies of CyUI.

## Build, refresh and checks

From an x64 Visual Studio developer prompt:

```powershell
python Tools/prepare_dependencies.py
cmake -S . -B build/CyUIModular -G Ninja -DCMAKE_BUILD_TYPE=Release
python Tools/check_msvc_dependencies.py build/CyUIModular
cmake --build build/CyUIModular --target CyInsightsApp CyComponentTests
ctest --test-dir build/CyUIModular --output-on-failure
```

The MSVC/Ninja include prefix is normalized when CMake misdecodes French NBSP
bytes. Header changes must rebuild consumers; the probe checks the generated rule
against real compiler output. This matters when UI state structures change.

```powershell
python Quest/CyInsightOVR/build.py --experimental-vulkan-layer --output bin/Quest
python Tools/sync_cyui.py --source <CyUI-checkout>
python Tools/prepare_dependencies.py
```

The sync tool reads the reviewed pinned Git objects; no paths from that checkout
are saved in application sources. `source-sha256.json` covers all imported files.
The MIT license and distribution notice are retained unchanged.

For runtime inspection, the existing navigation/interaction/range/multi-document
smokes still operate the real application. Additional visual options are
`--palette-editor-view`, `--ui-locale en|fr`, and `--ui-dpi 1|1.5|2`; combine with
`--snapshot <absolute-image-path>` and `--no-update-check`. DPI overrides are for
visual verification; normal execution follows the OS DPI.

Windows x64 and Android ARM64 are the build validation targets. The existing
desktop host is Win32/DX11; this migration does not claim a Linux/macOS desktop
port. Shared modules/adapters retain their portable C++ APIs.

## Validation on 2–3 November 2026

- Fresh Windows x64 Release configure/build, followed by **27/27 CTest suites**.
  The new `CyUiModuleTests` performs **49 checks**: preserved dense extrema,
  unavailable/source-frame gaps, exact integer ordering, bounded display arrays,
  activity time mapping, tile selection/anchored zoom, stale property cancellation,
  message filtering, read-only timeline/node selection, palette Undo/Redo and
  context-local themes/locales. Rendering covers five themes, two densities and
  DPI 1, 1.5 and 2, including geometry scaling.
- The actual executable passed navigation with continuous input across 111
  asynchronous model publications. Right-drag XY pan, wheel zoom, aligned frame
  overview and dynamic track heights were retained. A recorded Quest trace with
  2,750,767 indexed events passed exact-name double-click, context-menu histogram,
  reversed range selection, cached-bin paging and both filter-clear gestures;
  its selected interval reduced 36,320 matching scopes to 38.
- Native frame-stat range selection retained its curve and reduced five rows to
  one. The activity fixture reduced two matches to one while reusing the visible
  plot query. Recorded GPU graph, measured Quest tile snapshot and native task
  lifecycle views were rendered; the selected task came from 12,716 native tasks.
- Multi-trace detachment and document-close/reopen smokes passed. Native captures
  were read only; no game was launched or modified for this migration.
- All ten theme/density combinations were captured from the executable. The
  French palette was inspected at 150% and 200%; translated buttons fit and the
  initial window stays in the main viewport with scrolling for its properties.
- Android ARM64 development Vulkan APK compiled and its existing development
  signature verified. The Quest UI uses Core/Canvas/Plots/Images/Inspector only.
  **No headset interaction test or new live capture was performed for this UI
  migration.** The shared heatmap/interaction adapter was exercised on Windows.
- Compiler/Ninja include prefixes match byte for byte; application/CyUI headers
  appear in Ninja dependencies. No Audio, Gallery, Code, CEF or bundled ImGui
  target appears in the desktop build. The 35 vendored files match their SHA-256
  manifest, and the source publication audit passes.

Local validation artifacts are under `build/cyui-modules-20261102` and are not
versioned. The Windows application and APK are in the corresponding
`bin/CyUI-Modular-20261102-*` directories. Linux/macOS and headset rendering are
not claimed as runtime-tested platforms. No commit, push or release publication
was performed.
