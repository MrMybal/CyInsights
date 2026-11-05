# Troubleshooting {#title}

## Capture stays at zero bytes {#capture-stays-at-zero-bytes}
Check actual target/build/IP and plugin port 41855. A visible game is not proof its capture service is allowed. Read Session / Messages and the target log. Stop a failed owned capture before retrying; do not repeatedly restart an unrelated game.

## Quest is stuck or no HUD {#quest-is-stuck-or-no-hud}
Confirm the intended scene is visible, not Home, loading or a controller-required dialog. Keep the headset awake. ADB offline is distinct from app/render failure. Open CyInsightOVR > Connection > Start GPU service and inspect the setup/permission message separately. No separate companion/always-on PC is required. Accepted commands are not proof of visibility.

## No draw/triangle graph {#no-draw-triangle-graph}
Select HUD graph/value, not just Overview. Use the Vulkan APK and exact Development Vulkan target; reopen it fully once after preparation/update. Inspect layer connection, freshness, source cadence and publication errors. Prefer per-second counters; per-frame variants may be N/A in XR. See [HUD](doc:quest-hud).

## One GPU bar or no tiles {#one-gpu-bar-or-no-tiles}
Old traces cannot gain absent child scopes. Native scopes, Meta metrics and detailed stages are distinct. Confirm profiling before target GPU context and received stage/tile events, not just enabled settings. Wrong-target, stale and old-boot data is unavailable.

## Missing Stats/network/memory {#missing-stats-network-memory}
Check profile, compiled instrumentation, reader and load state. A declared counter can have no samples. Absence is not zero. Native log UI and full memory-callstack browsing retain limits.

## Black image or misaligned grid {#black-image-or-misaligned-grid}
Sharing consent does not prove image contents. Inspect saved pixels and foreground target. Keep black images flagged; do not claim exact mesh/eye identity. Image/tile observation times can differ.

## Antivirus/update failure {#antivirus-update-failure}
Preserve exact detection, path and package identity. Do not disable protection or assume signing proves safety. An executable removed after build cannot launch. Android key mismatch requires the matching key or an explicitly prepared backup/reinstall, never silent data erasure.

Record exact message, build, target, provider state and a short reproduction. Distinguish simulated fixtures from real hardware/trace validation.
