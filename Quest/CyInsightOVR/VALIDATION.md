# Tile/image recording revision, 2026-10-29

ExternalGame is an anonymized label for an unrelated development APK tested with
the user's permission.

- Two real six-second desktop-app captures completed, packed native Unreal data,
  detailed Meta stages, six tile snapshots and six image files each, and opened
  automatically. One indexed 685,068 native events / 371 frames with a real
  session/boot observation anchor. The second contained 145,738 hardware stages,
  including 82,571 detailed stages. Pixel inspection still found black game
  images; a native display probe received visible system UI instead. Concurrent
  Other Quest development tests repeatedly took foreground focus. Game-view visibility
  therefore remains unvalidated and needs an exclusive headset interval.
- The first integrated test exposed a client error: recording status includes
  `error: ""`, which was mistaken for a bridge refusal. The native and Python
  clients now distinguish a single-field dispatch error from recording status,
  preserving the recording ID for stop/cleanup. Regression checks cover both.
- Optional native ADB images now use bounded row sampling from Meta's installed
  capture command, capped at 1280x720 and a three-second child-process limit.
  Foreground target checks bracket acquisition; blank frames carry an explicit
  visibility diagnostic. MediaProjection remains a consented fallback.

- Optional bounded tile recording, consented MediaProjection view sharing,
  `.cycapture` transfer and native desktop replay are implemented. The PC reader
  and matching UE 5.5 Oculus clock reader compile; five affected native test
  suites and twelve SDK tests pass. Android fixture checks cover parser (12),
  stream (7), target selection (7), HUD lifecycle/recovery (23) and recording (17).
- The normally launched configuration panel passed actual input/scroll checks:
  89.96 FPS, 0.55 ms draw, 12.08 ms p95 frame interval at a 90 Hz target.
- The new clock export passed against an existing real Quest `.utrace` using
  its recorded session/frequency/boot metadata; a mismatched-session control
  produced no anchor. This is not a combined tile/image hardware validation.
- The user accepted screen sharing; an actual 1280x720 JPEG was received. The
  initial image was black while no game scene was open. This does not validate
  an associated game image.
- Game-image visibility validation remains incomplete. An earlier ExternalGame run
  reported Vulkan unavailable, fell back to
  OpenGL, and blocked in `MessageBox01.show`; Android recorded an ANR. With our
  collector stopped, forced layers removed and Meta detailed profiling disabled,
  a native Vulkan 1.1 startup probe under ExternalGame's UID successfully creates an
  instance and enumerates Adreno 740 (Vulkan 1.3.295). The temporary probe was
  removed. This does not establish that ExternalGame's Activity starts successfully:
  current controlled launch attempts stop in Meta's controllers-required and
  play-area dialogs, with no ExternalGame PID and no new game log. The old ANR log
  must not be interpreted as a fresh failure in this baseline configuration.
- ExternalGame subsequently started normally with Vulkan and about 72 FPS. Its
  current APK did not list CyInsights in startup plugin logs or listen on the
  CyInsights control port. Real combined capture tests used our dedicated GPU
  project. Its later pauses/removed render windows coincided with another Quest tool
  taking foreground focus; an old APK's earlier startup SIGSEGV is separate.
- Missing owned layer files are now safe to remove/recover: original GPU debug
  settings are restored, another tool's changes are preserved, and an absent
  file is not reported as a successful prepared integration on the next setup.
- Detailed workflow and evidence limits: [Quest tile recording](../../Docs/QuestTileRecording.md).

The dated results below describe earlier iterations and their separate scopes.


# CyInsightOVR development validation - 2026-10-28

## Update - 2026-10-29

- Starting the selected HUD now retains the configuration Activity. A real
  normal-window button check passed and reported `configuration_stayed_open=true`.
  The test stopped its HUD and restored the previous Meta settings afterwards.
- Nine positions are supported. Sixteen simulated-service checks pass, including
  top center, center and bottom center. A separate real Meta-service check accepted
  center placement and verified its settings through fresh LOG_STATE output.
- Frame scheduling no longer adds a complete interval after rendering/swap. It
  follows the panel display up to 90 Hz. JSON/capture preparation runs on the
  worker and unchanged capture payloads are not resent in panel polling.
- The normal-window interaction test measured 71.96 FPS against a 72 Hz target,
  0.59 ms average CPU draw, 0.31 ms presentation wait and 14.39 ms p95 interval.
  Snapshot preparation was 0.17 ms on that sample. These are short panel-navigation
  measurements, not a universal sustained-performance benchmark.
