# Third-party components actually used by CyInsights

Owned code license scopes are documented in [Docs/Licensing.md](Docs/Licensing.md).
An inherited broader upstream inventory is kept separately in
[historical notices](Docs/History/UpstreamThirdPartyNotices.md). It is not the
dependency inventory of this application.

| Component | Scope | License / full notice |
| --- | --- | --- |
| Dear ImGui docking (1.93 WIP snapshot) | Native app | MIT; `LICENCES/DearImGui/LICENSE.txt`, source tree license |
| ReShade SDK 6.8.x/API 20 headers | Windows collector only | BSD-3-Clause; `LICENCES/ReShade-SDK/LICENSE.md` |
| ReShade ImGui 1.92.5 declarations | Collector overlay only | MIT; `LICENCES/DearImGui-ReShade/LICENSE.txt` |
| ReShade runtime DLL | External install; not committed | `LICENCES/ReShade/LICENSE-ReShade.txt`; preserve full applicable runtime notices when distributing it |
| CyToolsCore 0.10.1 | Optional AI companion | MIT; `LICENCES/CyToolsCore-MIT.txt` |
| Official MCP SDK 1.30.0 and Python dependencies | Optional AI companion | Package license texts in `LICENCES/AI/`; exact 38 wheel hashes in `AI/dependency-lock.json` |
| Local Python interpreter | External installation used for venv | Python license in `LICENCES/AI/Python-3.11-License.txt`; no interpreter bundled in Git |
| Windows SDK/system DLLs and system fonts | Build/runtime prerequisites | Supplied by the developer/OS; not vendored |
| Unreal Engine/OpenXR supplied by UE | Optional native export/adapter and plugin prerequisites | Epic/SDK licenses; no engine headers/import libs/DLLs included |

The source and headers actually consumed are hash-pinned in
`DEPENDENCIES.lock.json`; the CyToolsCore source commit is pinned separately
in `AI/dependency-lock.json`. The vendored collector does not link the standalone
analysis library. The application does not link Epic libraries.

3Dmigoto, dxil-spirv, SPIRV-Cross, ImGuiColorTextEdit and the other upstream
decompiler components are **not included or linked** by this snapshot. Their
historical license texts are retained in LICENCES for provenance; their presence
is not evidence that these libraries are shipped in the CyInsights executable.
No DirectX Shader Compiler DLL, CyUEMCPSDK, provider CLI or model weights are
included. Pytest and its dependencies are included in the offline companion
wheel set for testing, with their notices preserved.
