# CyInsights 0.2.1

## Integrated Quest GPU capture

CyInsights now collects detailed Meta hardware GPU stages alongside Unreal
traces and saves both in a single `.cycapture` archive. Completed captures open
automatically in the desktop app. The running game is preserved; restarting it
is an explicit option.

- Detailed GPU tracks: Binning, Render, LoadColor, LoadDepthStencil, StoreColor
  and StoreDepthStencil, when available on the device.
- CPU/GPU clock alignment using Perfetto clock snapshots and Unreal session
  synchronization anchors.
- Explicit reporting of measured detail, coarse events and unavailable data.
- Reboot detection prevents combining timestamps from different headset boots.

The complete capture and analysis workflow was validated on Quest 3. The final
five-second test capture contained 630,034 indexed events and 360 native frames.
See [Quest GPU validation and limitations](MetaQuestGpu.md).

## Application improvements

- Visible, cancellable waiting for the game's capture service during startup.
- Automatic opening of completed captures, with a manual-opening option.
- Diagnostics showing the GPU stage types actually received.
- Optional `.pftrace` import for GPU-only diagnostics, without invented CPU
  tracks or Unreal frame IDs.
- A separate Quest test project that can launch without controllers and avoids
  the persisted Vulkan cache loading that caused crashes on the tested driver.

## Getting started

Extract the complete Windows ZIP and run `CyInsightsApp.exe`. To analyze Unreal
traces, register the root folder of your licensed Unreal installation in
Session. Native readers are prepared using your local C++/CMake tools; Unreal
itself is not rebuilt. Unreal plugin sources are available in the separate ZIP.

Detailed Meta GPU capture requires an authorized ADB connection and GPU
profiling enabled before the game starts. Start recording from CyInsights;
a separate recording in Meta Quest Developer Hub is not required. Texture and
buffer readbacks remain disabled by default.

## Limitations

Hardware stages do not provide an exact mapping to Unreal passes, shaders,
objects or game frames. Unknown process identities and approximate correlations
remain explicitly labeled. Quest 3 validation does not cover every headset,
engine version or driver. See the [Quest capture guide](MetaQuestGpu.md).

Windows binaries are not Authenticode-signed. Component licenses, native reader
sources and SHA-256 checksums are included with the release.
