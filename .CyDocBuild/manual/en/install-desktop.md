---
id: install-desktop
title: Install and prepare native readers
description: Windows package and explicit engine registration.
section: Guides
order: 1
parent: desktop
revision: e510be27aea3
---

Extract the complete Windows package to a writable folder and run CyInsightsApp.exe. Keep CyInsightsUpdater.exe and CyCaptureTool.exe beside it. The desktop product targets Windows x64; Android recordings are analyzed on the PC. Preferences and indexes are distinct from original recordings.

## Register Unreal {#register-unreal}
In Session, expand Sources / Native analyzer. Browse engine folder selects the engine root or its Engine directory; choose Add engine. Registration is saved. Prepare native readers builds the matching local adapters with that installation; it does not compile the entire engine. Engine headers/libraries and a compatible C++ toolchain must be present.

Engines are added explicitly, without an automatic disk-wide search. If a reader is missing/incompatible, register the correct engine and retry Analysis > Analyze native timing after preparation finishes. An export bridge remains available for supported providers. Engine binaries from Epic are not redistributed in this package.

Reader version support and plugin compatibility are different questions. An incomplete local SDK can block preparation even when the plugin supports that engine family. Preserve the original .utrace; a persistent index is a cache, not the recording. An antivirus may remove a successfully compiled executable, so verify the exact installed file/report if launch fails.

Next: [Unreal plugin](doc:unreal-plugin) or [Open a recording](doc:opening-traces).
