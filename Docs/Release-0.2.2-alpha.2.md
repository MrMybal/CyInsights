# CyInsights 0.2.2 Alpha 2

This is an **alpha prerelease** for development and testing.

## GitHub updates

The Windows application and CyInsightOVR now include a native update panel:
**Help > Updates** on Windows and **Updates** on Quest. Optional background checks
run at most once a day. Downloading and installing remain explicit actions.

Only newer stable releases are offered. Alpha, beta, release candidates and other
prereleases are ignored, including tags marked incorrectly as stable. Older
versions are never offered. **This alpha must be downloaded manually**; it is not
offered by the new updater.

Windows verifies the downloaded ZIP and its file manifest, prepares it in the
background, then uses the bundled native updater to back up replaced files and
restart the application with its open capture paths. Failed file replacements
roll back. Keep all files from the Windows archive together.

Quest keeps the installed standard/Vulkan variant and verifies the APK's size,
SHA-256, package, version code and signing certificate before opening Android's
installer. Android requires confirmation and may first ask permission to install
updates. Settings and captures are preserved. Updating does not restart a game.
The collector still needs authorized ADB bootstrapping after a headset reboot.

See [update behavior and publishing requirements](Updates.md).

## Downloads

- **Windows-x64.zip**: native desktop application, capture CLI, updater, MCP/SDK
  companion, offline Python dependencies and documentation.
- **Quest-Standard-arm64.apk**: standalone Quest metrics and configuration panel.
- **Quest-Vulkan-arm64.apk**: the panel with the experimental Vulkan layer for
  tile heatmaps and submitted draw/input geometry counters. Install one APK
  variant; both use the same Android package.
- **Quest-arm64.zip**: both APK variants, developer bridge, ADB bootstrap and guide.
  Run `Start-CyInsightOVR.ps1 -Install -Launch` for standard, or add
  `-Variant vulkan` for the experimental variant.
- **Unreal-Plugins-Source.zip**: integrated CyInsights Unreal plugin sources.
  Copy `Plugins/CyInsights` into your project's `Plugins` directory and build
  with your licensed engine. See [Unreal compatibility](UnrealCompatibility.md).
- **AI-SDK.zip**: standalone MCP/SDK companion, analysis method, offline Python
  dependencies and documentation.
- **SHA256SUMS.txt** and **release-manifest.json**: package checksums and versions.

## Validation and alpha limitations

The updater was tested with real GitHub metadata/downloads, an isolated Windows
replacement/restart with backup and settings preservation, and Android APK
validation on Quest 3. A future stable APK installation has not been tested
end to end because no compatible newer stable Quest package is published yet.

Existing Quest limitations remain: optional scene image recording can return
black images; tile alignment is approximate, with no exact mesh/eye attribution;
the experimental Vulkan heatmap needs a compatible development target. Draw
counters describe submitted work and known input triangles, not visible geometry
or completed GPU work. See the [Quest guide](../Quest/CyInsightOVR/README.md) and
[validation record](../Quest/CyInsightOVR/VALIDATION.md).

Windows executables are unsigned. APKs use development signing and are
intentionally debuggable for the authorized ADB bridge. Native `.utrace` readers
are built locally from source using a licensed Unreal installation and C++ tools;
Unreal binaries are not included. See [licensing notes](Licensing.md).
