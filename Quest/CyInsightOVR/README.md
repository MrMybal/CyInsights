# CyInsightOVR - standalone Quest GPU tools

Native Android arm64 application, C++ / Dear ImGui / CyUI. No Unreal installation,
web runtime or game modification is needed to open this app. The first version is
a **Quest panel** with system-counter curves and tile-stage snapshots. The
panel uses a TextureView so its GLES content is composed into the Android window.
A separate Android HUD and controls for Meta's installed VR HUD are included;
a custom universal VR compositor overlay is **not validated**.

## Build and install

`python Quest/CyInsightOVR/build.py` uses the installed Android SDK 34, build-tools
35.0.1, NDK 27.2 and Android Studio JBR. Override `--sdk`, `--java`, `--output` as
needed. No Gradle/network download is used. This produces a development APK signed
with a local development-only key in ignored `build/CyInsightOVR/`. Do not use that
key for a public production release. Android debuggability is explicit.

Install the standard or Vulkan APK, then open **CyInsightOVR** from the Quest
library. The app starts its own foreground **GPU service**. **Connection** provides
Start / Stop controls and its notification reopens configuration. Opening the app
after a headset reboot starts the service again; no PC script, shell helper,
wireless ADB pairing or other application is needed for runtime startup.

The service executes the firmware's installed profiler as the APK UID. The
ordinary GPU counters need no root or shell identity. Android's external-layer
settings and Meta HUD controls use the app's one-time `WRITE_SECURE_SETTINGS`
developer authorization. On a development installation, authorize once with the
optional installer from the extracted Quest bundle:

```powershell
.\Start-CyInsightOVR.ps1 -Device YOUR_ADB_SERIAL -Install -Launch
```

This permission persists across headset reboots and same-signed updates. It must
be granted again after uninstalling or revoking it. It controls system settings;
our app uses it for its selected GPU layer and Meta HUD configuration. The APK
cannot grant this development permission to itself. The installer performs
installation/authorization only: it no longer creates a shell-owned service or
copies a connection token. `-HudOnly -Launch` remains a compatible authorization
shortcut. Add `-Variant vulkan` for the APK with the optional external layer.

The GPU service's SDK endpoint binds only to **127.0.0.1**, on a fresh ephemeral
port, and authenticates requests with a random 256-bit capability stored mode
0600 in the APK's private files. Tokens never appear in command arguments or logs.
The supported operations remain an allowlist; arbitrary commands, paths and URLs
are rejected. The PC uses authorized ADB to call the installed APK's client under
its UID. It consumes the same on-device service and does not start a helper.

The Vulkan layer opens a private, bounded socket pair through an Android
ContentProvider URI granted only to the selected Development target. Android
Binder supplies the caller's real UID/PID; the host verifies both before accepting
counter data. The client releases an unstable provider reference after receiving
the descriptor, so closing/updating the profiling app does not terminate the
game. JNI attaches only a background worker to the target's existing JVM;
render callbacks perform no IPC, file I/O or waits. The grant also makes the layer
package visible on Android 11+, without changing the game's manifest or sharing
app files. A lightweight permission check restores this scoped grant if Android
revokes it when the game is force-stopped. Android loads the layer from
CyInsightOVR's own APK; no private target-library copy is required. Preparing a
target still requires reopening it once to create its GPU context with the
selected layers. Unsupported native/Java runtime access remains unavailable,
never a fabricated tile feed.

## Use

**Connection** shows the on-device GPU service separately from Meta HUD
settings authorization. Open the app after boot; if you explicitly stopped the
service, press **Start GPU service**. Start heatmap/capture actions also ensure
the app service is running. No reinstall or PC bootstrap is part of this flow.
Older development builds used a shell helper. Retire that legacy helper once
before testing the new runtime; rebooting the headset also removes it.

Open **Updates** to check GitHub for newer stable builds of the installed APK
variant. Alpha and other prereleases are ignored. The app verifies the download
and signing certificate before requesting Android installation confirmation.
See [updates and publishing requirements](../../Docs/Updates.md). The versioned
installer also accepts `-Variant vulkan -Install` for the experimental APK.

1. Open CyInsightOVR from the headset's app library / Unknown Sources.
2. Set the exact game package. Selecting CyInsightOVR itself for a background
   capture is rejected, because its configuration panel will close.