- The final installed standard APK repeated the normal-window check successfully:
  drag, key scrolling, Android wheel, rendered checkbox and preference persistence,
  HUD start without closing configuration, and PixelCopy success. It used the
  user's saved top-center preset. This sample measured 71.97 FPS, 0.53 ms CPU draw,
  0.30 ms presentation wait, 14.83 ms p95 interval and 0.19 ms snapshot preparation.
- Launcher probing on this Quest reported pinning supported and resolved our
  configuration intent. Pin creation/placement still needs the actual launcher
  confirmation; support/request acceptance alone is not a pinned-icon pass.
- An explicitly opt-in Vulkan discovery layer builds in a separate development
  APK. The default APK does not contain it. That initial discovery build did not render a heatmap; see the later integrated test below. Android
  external-layer eligibility is limited to debuggable/opted-in targets; it is not
  a demonstrated route over all unmodified commercial games.
- The October 29 external-layer attempt did not reach Vulkan dispatch. Android
  reported that the layer APK was not visible to the target. The documented
  target-local method was also attempted, but the test project did not create
  its render window. Its exit report confirmed an ANR; the GameThread was waiting
  in Unreal's MessageBox01 while the main thread waited in surface destruction.
  A reference launch with the original GPU settings also stalled. The Unreal
  startup log reported Vulkan unavailable. These failed runs do not validate the
  layer or establish the underlying cause of the Vulkan failure. All four GPU
  debug settings were restored and the temporary target-local library removed.
  Only our blocked test project was stopped; the standard companion APK was
  reinstalled afterwards.

Initial tests: Quest 3, Android API 34. Only org.cyinsights.ovr was updated or
instrumented; org.cyinsights.gputest was opened for collection. Later ExternalGame
external-layer tests are described separately below and were explicitly authorized.

## Integrated development tile HUD - 2026-10-29

- With the user's explicit authorization, ExternalGame was used after the
  dedicated test project failed to open. No ExternalGame source or engine was changed,
  rebuilt, launched or stopped by the tooling; the user performed game restarts.
- The documented target-local Android external-layer method reached real Vulkan
  dispatch on ExternalGame: the library appeared in its maps, created a device, and
  observed inline render areas including 2016x1760. This supersedes the earlier
  failed discovery attempts above. Loading does not prove rendering visibility.
- Native app and authenticated SDK now include explicit layer preparation,
  independent positionable tile HUD/fullscreen controls, live sampling, stop and
  removal/restoration. The experimental APK compiles and v3 signature verification
  passes; the standard APK continues to omit the layer.
- A previous real ExternalGame snapshot contained 946 tiles on 44 surfaces (capture
  d2269055-53f7-4bfe-96a2-f236e1eb7002). It is historical captured data, not proof
  that the current live feed works.
- The integrated live attempts received zero target tiles, including after the
  user fully restarted ExternalGame. Android showed ExternalGame resumed and awake, with
  fresh VrApi output around 72 FPS, but the detailed profiler returned system
  compositor surfaces only. A bounded exclusive capture, with our helper and
  its identified orphan counter reader stopped, still returned no ExternalGame data.
  Therefore concurrent readers alone do not explain this failure.
- MQDH GPU Profiling off/on had previously recovered the unresponsive profiler's
  status query. A successful status query does not prove target-context attachment.
- The live feed was stopped and the owned development layer removed; the original
  four Android GPU debug settings were restored for a comparison without the layer.
- Twelve parser checks and sixteen development-layer lifecycle checks passed on
  Android using explicit synthetic fixtures. They cover source attribution,
  missing-target diagnostics, partial data, rollback recovery, ownership,
  changed settings/library preservation, mode changes and stop/release. They do
  not establish GPU hardware output or physical HUD visibility.
- Command-pool destruction/reset now clears tracked command-buffer state. Feed
  publication is serialized with stop. A failed preparation rollback preserves
  its journal instead of discarding the original settings.
- The no-layer comparison succeeded: capture 64ad5b2e-6b71-4f57-9357-ac161ddd0379
  received twelve ExternalGame surfaces. Its process maps showed the system
  libVkLayer_ADRENO_qprofiler.so. The explicit development chain had requested
  only CyInsightOVR. Preparation now preserves the enabled installed profiler
  by requesting both layer names and journals the exact chain for restoration.
  The installed setup reported profiler_layer_preserved=true. After a manual
  target restart, the integrated SDK received 46 measured tiles at 2016x1760;
  Vulkan recorded HUD commands in subpass 2. The user confirmed seeing the small
  colored grid, but reported poor placement and disappearance.
