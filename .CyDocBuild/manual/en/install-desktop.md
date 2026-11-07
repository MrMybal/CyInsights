---
id: install-desktop
title: Install and prepare native readers
description: Windows installer, portable archive and explicit engine registration.
section: Guides
order: 1
parent: desktop
revision: 7145e6b195b8
---

The desktop product targets Windows x64; Android recordings are analyzed on the PC. Both Windows formats include the application, helper tools, this complete offline manual and license notices.

## Choose an edition {#choose-edition}

Use the Setup EXE for a per-user installation with Start menu shortcuts and an entry in Windows Apps. Administrator rights are not required. The desktop shortcut and Open-with registration are optional. Settings remain in LocalAppData/CyInsights; uninstalling removes packaged application files, preserving preferences and additional recordings. Close CyInsights and capture tools before installing or uninstalling.

For the Portable ZIP, extract the complete archive into a writable folder and run CyInsightsApp.exe. Keep CyInsightsUpdater.exe, CyCaptureTool.exe, Documentation/ and cyinsights-portable.txt beside it. No installer, shortcut or file association is required.

## Portable data {#portable-data}

With cyinsights-portable.txt present, preferences, window layout, native-reader cache, logs and update transactions use the adjacent Data/ folder. Move the complete folder to keep those files. This edition does not read the installed edition's preferences. Registered Unreal paths/readers remain tied to the host engine installation; the browser and operating system can still keep their own temporary data and reading preferences. Original recordings remain separate from settings/index caches.

## Register Unreal {#register-unreal}
In Session, expand Sources / Native analyzer. Browse engine folder selects the engine root or its Engine directory; choose Add engine. Registration is saved. Prepare native readers builds the matching local adapters with that installation; it does not compile the entire engine. Engine headers/libraries and a compatible C++ toolchain must be present.

Engines are added explicitly, without an automatic disk-wide search. If a reader is missing/incompatible, register the correct engine and retry Analysis > Analyze native timing after preparation finishes. An export bridge remains available for supported providers. Engine binaries from Epic are not redistributed in this package.

Reader version support and plugin compatibility are different questions. An incomplete local SDK can block preparation even when the plugin supports that engine family. Preserve the original .utrace; a persistent index is a cache, not the recording. An antivirus may remove a successfully compiled executable, so verify the exact installed file/report if launch fails.

Next: [Unreal plugin](doc:unreal-plugin) or [Open a recording](doc:opening-traces).