3. Overview selects up to six named metrics from the firmware catalog (never
   hard-coded IDs), rather than enabling every hardware counter. It shows
   measured **system** GPU counters. Negative sentinel values,
   missing and stale observations are unavailable, never silently zero.
4. To collect tiles, detailed profiling must have been enabled **before the
   target created its GPU context**. The explicit enable button requests this;
   close and reopen the target yourself. No game is force-stopped or restarted.
5. **Capture GPU tiles in 5 s** closes the panel and starts a foreground recorder.
   Return to the target game and keep it rendering. After the countdown, a 300 ms
   snapshot is captured without opening another window over the game. A notification
   shows progress; the scheduled capture can be cancelled before recording starts.
   The recorder saves the result even when the configuration Activity is destroyed.
   It does not launch, force-stop or restart the target game.
6. GPU heatmap displays measured tile geometry and selected stage duration.
   Gray is missing; zero is a valid measurement. Surface, tile and stage selection
   is native and shared with the desktop reader. The initial surface is the largest
   recorded surface, without inferring an eye or Unreal frame. Reopen CyInsightOVR
   to inspect the latest saved capture; USB and a separate capture program are not
   needed while CyInsightOVR's GPU service is running.

The app-owned GPU service continues when the panel is closed. Capturing while the full panel
has focus can suspend the immersive target, which is why recording now starts
after the panel closes. `Tools/OvrBridge.py` remains an optional developer client;
it is not needed for the normal in-app recording workflow. Desktop tile export
is served by the APK itself through the authenticated service, streamed in
`CYCAP001` format, and checked by CyInsights before extraction. An export worker
keeps normal SDK/UI requests responsive; no shell access to scoped storage is
required.

### VR HUD and configuration panel

The full-size CyInsightOVR window is a **configuration and analysis panel**. Open
**Display & HUD**, choose the size and one of the nine positions, then press
**Start HUD** in the fixed bottom bar. Positions include **Top center**, **Center**
and **Bottom center**, in addition to the corners and side centers:

- Starting or applying the HUD keeps the configuration panel open. Return to
  the game through the Quest menu when ready; the native stop/configuration
  notification remains available.
- FPS and app GPU time are the default graphs. Choose graphs and numerical values
  independently from the documented metrics detected on this headset. The limits
  are six graphs and eight values; unavailable metrics are disabled. Settings are
  saved automatically and restored when the panel is reopened. Selections outside
  our documented list remain untouched in Meta; enabling values may also show
  those existing statistics.
- Meta can list a hardware metric while refusing to enable it at the moment.
  Start applies the usable selections and shows an explicit warning for each
  missing graph/value. Saved choices are kept and retried on the next Start;
  unavailable counters are never displayed as measured zero. A selection with
  no usable metrics fails and restores the prior HUD configuration.
- The metric list has Frame & GPU, CPU, Memory and Device filters. Its managed
  catalog includes all 25 numerical metrics initially enabled on the tested Quest,
  so those pre-existing selections can be disabled while applying the user's choices.
- The installed Meta VR HUD renderer shows the chosen metrics. This is
  explicitly a **Meta-rendered mode**, not our custom Android HUD or heatmap.
- Default placement is head-locked, scale 1, distance 2, pitch +8 and yaw -12.
  Nine positions (including top center, center and bottom center) and Medium size
  can be selected in our panel. The tested service
  clamps a requested scale of 0.3 back to 1; smaller physical sizes are not claimed.
- With no numerical values selected, the tall stats table is hidden **after** placing/enabling the overlay,
  because enabling the overlay may reapply saved display settings.
- Original table visibility, placement, graph visibility and managed metric
  selections are saved before changes, and restored by **Stop HUD**. The Meta HUD keeps this snapshot in private app storage and it survives the app process restarting. A failed activation attempts rollback and keeps diagnostics explicit.
- Change options in the still-open panel, or reopen CyInsightOVR later. The notification also has
  Stop. The CLI can use `Tools/OvrBridge.py ... overlay_meta_off`.

Standalone mode reads the installed Meta settings provider directly as the APK
and verifies applied settings without a shell process or collector connection.
The legacy diagnostic backend reads Meta's LOG_STATE output, including chunked
JSON. Accepted requests/settings are separate from
physical headset visibility. The user confirmed the Meta FPS graph was visible and then confirmed the final
HUD-only size was suitable after the configuration panel/table were removed.
FPS and GPU graph enablement is verified from the service configuration;
immersive-game appearance and data still need a dedicated check.

