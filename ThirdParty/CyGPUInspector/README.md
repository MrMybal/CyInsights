# CyGPUInspector source snapshot for CyInsights

Only the shared Core, GPU Session/Analysis, Windows collector and necessary
Dear ImGui/ReShade headers are vendored. The standalone CyGPUInspector UI,
decompiler, injector and database are not included or linked.

Source: https://github.com/MrMybal/CyGPUInspector (copyright Cyberalien).
Provenance and exact consumed hashes: ../../DEPENDENCIES.lock.json.
Original consumed hashes: UPSTREAM.lock.json. This snapshot includes local
upstream working-copy changes, not just a pristine Git commit.

CyInsights changes the archive writer to attribute delayed GPU timing to its
execution frame and validates SHA-256 shader filenames before loading files.
The copyright owner authorized MIT licensing of this copy's Core and collector
for Unreal integration. Session/Analysis remains AGPL-3.0-or-later. Each MIT
component has its own LICENSE; the AGPL text here covers Session/Analysis.
ThirdParty libraries keep their original licenses. See ../../Docs/Licensing.md.

The top-level CMakeLists.txt is a snapshot marker. Build the application from
the CyInsights repository root, or the collector from Tools/WindowsCollector.
