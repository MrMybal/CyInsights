# CyInsights 0.2.2 Alpha 1

This is an **alpha prerelease** for development and testing.

## Downloads

- **Windows-x64.zip**: native desktop application, capture CLI, standalone MCP/SDK companion, offline Python dependencies, analysis methods and documentation.
- **Unreal-Plugins-Source.zip**: the single integrated CyInsights Unreal plugin. Copy `Plugins/CyInsights` into your project's `Plugins` directory and build with your licensed engine. Targets UE 4.27 through 5.8; see the [compatibility matrix](UnrealCompatibility.md) for tested versions and limitations.
- **Quest-arm64.zip**: both CyInsightOVR APK variants, the ADB bootstrap, developer bridge and documentation. Extract the whole bundle before installation.
- **Quest-Standard-arm64.apk**: system metrics and the Quest configuration/analysis panel, without the experimental Vulkan layer.
- **Quest-Vulkan-arm64.apk**: adds the optional development Vulkan layer for live tile heatmaps and draw counters. Both APKs use `org.cyinsights.ovr`; install one variant, not both.
- **AI-SDK.zip**: standalone MCP/SDK companion, analysis method, offline Python dependencies and documentation. Requires Python 3.11 x64; run `AI/SetupRuntime.ps1` after extraction. It connects to the desktop application's explicitly enabled local bridge.
- **SHA256SUMS.txt**: checksums for all downloadable packages and APKs.

For the standard Quest installation, run `Start-CyInsightOVR.ps1 -Install -Launch` from the extracted Quest bundle with authorized Android platform-tools. To use the experimental variant, install `CyInsightOVR-0.1.0-dev-vulkan-arm64.apk` using `adb install -r`, then run `Start-CyInsightOVR.ps1 -RestartBridge -Launch` without `-Install`. The developer bridge must be bootstrapped again after a headset reboot. Neither workflow restarts your game automatically. See the [Quest companion guide](../Quest/CyInsightOVR/README.md).

## Quest companion, GPU tiles and draw counters

CyInsightOVR provides a native Quest panel, configurable metric curves and controls for the installed Meta statistics HUD. The experimental Vulkan variant adds a separate placeable tile HUD and an approximate fullscreen heatmap for compatible development targets.

CyInsights can record the companion's measured GPU tile snapshots alongside Unreal timing in a single `.cycapture`, replay the snapshots on the desktop timeline and inspect hot tiles. Optional image recording and association are included, with the image limitations below.

The Vulkan collector reports submitted draw calls and **known input triangle counts**, including instances, reused command buffers and secondary execution. Unknown indirect geometry remains explicitly incomplete. Curve sampling is selectable at **1, 5, 10 or 20 Hz**, with 10 Hz as the default; firmware-provided Meta system counters retain their own update rate. These observations are not counts of visible triangles or completed GPU work.

Collector fixes improve reader shutdown, stale-source recovery and coexistence with Meta's Adreno profiling layer. Detailed profiling availability is checked before requesting the extra layer. Target restarts remain an explicit user action.

## Desktop and integration

- Multiple open traces and detached native trace windows, with corrected graphics backend ownership during window lifecycle changes.
- Quest tile replay and analysis integrated with capture options, the native MCP bridge and the standalone SDK.
- More explicit capture status and errors, bounded background work and configurable metric sampling.

## Alpha limitations

- Image recording can currently return black images on the tested Quest. A reliable game-scene image capture is **not validated**; do not rely on it for mesh identification.
- Tile alignment is observation-based and approximate. Exact eye, mesh and Unreal GPU-frame attribution is unavailable. The tested OpenXR path does not expose `vkQueuePresentKHR` frame boundaries, so per-frame draw counts remain unavailable rather than being inferred from submissions.
- The custom Vulkan heatmap requires a compatible debuggable or explicitly opted-in Vulkan target. It is not a universal overlay for every Quest game. Meta HUD rendering remains provided by Meta's installed service.
- A measured 20 Hz draw stream was verified on Quest 3. Continuous validation of all four sampling rates and a capture-overhead benchmark remain incomplete. Additional collection has a cost; enable only the data you need.
- Windows binaries are unsigned. Android APKs use development signing and remain intentionally debuggable for the authorized ADB bridge. They are alpha development builds, not production store packages.
- Fast native `.utrace` readers are prepared locally from source using your licensed Unreal installation and C++ tools. Epic-linked reader binaries and Unreal Engine binaries are not included.

See the [Quest validation record](../Quest/CyInsightOVR/VALIDATION.md) and [licensing notes](Licensing.md) for details.