On 2026-11-02, real Quest 3 checks ran under the APK UID with the developer
collector stopped: 30 HUD metrics were recognized, Start HUD applied and verified
the basic graphs/placement and the saved user selection, and Stop HUD restored
the original settings. Two saved hardware graph selections were refused by Meta
and reported explicitly; they no longer prevented the usable HUD from starting.
This validates direct HUD control, not autonomous detailed tile collection or
physical visibility. The persistent authorization was granted; a complete
headset-reboot check remains outstanding. See `VALIDATION.md`.

### Development Vulkan heatmap overlay

The separate development APK built with `--experimental-vulkan-layer` includes
our optional `VK_LAYER_CYINSIGHTS_heatmap`. The standard APK does not include it.
**This path is experimental: a measured feed and briefly visible HUD are confirmed,
but stable HUD/fullscreen validation is still in progress.** It is not a universal compositor overlay.
Android permits external layers only for debuggable or explicitly opted-in targets.

The **Heatmap overlay** page provides two independent presentations:

- **Extra HUD** with nine positions, separate from the Meta statistics HUD.
- **Fullscreen (approximate)** with its own opacity and Render-cost color ceiling.

Default tile opacity is 25% for the small HUD and 20% for fullscreen. The
background follows that slider at one fifth of its opacity instead of remaining
opaque. A Transparent preset restores these values; custom saved choices are kept.

Choose the exact Development Vulkan package, prepare its layer, then completely
close and reopen the target yourself. Starting the heatmap never launches or
restarts a game and keeps the configuration panel open. Return to the game through
the Quest menu. Apply switches modes and positions without restarting the target.
Stop releases the live sampler and waits for its worker to finish, retaining
the prepared target so Start can resume without a game restart. Repeated Start
on the same active target applies settings. Start/Apply use the prepared heatmap
target even if the saved snapshot target differs.

The Advanced setup action **Uninstall target integration** confirms removal and
restores the previous four Android GPU layer settings if another tool has not changed them. Existing unknown layers are
never overwritten. Original settings and owned-library SHA-256 are journaled in
private app storage; failed rollback retains recovery metadata.

Preparation also requests detailed profiling and retains the installed, enabled
Meta/Adreno profiler in the explicit Vulkan layer chain. Selecting only our layer
can suppress Meta profiling; accepted setup remains separate from measured output.

The app service owns one continuous profiler process, accepts complete target
sections through a bounded latest-only handoff, and publishes only fresh measured
Render costs with tile geometry to the selected target over its authenticated local socket. Parsing and file polling run off
the render thread. It does not replay a saved capture as live data and does not
save every live update to disk. Missing measurements produce an explicit diagnostic.
A loaded layer or accepted start request alone does not prove a visible overlay.

The current renderer supports inline render-pass subpasses with one color
attachment and tracked graphics dynamic state. It selects a candidate by surface
dimensions; this does not establish eye, swapchain-image or frame identity.
Measurements are sampled and delayed, not costs for the currently displayed frame.
The input expires after three seconds for newly recorded command buffers; an
application reusing cached command buffers may retain a previously recorded HUD
until it re-records them. Dynamic rendering and universal APK support are not claimed.

