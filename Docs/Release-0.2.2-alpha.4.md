# CyInsights 0.2.2 Alpha 4

[Back to README](../README.md) · [Français](Release-0.2.2-alpha.4.fr.md)

**Desktop and Quest version: 0.2.2-alpha.4 · Stage: ALPHA**<br>
**Available builds: Windows x64 and Android/Quest arm64**<br>
**Bundled Unreal plugin descriptor: 0.2.1 (sources)**

An experimental prerelease for testing and feedback. The plugin's descriptor
version is independent of the application/package version.

## Windows installer and portable edition

The [Setup EXE](https://github.com/MrMybal/CyInsights/releases/download/v0.2.2-alpha.4/CyInsights-v0.2.2-alpha.4-Windows-x64-Setup.exe)
installs CyInsights for the current user and adds Start menu shortcuts and an
uninstaller. The [Portable ZIP](https://github.com/MrMybal/CyInsights/releases/download/v0.2.2-alpha.4/CyInsights-v0.2.2-alpha.4-Windows-x64-Portable.zip)
keeps settings, layout, caches, logs and update files in its adjacent `Data/`
folder. Both include the complete English/French offline manual.

These Windows editions are an additional build of this alpha. Their exact source
commit and hashes are recorded in `windows-editions-manifest.json` on the release;
use [SHA256SUMS-Windows-Editions.txt](https://github.com/MrMybal/CyInsights/releases/download/v0.2.2-alpha.4/SHA256SUMS-Windows-Editions.txt)
for these files. The original release assets retain their original checksums.
The installer and application are unsigned. Alpha updates remain manual.

## Integrated English and French documentation

The Windows application and CyInsightOVR now include the current CyDoc reader.
Open **Help > User manual** or **F1** on Windows, or **Help** on Quest. The manual
contains complete English and French guides, task-based navigation, global header
search, quick title/description tooltips, screenshots and explanatory diagrams.
Images enlarge inside the reader window; Escape reduces them.

The manual opens directly from `Documentation/Documentation.html`. Ship the
complete `Documentation` directory. Scripts, styles, media and license notices
are included in `DocData`; reading requires no server or connection. Assistant
guidance is kept separately in `DocAI.md`. Public API references describe the
39 supported operations, parameters, results and permissions without source
implementation listings. Historical development records are outside the reader.

The same reader is available at [CyInsights documentation](https://mrmybal.github.io/CyInsights/Documentation.html#/en/welcome).

## Quest HUD controls

HUD graphs, numerical HUD values and configuration-window graphs have independent
selections. Draw-call and triangle rate counters can be selected without clearing
other metrics. Native Meta values and custom development-game metrics retain
their distinct availability and target requirements. Unsupported, stale or
missing values remain unavailable rather than becoming zero.

Custom Vulkan counters and the tile overlay require the Vulkan APK, a supported
debuggable target and its prepared layer. A complete manual target reopen may be
needed after changing that layer. This alpha does not provide a universal custom
overlay for unmodified commercial games.

## Downloads

- Windows x64 Setup EXE and Portable ZIP; the original application ZIP is also retained.
- Standalone Quest Standard and Vulkan APKs; install one variant.
- Quest bundle with both APKs and optional developer-authorization installer.
- Unreal plugin sources and standalone MCP/AI SDK bundle.

This remains an **alpha prerelease**. The application's updater continues to offer
only newer stable releases; install this alpha manually. Native `.utrace` readers
are prepared locally using a compatible licensed Unreal installation and C++
toolchain. No Unreal Engine binaries are included.

[Download and published release notes](https://github.com/MrMybal/CyInsights/releases/tag/v0.2.2-alpha.4) · [SHA-256 checksums](https://github.com/MrMybal/CyInsights/releases/download/v0.2.2-alpha.4/SHA256SUMS.txt)

## Known issues

The published notes do not provide an itemized confirmed-bug list. Consult the
[issue tracker](https://github.com/MrMybal/CyInsights/issues) for reports and
updates. The documented limits below do not imply an absence of other bugs.

## Documented limitations

- The desktop package targets Windows x64; no Linux/macOS application package is
  supplied. Native `.utrace` preparation needs a compatible licensed UE 5.5–5.8
  and C++/CMake tooling. It compiles the adapters locally, not Unreal itself.
- Unreal plugin sources target UE 4.27 and 5.0–5.8, with version-specific
  validation gaps. Shipping, Test and Server are excluded. See the
  [compatibility matrix](UnrealCompatibility.md).
- Incoming traces are analyzed after recording stops. Native game-log reading,
  memory callstack exploration and several Unreal Insights providers are incomplete.
- Quest system metrics are observations, not guaranteed identities of an individual
  GPU execution frame or eye. Detailed sources depend on firmware and target setup;
  missing or stale values remain unavailable.
- The Vulkan heatmap and custom draw/triangle counters require a supported,
  prepared debuggable target. They are not a universal custom overlay for arbitrary
  commercial APKs. Tile/image association can be approximate; see the
  [tile recording limits](QuestTileRecording.md).
- Windows command/resource detail requires the optional collector and separately
  configured ReShade runtime. Requesting a detailed GPU snapshot remotely does
  not currently transfer that Windows payload to the receiver. Image/buffer
  readbacks remain a separate opt-in.
- AI tools expose the implemented providers and permissions; they do not add
  native live analysis or missing GPU timestamps. See the
  [operation coverage](CapabilityCoverage.md).

## References

- [Manual](https://mrmybal.github.io/CyInsights/Documentation.html#/en/welcome) · [Website](https://mrmybal.github.io/CyInsights/).
- [All releases](https://github.com/MrMybal/CyInsights/releases) · [Report an issue](https://github.com/MrMybal/CyInsights/issues/new).