- The repeated-snapshot source delivered only about 0.087 updates/s (roughly
  eleven seconds), conflicting with the three-second stale-input expiry. A bounded
  direct continuous-source test delivered target sections about every 110 ms.
  The integrated collector now owns one `-t0.1 -v -c` process, bounds and parses
  complete target sections on a reader thread, and retains only the latest result.
  It serializes capture ownership with the counter reader and reports source
  failures. Interrupted final sections are not published.
- The integrated continuous HUD reported around 4 updates/s with sample ages near
  250 ms while ExternalGame remained active; no game restart was required for the
  collector change. The small HUD was moved to center for visual confirmation.
  New layer builds also inset corner/edge positions by 15% to avoid the image edge.
  The currently running target still has the previous layer binary mapped;
  the new safe-inset placement needs a later manual target restart.
- Eighteen simulated lifecycle checks (including profiler-chain preservation),
  twelve parser checks and seven continuous-section checks pass. These fixtures
  are separate from the real measured stream.
- **Pending:** stable user-visible confirmation of the continuous centered HUD,
  fullscreen verification, final native control interaction, safe-inset placement
  after layer reload, and overhead. The initial visible grid is a partial result;
  both finished presentations are not yet claimed.

## Transparency and restart correction - 2026-10-29

- The HUD background was hard-coded at 85% opacity, independently of the tile
  slider. It now follows the selected opacity at one fifth strength; defaults
  are 25% tiles for HUD and 20% fullscreen. The native slider displays percent
  and a Transparent preset is available. The user's custom saved opacity is kept.
- The configuration retained the dedicated test package while the integration
  was prepared for ExternalGame. Start/Apply now bind to the prepared heatmap target,
  displayed in the heatmap page. Capture and setup retain their selected target.
- Start is idempotent on the same live target. Stop closes the continuous source
  and waits for the worker before returning; a subsequent Start need not restart
  the game. The old asynchronous stop could reject immediate restarts.
- Uninstall is separated under Advanced setup with a confirmation explaining that
  it removes preparation. Stop preserves it. This prevents confusing hide/restart
  with uninstall/reprepare.
- Twelve parser, twenty-one lifecycle, seven continuous-stream and seven target
  selection checks pass on Android with explicit test fixtures. The APK builds
  and its v3 signature verifies. After installation and a user-performed ExternalGame
  restart, three real SDK cycles passed: Start, duplicate Start, complete Stop,
  then Start again. Every cycle received 46 fresh target tiles (ages 246-380 ms),
  preparation remained intact, and the game PID stayed unchanged. The final HUD
  was left active at center with 25% opacity. Physical transparency confirmation
  is separate and pending.

## Confirmed

- Android arm64 APK builds offline with the installed SDK/NDK; development APK
  signature verification passes. The desktop app and shared reader compile.
- The original separate GL SurfaceView produced a valid internal GL buffer but
  did not appear to the user. Replacing it with a TextureView composed into the
  Android window fixed the panel: the user confirmed CyInsightOVR was visible.
- Android SELinux rejected app-to-shell Unix streams. The replacement transport
  binds loopback only and pairs through a per-start random capability in private
  app storage. The actual application UID can read snapshots (instrumented).
- Three device bridge checks pass: reject unauthenticated requests, recover with
  authenticated access, reject operations outside the allowlist.
- Six named metrics are selected from the current firmware catalog to limit
  simultaneous collection. The real bridge reports six live observations. The
  earlier unrestricted prototype received 47 metrics; it is no longer default.
- Ten Android parser checks pass on explicit synthetic fixtures. C++ capture
  validation checks pass, with existing capture (18) and Meta GPU (68) checks.
- A real unavailable tile snapshot opens in the desktop native reader, with an
  explicit missing-data message. A desktop screenshot was inspected. The test
  process initially required manual cleanup. The viewport-copy defect discovered
  during the measured-capture tests below was subsequently fixed and shutdown retested.

## Real tile capture through the native application

- Confirmed the old workflow could capture while the configuration panel paused
  the target. A sleeping headset also produced empty source output. Neither
  condition is a parser success or evidence of unavailable hardware capability.
