# CyInsights 0.2.2 Alpha 3

This is an **alpha prerelease** for development and testing. Download it manually;
the built-in updater continues to offer only newer stable releases.

## Modular interface

CyInsights now uses CyUI 1.2.0 at a pinned revision. Counter and frame-stat curves,
activity plots, histograms, task lifecycle diagrams, GPU pass timelines, resource
graphs, image navigation and the message log use shared modules. Plot preparation
retains extrema and missing-data gaps while caching visible display data.

Appearance settings include five themes, compact/comfortable density, English
and French controls, and a palette editor with undo/redo. The specialized nested
CPU/GPU timeline retains its indexed loading, exact function filters, range
selection and right-button navigation. Irregular measured GPU tiles retain their
actual geometry over the shared image viewport.

Unused Audio, Gallery, browser and code-editor dependencies are excluded. Quest
uses a smaller module subset and the existing single ImGui integration. See
[module mapping and remaining adapters](CyUIMigration.md).

## CyInsightOVR service

The Quest application owns and starts its foreground GPU service. Reopening
CyInsightOVR after a headset reboot starts the service again; a PC bootstrap is
no longer part of normal service startup. Desktop recording connects to the
installed app's service and exports its recorded tile data through that service.

Meta HUD and external Vulkan-layer configuration still require the one-time
developer authorization described in the [Quest guide](../Quest/CyInsightOVR/README.md).
This Android permission cannot be granted by the APK itself. Enabling a Vulkan
layer still requires reopening the selected compatible development game once.
GPU source availability depends on the headset firmware and selected game.

## Downloads

- **Windows-x64.zip**: desktop application, capture CLI, updater, native-reader
  sources, documentation and optional offline MCP/SDK companion.
- **Quest-Standard-arm64.apk**: standalone Quest metrics and configuration panel.
- **Quest-Vulkan-arm64.apk**: the panel with the experimental tile heatmap and
  submitted draw/input-geometry counters. Install one APK variant.
- **Quest-arm64.zip**: both APKs, optional developer authorization installer,
  SDK bridge and guide.
- **Unreal-Plugins-Source.zip**: integrated CyInsights Unreal plugin sources;
  build them with your licensed engine. See [compatibility](UnrealCompatibility.md).
- **AI-SDK.zip**: standalone MCP/SDK companion, analysis method and offline dependencies.
- **SHA256SUMS.txt** and **release-manifest.json**: package hashes and source revision.

## Validation and limits

The modular interface was tested on Windows with recorded Unreal/Quest traces,
timeline selection/filtering, asynchronous navigation, multiple documents and
detached windows, themes, densities and DPI scaling. The native component suite
contains 27 test suites, including 49 modular-interface checks. Both Android arm64
APK variants are rebuilt and their signatures verified for this release.

No new headset interaction or live game capture was performed for this interface
release. Existing experimental limits remain: scene-image recording can return
black images; tile alignment is approximate; counters describe submitted work
and known input triangles, not visible geometry or completed GPU work. Custom
universal VR overlays and exact mesh/eye attribution are unavailable. See the
[Quest validation record](../Quest/CyInsightOVR/VALIDATION.md).

Windows executables are unsigned. APKs use development signing and remain
debuggable for authorized developer access. Native `.utrace` adapters are built
locally using a licensed Unreal installation; Unreal binaries are not included.

[CyUI](https://github.com/MrMybal/CyUI) is MIT licensed. Its upstream repository is
currently private; the [pinned upstream license](https://github.com/MrMybal/CyUI/blob/ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3/LICENSE)
requires repository access. The [full included MIT license](../LICENCES/CyUI-MIT.txt)
is distributed with CyInsights. See [component licensing](Licensing.md).
