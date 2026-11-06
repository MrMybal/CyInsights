# CyInsights 0.2.2 Alpha 4

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

The same reader is available at [CyInsights documentation](https://mrmybal.github.io/CyInsights/).

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

- Windows x64 application ZIP.
- Standalone Quest Standard and Vulkan APKs; install one variant.
- Quest bundle with both APKs and optional developer-authorization installer.
- Unreal plugin sources and standalone MCP/AI SDK bundle.

This remains an **alpha prerelease**. The application's updater continues to offer
only newer stable releases; install this alpha manually. Native `.utrace` readers
are prepared locally using a compatible licensed Unreal installation and C++
toolchain. No Unreal Engine binaries are included.