- The native capture button now closes the panel and starts a five-second
  foreground countdown. The service requests a short snapshot, matches its job ID,
  saves atomically without the Activity, and leaves a result notification. It
  never launches or restarts the user's selected target.
- A native-button instrumented run opened only our dedicated GPU test project
  after the panel closed. Capture `39c5f223-b190-4ffc-b266-e8b71974f09b` contained
  1,401 real measured tiles. The same run verified the C++ reader loaded that ID
  and exactly 1,401 tiles after reopening the panel, then copied the actual window.
  Instrumentation completed with RESULT_OK (-1). Earlier real runs contained
  990, 1,592 and 1,331 tiles; these were not substituted for the final UI result.
- Observed stages include Binning, Render, LoadColor, LoadDepthStencil,
  StoreColor, StoreDepthStencil, Dispatch, Blit and Preempt. Missing stages remain
  missing; no shader, eye, current frame or world-space cell is inferred.
- The native Quest screenshot was inspected. A height constraint was then added
  so the whole map fits below the controls instead of clipping its last row.
- On the final installed recorder, capture
  `1170acbc-faf1-4beb-bfcb-baf570f62478` saved and reopened 1,564 measured tiles.
  Window PixelCopy in that longer run failed with source-invalid as the panel
  became inactive. A separate immediate post-wake window test passed (RESULT_OK)
  and its inspected screenshot shows that exact capture with the full map fitting.
  These are separate checks; the failed screenshot is not counted as a pass.
- The saved .cyovr was pulled unchanged and opened by the desktop native reader.
  Its initial surface uses the largest recorded dimensions; the chosen surface
  and per-tile costs are visible. This is recorded tile data, not a live overlay.
- Surfaces without usable tile geometry are counted diagnostically and excluded
  from measured heatmap surfaces. An empty profiler response explains that the
  headset must be awake and the target rendering instead of claiming measured data.

## Desktop viewport assertion regression

- The user reported ImGuiViewport's PlatformUserData / RendererUserData assertion.
  A document copied the complete backend-owned viewport. The copy's destructor
  could assert on either document closure or application shutdown.
- Documents now store a plain ID/position/size value, never backend ownership.
- `--document-lifecycle-smoke` ran with two real .cyovr files: two native viewports
  and two native handles were present, one document was detached, reattached,
  detached again and closed. The remaining document and application then shut
  down with exit code 0. No assertion dialog was produced by the corrected run.

## Compact HUD correction (historical tests, 2026-10-28)

- User identified both the large CyInsightOVR configuration window and a separate
  Meta FPS graph. This establishes visibility of the Meta-rendered graph, not of
  our custom Android window.
- The October 28 version removed the configuration task when starting the HUD.
  This behavior was superseded on October 29: starting the HUD now keeps the
  configuration open, as requested. Both tests pressed the actual rendered ImGui
  button using its GL-thread position instead of assuming pixels.
- FPS and app GPU-time graphs are requested explicitly; the tall stats table is
  hidden after positioning. Applied graph/placement settings are read back from
  fresh, assembled Meta LOG_STATE output before reporting success.
- Eight simulated-service tests pass: chunked state, stale rejection, options,
  compact placement, targeted GPU graph selection, restore, restart and rollback.
- Scale 0.3 was tested but the device reported scale 1.0. The user found the first
  Small preset too large, then confirmed the final HUD-only size was suitable
  after the configuration panel and tall table were removed.
- No Meta SDK library was added to the APK or repository. The official SDK was
  examined only in ignored build scratch. Runtime controls use documented ADB
  broadcasts to the already installed service.

## Quest panel input and display settings

- The panel now keeps its header, tabs and scroll controls visible around a
  dedicated scrolling content region. Touch/pointer drags cancel a pending click
  before scrolling; Android scroll axes, forwarded joystick axes and page/directional
  keys are routed to native input. Footer buttons remain an explicit fallback.
- Display & HUD saves the Overview chart selection, HUD graph/value selections,
  size and corner in private preferences. Its metric list has category filters;
  Start HUD is pinned in the footer instead of below the complete list.
- The managed Meta catalog uses documented IDs and measured availability. It also
  covers all 25 numerical metrics originally enabled on this device, rather than
  leaving the old table selected when the user chooses only one value. Metrics
  outside the documented catalog remain untouched and that boundary is shown.
