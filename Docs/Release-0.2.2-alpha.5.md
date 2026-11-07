# CyInsights 0.2.2 Alpha 5

**Windows x64 and Meta Quest arm64 · ALPHA**

## Install the complete MCP/SDK from the application

Open **AI Inspector > Connection > Install MCP/SDK** on Windows. CyInsights
downloads private Python 3.11 x64 from a pinned Astral release, verifies its
SHA-256, and installs the CyToolsCore/MCP dependencies already shipped with the
application. Package installation is offline and requires matching wheel hashes.
No global Python installation or administrator access is required.

The panel shows progress, cancellation, retry/repair and a copyable installation
log. The runtime becomes ready only after checking its imports, pinned dependency
versions and the complete 39-operation contract. Enable the local bridge, then
copy the verified MCP configuration into your external client. Application control
and remote capture permissions remain separate and disabled initially.

Installed builds use their local application data; portable builds use
`Data/AI/Runtime` beside the application. Failed or cancelled preparation does not
replace an existing verified installation. A separate SDK ZIP is available for
advanced use; it is not an extra requirement for the Windows app. No AI model is
downloaded or included.

## Downloads

- **Windows-x64-Setup.exe:** per-user Windows installer, Start menu shortcuts,
  uninstaller and complete English/French offline manual.
- **Windows-x64-Portable.zip:** extract and launch; settings and the private SDK
  runtime stay with the portable folder.
- **Windows-x64-Update.zip:** the complete payload used by the application updater;
  built from the same source as Setup and Portable. Windows also recognizes legacy
  update asset names. This is not an older application build.
- **Quest Standard/Vulkan APKs and bundle:** rebuilt with version 0.2.2-alpha.5
  and Android version code 6; install one variant.
- **Unreal plugin sources and optional AI SDK sources/dependencies.**

All assets belong to this tag. See `SHA256SUMS.txt` and `release-manifest.json`
for the matching checksums and source commit.

## Alpha limits

This remains an experimental alpha. The automatic application updater continues
to select stable releases only; install this alpha manually. Windows binaries
are unsigned. The first private Python download needs Internet access; subsequent
preparations can reuse the verified cached archive. Windows `tar.exe` is required.
The manual and native analysis tools continue to work offline without the SDK.

No new Quest GPU capabilities are introduced in this release. Detailed counters
and tile overlays retain their documented firmware/Development-target limits.
The updated APKs have not been retested on a headset for this desktop change.

[English manual](https://mrmybal.github.io/CyInsights/Documentation.html#/en/ai) ·
[Français](Release-0.2.2-alpha.5.fr.md) · [README](../README.md)
