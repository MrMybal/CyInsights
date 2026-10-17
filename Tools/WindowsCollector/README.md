# Windows collector (MIT)

This CMake project builds CyInsights' optional internal Windows collector from
the retained MIT Core/ReShade collector code. The source directory keeps its
upstream attribution; no CyGPUInspector executable or Unreal plugin is used.
The result is `CyInsightsGpu.addon64`, with CyInsights-specific IPC names and
exports. GPUInspector sessions are invisible to the CyInsights recorder.
It does not link the AGPL session/analysis libraries or Epic libraries.
ReShade SDK (BSD-3-Clause) and its ImGui declarations (MIT) retain their notices.
Use a Visual Studio 2022 x64 developer prompt:

```powershell
cmake -S Tools/WindowsCollector -B build/WindowsCollector -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/WindowsCollector
cmake --install build/WindowsCollector --prefix Plugins/CyInsights/Binaries/ThirdParty/CyInsightsGpu/Win64
```

Install/configure the add-on-capable ReShade runtime separately according to
its official distribution and license, and configure its path in the plugin.
No ReShade runtime DLL is committed here. Do not copy an old AGPL add-on from
a historical release and describe it as the MIT build. This source build
does not register the original overlay; controls and analysis belong to CyInsights.
Hardware capture validation remains separate from a successful source build.

Native `.utrace` collection is the normal mode. The extra loader is off by default
in both Editor and Development games. Enable it explicitly in Project Settings >
Plugins > CyInsights - Windows GPU collector; a restart is needed because ReShade
must be loaded before device creation. Loading it cannot be enabled midway through
a running game's capture. `-CyInsightsGpu` is the explicit startup override,
`-NoCyInsightsGpu` disables it. Runtime and add-on paths can be specified with
`-CyInsightsReShade=...` and `-CyInsightsGpuAddon=...`.

Connect a local game process in CyInsights > Session > Windows collector before
requesting a detailed capture. Descriptors, barriers, per-command timestamps and
texture/buffer readbacks are separate opt-ins. No GPUInspector application is
searched for or launched. The existing network command can request detail from an
already enabled collector/connected recorder, but detailed payloads still use local
IPC rather than the native Unreal TCP trace stream.