The optional authenticated SDK exposes `heatmap_prepare`, `heatmap_start`,
`heatmap_update`, `heatmap_stop` and `heatmap_remove`; `snapshot.live_tiles` reports
source age, tile count, achieved update rate, mode, preparation and current-process
readiness. Public Android external-layer documentation:
[Android Vulkan layers](https://developer.android.com/ndk/guides/graphics/validation-layer).

### Experimental Android HUD

**Connection > Test Android HUD** starts our own native Android
window with sampled counter graphs and the last tile snapshot. Android's special
overlay permission is required. Its window attaches, but the user did not see it
in the Quest. It is no longer presented as the main VR HUD path. A custom
cross-game VR compositor overlay and live image-aligned heatmap remain unvalidated.

**Check this headset's XR overlay support** queries the actual runtime from the
Connection page. The bundled Khronos OpenXR loader is open source;
no Unreal installation or proprietary Meta runtime is bundled. This inquiry creates
no XR instance or session. A failed query is unknown, not proof of unsupported
hardware. On the tested Quest 3, enumeration succeeded with 72 extensions and did
not expose `XR_EXTX_overlay` to this APK.

A separate shell SurfaceControl experiment created only two small transparent
panels. The user confirmed they were not visible in VR, and automatic cleanup
removed them. The physical display was excluded from standard Android composition;
changing the display routing to force composition is not part of this product.
The optional public Meta SDK experiment accepted custom strings, but that alone
does not establish a custom cross-game HUD. It is not a shipped HUD backend.
In the final test with the GPU project visible and the headset worn, the user
reported no visible HUD despite 90 accepted SDK updates. That test failed.

Installation and normal app opening do not enable either HUD automatically.
A visible control panel is not evidence that an overlay is shown over a VR game.

### Panel navigation and app library

The header, tabs and Scroll up / Scroll down buttons stay visible while the page
content scrolls. Pointer drags scroll without activating the control underneath;
Android scroll-axis events, forwarded controller stick axes and page/directional
keys are routed to native scrolling. The footer buttons provide a fallback when
the Quest host does not forward the controller axes. **Display & HUD > Panel graphs**
selects the sampled system curves shown in Overview.

The APK has an Android MAIN/LAUNCHER entry named **CyInsightOVR**. As a sideloaded
development app it is opened through **Library > Unknown Sources**, not the Store
library. The installer's `-Launch` option opens the same configuration Activity.
Installing this APK does not publish it to Meta or change the Quest library policy.

**Quick access** in the header requests an icon through Android's public launcher
shortcut API when supported. Confirm the addition in the Meta launcher dialog.
This shortcut opens **Display & HUD** directly, including when a saved GPU capture
is loaded in the background. The launcher chooses where to put it; an accepted
request is not treated as a confirmed pinned shortcut. The non-exported callback
and the launcher's pinned-shortcut list provide separate confirmation.

The panel follows the host display rate up to 90 Hz while visible, and stops
rendering when paused. Its schedule includes render/presentation time. Snapshot
JSON and capture models are prepared on the worker, with a bounded handoff to GL;
unchanged complete captures are omitted from subsequent panel polling responses.
Connection shows measured panel FPS, CPU draw time and presentation wait.

## Exchange with CyInsights

The integrated tile-recording options, associated Quest view sharing, timeline
replay and current validation limits are described in
[Quest tile recording](../../Docs/QuestTileRecording.md). Images are opt-in and
require the system screen-sharing consent; their association is approximate.

Each finished in-app snapshot is saved atomically by the background recorder to
`/sdcard/Android/data/org.cyinsights.ovr/files/Captures/*.cyovr`, including diagnostic
captures with unavailable data. The request ID must match the completed capture;
an older result is never accepted as completion of a new request. The panel also
saves snapshots made by the optional developer client when it polls them.
Export supports Android's document picker. Retrieve saved snapshots:

```powershell
.\Start-CyInsightOVR.ps1 -Device YOUR_ADB_SERIAL -PullCaptures
```

Open the `.cyovr` file in CyInsights with Open captures / Ctrl+O. It opens in its
own detachable document and the shared **Quest GPU heatmap** view. There is no
CSV conversion or synthetic Unreal frame. The schema is
`cyinsightovr.capture`, version 1; durations are microseconds. Surface occurrences
are separate even when their reported source index repeats. The requested target
must match the source process label; no attribution is inferred from dimensions.

CLI durations are measured, but the source does not provide an execution clock
anchor here. `observed_mono_ns` is **receipt/observation**, not GPU execution.
`time_basis=capture_relative_unanchored`, unknown frame and eye identity remain
explicit. These values do not establish alignment with the currently displayed
eye image or Unreal timeline. The experimental development overlay labels its
presentation as sampled and approximate; exact alignment needs independent evidence.

`Bin` is an envelope, not an extra additive render stage. Render includes vertex
and fragment operations. A hot tile does not identify a unique shader, mesh or
actor. Detailed GPU profiling changes scheduling and adds overhead; short
diagnostic snapshots are not the baseline for final performance measurements.

## Draw calls, input triangles and faster curves

The **Overview** page includes **Draw calls & input triangles** controls. Select
the development game's Android package, press **Prepare draw counter layer**, and
completely close/reopen that target once. Then press **Start draw counters**.
Preparation does not enable Meta detailed GPU profiling and never restarts a game.
It requires the experimental Vulkan APK and a debuggable Vulkan target.

In **Display & HUD**, **Curve updates** selects **1, 5, 10 or 20 Hz** (10 Hz by
default). Enable the known submitted draws, input triangles and indirect-command
curves there. The measured source cadence is displayed separately. Meta's system
counters retain their own cadence, normally around 1 Hz. Ordinary panel polling
stays at 1 Hz while draw collection is stopped. Slow readiness checks are cached;
high-rate collection uses the persistent peer-authenticated layer socket instead
of target-UID readers or shell/cat launches. Stopping the service or counter
request releases the native export lease. Source samples are not interpolated or duplicated.

Draws are counted when tracked command buffers are successfully submitted,
including reused command buffers, repeated secondary executions and instances.
The layer's own heatmap draws are excluded. The native recorder keeps lightweight
command metadata while the optional layer is loaded. Its IPC export runs only
while the app requests counters, on a worker, at the selected rate.
No GPU query, buffer readback, texture capture or pipeline wait is added for counts.
Actual game overhead has not yet been benchmarked.

Input triangles are before culling, clipping, shader-generated geometry and view
expansion. Fixed-count indirect draws retain their known draw count, but their
triangle counts remain unknown. GPU-controlled draw counts, primitive restart,
unknown topology and missing command buffers mark totals as incomplete. The UI
labels known totals explicitly; a missing value is not displayed as zero.

Per-presented-frame means are available only when actual `vkQueuePresentKHR`
boundaries are observed on one device. **An OpenXR Quest submission is not assumed
to be a frame.** When the XR runtime provides no Vulkan presentation boundary,
these per-frame curves remain unavailable; measured submission-window counts and
draws/triangles per second still work. These are process-submission observations,
not proof of visible triangles, GPU execution timing, per-eye or mesh attribution.
This feature adds panel graphs; Meta's HUD metric catalog is unchanged.

The same operations are available through `Tools/OvrBridge.py`:
`draw_counters_prepare --package org.example.game`,
`draw_counters_start --package org.example.game --hz 20`, and
`draw_counters_stop`. Authenticated `snapshot` returns `draw_counters` and the
derived metrics. History has a restart-scoped sequence cursor so mixed source
cadences do not drop or duplicate observations.

## Current boundaries

- No validated custom universal cross-game VR overlay or image-aligned live heatmap.
- No automatic target restart, per-eye assumption or world-space cell mapping.
- System counter history is sampled, not measured per Unreal frame.
- No copy of textures, buffers, shader binaries, credentials or app data.
- Installation does not modify or launch other games.
- Firmware-dependent profiler capabilities and actual measured data are separate.

On 2026-10-28 the native button -> background capture -> atomic save -> native
heatmap reader path passed on Quest 3 using our dedicated GPU test project:
capture `39c5f223-b190-4ffc-b266-e8b71974f09b`, 1,401 measured tiles. The same
format also opens in the desktop reader. This validates real recorded tile data,
not a live image-aligned or universal custom VR overlay. See `VALIDATION.md`.

Sources: [Meta profiler](https://developers.meta.com/vr/documentation/native/android/ts-ovrgpuprofiler/),
[tile stages](https://developers.meta.com/vr/documentation/unreal/ts-renderdoc-renderstage/).
For repeatable checks, run `ParserTests` through the packaged Java bridge. The
own-app `SmokeTest` instrumentation checks the real app's access to the app-owned GPU service
and copies its Android window. `-e capture_flow true` additionally presses the
native recording button and opens only `org.cyinsights.gputest`, our dedicated
test project, to verify measured capture, persistence and native heatmap loading.
It returns from that test to Home before inspecting the panel. Android terminates
the instrumented app after the test; launch the app normally afterwards. This test
does **not** establish physical visibility of a custom HUD over a VR game.

Application code is AGPL-3.0-or-later; bundled component notices remain in
`LICENCES/` and in the APK assets. No Unreal or Meta proprietary binaries are
redistributed; the device's existing profiler is invoked under developer rights.
Khronos OpenXR 1.1.65 headers and Android loader are bundled with their Apache-2.0
license and source/archive hashes in `ThirdParty/OpenXR/ORIGIN.json`.
