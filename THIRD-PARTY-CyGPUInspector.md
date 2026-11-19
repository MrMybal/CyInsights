# Third-party components actually used by CyInsights

Owned code license scopes are documented in [Docs/Licensing.md](Docs/Licensing.md).
An inherited broader upstream inventory is kept separately in
[historical notices](Docs/History/UpstreamThirdPartyNotices.md). It is not the
dependency inventory of this application.

| Component | Scope | License / full notice |
| --- | --- | --- |
| Dear ImGui docking (1.93 WIP snapshot) | Native app | MIT; `LICENCES/DearImGui/LICENSE.txt`, source tree license |
| CyDoc 0.1.0 at pinned revision | Offline local documentation reader and compiler | MIT; LICENCES/CyDoc-MIT.txt, exported dependency notices |
| CyUI 1.2.0 at pinned revision | Modular native interface | MIT; `LICENCES/CyUI-MIT.txt` |
| ReShade SDK 6.8.x/API 20 headers | Windows collector only | BSD-3-Clause; `LICENCES/ReShade-SDK/LICENSE.md` |
| ReShade ImGui 1.92.5 declarations | Collector overlay only | MIT; `LICENCES/DearImGui-ReShade/LICENSE.txt` |
| ReShade runtime DLL | External install; not committed | `LICENCES/ReShade/LICENSE-ReShade.txt`; preserve full applicable runtime notices when distributing it |
| CyMCPSDK 0.7.0 at pinned revision | Native assistant, CLI and MCP | MIT; `LICENCES/CyMCPSDK-MIT.txt`, `LICENCES/CyMCPSDK/` |
| External Python build interpreter | Build-time documentation/packaging only | `LICENCES/Python-Build.txt`; no interpreter bundled |
| NSIS 3.12 | Build-only installer compiler; generated Windows setup/uninstaller | zlib/libpng and component notices in `LICENCES/NSIS.txt`; zlib compression is selected, compiler is not bundled |
| Windows SDK/system DLLs and system fonts | Build/runtime prerequisites | Supplied by the developer/OS; not vendored |
| Unreal Engine/OpenXR supplied by UE | Optional native export/adapter and plugin prerequisites | Epic/SDK licenses; no engine headers/import libs/DLLs included |

The source and headers actually consumed are hash-pinned in
`DEPENDENCIES.lock.json`; the native CyMCPSDK and CyUI revisions and file hashes
are pinned in their snapshot/source-sha256 manifests. The vendored collector does not link the standalone
analysis library. The application does not link Epic libraries.

3Dmigoto, dxil-spirv, SPIRV-Cross, ImGuiColorTextEdit and the other upstream
decompiler components are **not included or linked** by this snapshot. Their
historical license texts are retained in LICENCES for provenance; their presence
is not evidence that these libraries are shipped in the CyInsights executable.
No DirectX Shader Compiler DLL, CyUEMCPSDK, provider CLI or model weights are
included. Pytest and its dependencies are included in the offline companion
wheel set for testing, with their notices preserved.
