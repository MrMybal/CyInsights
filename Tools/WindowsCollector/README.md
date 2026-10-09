# Windows collector (MIT)

This isolated CMake project builds the owned MIT Core and ReShade add-on.
It does not link the AGPL session/analysis libraries or Epic libraries.
ReShade SDK (BSD-3-Clause) and its ImGui declarations (MIT) retain their notices.
Use a Visual Studio 2022 x64 developer prompt:

```powershell
cmake -S Tools/WindowsCollector -B build/WindowsCollector -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/WindowsCollector
cmake --install build/WindowsCollector --prefix Plugins/CyGPUInspector/Binaries/ThirdParty/CyGPUInspector/Win64
```

Install/configure the add-on-capable ReShade runtime separately according to
its official distribution and license, and configure its path in the plugin.
No ReShade runtime DLL is committed here. Do not copy an old AGPL add-on from
a historical release and describe it as the MIT build. This source build
includes the collector overlay code, not the CyGPUInspector standalone UI.
Hardware capture validation remains separate from a successful source build.
