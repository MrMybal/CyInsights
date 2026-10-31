# GitHub updates

The Windows application and CyInsightOVR check the public releases of
`MrMybal/CyInsights`. Open **Help > Updates** on Windows or **Updates** in the
Quest panel. Automatic checks run at most once a day and can be disabled there.
Checks and downloads run independently of analysis, rendering and metric polling.
Updates are installed only after the user selects the install action.

## Stable releases only

Drafts and all prereleases are excluded. A tag containing `-alpha`, `-beta`,
`-rc` or another prerelease suffix is rejected even if its GitHub prerelease flag
was accidentally cleared. A newer stable version is required; older releases
are not offered. A stable final release can replace an installed alpha with the
same numeric version. For example, `0.2.2` can update `0.2.2-alpha.1`, while
`0.2.1` cannot.

The `0.2.2-alpha` builds are therefore intentionally not offered
by this updater. Installing the updater for the first time remains a manual
installation. This feature does not convert existing alpha releases into stable
ones or publish a stable release automatically.

## Download and installation

Metadata comes from GitHub's HTTPS API. Product names and asset URLs are pinned
to this repository and the chosen tag. Redirects are limited to GitHub's HTTPS
download hosts. Asset size and GitHub's SHA-256 digest are verified before use.
Invalid downloads are not installed. Rate limits, missing packages, unavailable
network and cancellation are reported in the update panel.

Windows downloads and prepares the complete ZIP in local application data.
Archive paths, duplicate names, symlinks and extraction bounds are checked before
extraction through Windows' `tar.exe`. Every extracted file must match the
package manifest. The bundled native `CyInsightsUpdater.exe` waits for the current
process to close, verifies the prepared package again, backs up replaced files,
and restarts CyInsights with its open capture paths. A failed file replacement
rolls back replacements already made. Unlisted files, settings and captures are
not removed. The application folder must be writable; other instances holding
its executables open must be closed. A missing Windows `tar.exe` requires manual
extraction instead. Backups and diagnostics remain in the update transaction
directory under local application data.

Quest keeps the installed standard/Vulkan variant. Before opening Android's
installer it checks the package identity, increasing Android version code,
release version, arm64 native libraries, variant and the same signing certificate
as the installed APK. Android still verifies the APK and requires installation
confirmation. The user may first need to permit CyInsightOVR to install updates.
If Quest firmware does not expose the installer or its settings window, the panel
reports that authorized ADB installation is needed. Installation does not erase
application data or restart a game. Advanced collector bootstrapping after a
headset reboot remains a separate requirement.

## Publishing compatible packages

1. Set the version in root `VERSION.json`. Increment `android_version_code` for
   each distributed Android build. Both Windows resource versions and Android
   build metadata are derived from this file.
2. Build Windows with `build.cmd Release`; keep `CyInsightsUpdater.exe` alongside
   `CyInsightsApp.exe` and `CyCaptureTool.exe`. Package all three with
   `Tools/package_release.py` and the matching tag as `--version`.
3. Build Quest with `Quest/CyInsightOVR/build.py` and optionally
   `--experimental-vulkan-layer`. The versioned filenames already match the
   updater. The bootstrap reads `cyinsightovr-build.json`; use
   `-Variant vulkan -Install` to install the experimental variant.
4. Publish the files as assets of the matching GitHub tag. Stable tags must be
   `vMAJOR.MINOR.PATCH` or `MAJOR.MINOR.PATCH`, with `draft=false` and
   `prerelease=false`. Alpha builds must remain marked as prereleases and are
   never offered by the updater.

Expected stable asset names (example version):

- `CyInsights-v1.2.3-Windows-x64.zip`
- `CyInsights-v1.2.3-Quest-Standard-arm64.apk`
- `CyInsights-v1.2.3-Quest-Vulkan-arm64.apk`

Keep the original Android signing key private and backed up. Changing it prevents
an in-place update of the existing package; uninstalling would lose application
data. Never place that key in Git or a release. This updater does not supply an
Authenticode certificate or an antivirus verdict for Windows binaries.