- A normal-window check on the Quest passed drag-without-click, page-key scrolling,
  Android AXIS_VSCROLL, tapping the actual rendered chart checkbox and saving that
  choice. Own-window PixelCopy returned success (0); its screenshot was inspected.
  Test preferences were restored afterwards. It opens no game and does not test
  headset visibility of a cross-game HUD.
- Earlier instrumentation runs sometimes received a paused/hidden Activity with
  zero rendered frames. Those are failures, not passes. The normal-launch check
  waits for real frames and rejects that state instead of forcing offscreen draws.
- Thirteen simulated-service checks cover selection, rejection, availability,
  restoration, restart and rollback. A separate real-service probe verified a
  selected FPS graph plus GPU-time numerical value and restored the prior settings.

## Not validated / not working

### Additional independent-overlay experiments

- A shell-owned SurfaceControl probe created two 360x144 transparent surfaces,
  without an Activity, dim layer, input interception or APK modification. Its
  first attempt hit the headless process's uninitialized default font; the second
  used only explicit vector shapes and submitted successfully. The user confirmed
  no cyan panel was visible. Both surfaces were released automatically.
- SurfaceFlinger reported the VR physical display on INVALID_LAYER_STACK.
  Android 14 LayerFilter::includes explicitly rejects that stack. Granting shell
  SurfaceFlinger access therefore did not make a VR overlay. No display routing,
  protected surface, platform signature or OS security policy was changed.
- The public Meta SDK Java client connected under our actual app identity and
  accepted 35 and then 90 custom-string updates. getLatestEventJson returned empty
  objects for this companion. The longer test also enabled the normal stats area
  temporarily to check whether compact-mode hiding affected debug content.
  Acceptance is not visibility: no positive headset confirmation was obtained.
  Finally blocks cleared our debug string and restored the previous HUD settings.
- A final 90-second public-SDK run was made after the user confirmed that the GPU
  test project was visible and the headset was worn. All 90 string calls were
  accepted, but returned metric objects had zero fields. The user reported no
  visible HUD at all. This is a failed cross-game visibility test, not a pass
  based on the SDK return value. The experiment is not enabled in normal use.
- A Meta on-demand screenshot request targeted only org.cyinsights.gputest. Its
  texture request timed out after six seconds, so it supplied no visual proof.
  Android PowerManager state alone is not treated as proof of headset presence
  or VR rendering; a visible/running test project is needed for the remaining check.
- A read-only JNI inquiry using the official Khronos 1.1.65 Android loader measured
  72 runtime extensions from this exact Quest. XR_EXTX_overlay was absent. No XR
  instance, session or swapchain was created. Loading the private system loader
  directly was refused by Android's namespace policy; no bypass was attempted.
- The normal app now exposes this inquiry in Connection & capabilities and
  distinguishes query failure (unknown) from a measured missing extension.
  The free Khronos loader is bundled with license/hashes; the proprietary Meta
  SDK used by the optional experiment is not bundled.

- The Android HUD button passes an own-app input test and attaches an Android
  overlay window. The user reported no visible HUD. This is experimental and
  must not be described as a working cross-game VR overlay.
- Compact Meta HUD appearance over an immersive game has not been visually
  confirmed in this iteration. Only the Meta FPS graph at Home was identified.
  This path uses Meta's renderer, not our custom charts or tile heatmap.
- A custom universal HUD is still unavailable. On the connected device,
  com.oculus.permission.ACCESS_OVERLAY_RENDERING and com.oculus.vrshell.OVERLAY_HOST
  are signature|privileged permissions. The Khronos runtime inventory does not
  list XR_EXTX_overlay for Quest. No supported public cross-game custom drawing
  route was established, and no plugin was made mandatory in target games.
- No live eye-image registration, exact Unreal frame clock association or unique
  shader/mesh attribution is provided. Observation times stay unanchored.

The updated app is installed. The native Meta HUD button integration test also
passes; it removes the panel, retains the HUD controller, and restores the saved
Meta settings in its cleanup. The app can reopen the latest measured heatmap.
The development collector runs until headset reboot; bootstrap it again then.
Changes have not been committed, published or released.

References:
- https://github.khronos.org/OpenXR-Inventory/runtime_extension_support.html
- https://registry.khronos.org/OpenXR/specs/1.1/loader.html#android-api-layer-discovery
- https://developers.meta.com/vr/documentation/native/android/ts-ovrgpuprofiler/
