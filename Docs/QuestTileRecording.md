# Quest tile snapshots and associated views

CyInsights can optionally record CyInsightOVR tile observations alongside the
native Unreal trace. The resulting `.cycapture` includes the tile index, measured
surface/tile snapshots and any associated JPEG images. No separate import is
required to replay these sources in the desktop application.

## Capture

1. Install the matching CyInsightOVR development APK and authorize its ADB
   developer bridge. The bridge must be started again after a headset reboot.
   Enable Meta detailed profiling before opening the development game.
2. In CyInsights, open **Session & capture**, connect to the game's IP, and expand
   **Capture options > CyInsightOVR tile snapshots**. Enable **Record Quest tile
   costs**. Tile snapshots and images are both off by default. Choose 0.2–5
   snapshots per second (default 1 Hz).
3. For images, also enable **Include associated Quest images**. Supported Quest
   firmware uses its native `meta_screencap` through the explicitly authorized
   ADB developer bridge, only during this opt-in recording. Otherwise press
   **Enable Quest view sharing...** and accept the system dialog. Return to the
   game before recording. No audio is recorded.

The MediaProjection fallback is prepared at up to 2 Hz. Native images are
acquired by the recording worker with a three-second process limit and scaled
to fit 1280x720 while reading rows; a full-resolution bitmap is not retained.
Actual delivered cadence depends on capture time. Only one tile observation is
pending, so a slow image source cannot accumulate an unbounded queue. File writing and image
decoding run off the rendering threads. Screen sharing remains under the user's
control and can be stopped from CyInsightOVR or its notification. Native ADB
images stop with their recording. The bridge checks the target's resumed state
before and after acquisition; loss of focus leaves an unavailable image instead
of attaching an unrelated application's view. All-black native images retain
an explicit visibility diagnostic. Neither source establishes calibrated eye or
mesh identity.

Native per-frame GPU scopes, detailed Meta hardware stages, sampled tile
observations and compositor images are different sources with different costs.
Detailed profiling changes scheduling. Use short diagnostic captures and compare
performance against an unprofiled run.

## Replay and analysis

The **Quest tile snapshots / observation time** track appears in Timing Insights
when a valid `CyInsights.Sync` anchor maps the recording to the native trace.
Selecting a marker opens **Quest GPU heatmap** with its snapshot, image, surfaces
and tile stage durations. Previous/Next and the snapshot slider browse the record.

The current timeline selection limits the range summary and **Inspect range
peak**. A configurable absolute Render threshold (default 500 microseconds)
marks expensive observations. Detailed analysis shows median, p95, maximum,
missing values and the most expensive measured tiles. A tile alert uses either
the absolute threshold or a cost above 50 microseconds and the configured multiple
of the surface median. Partial edge tiles have different areas; compare geometry
as well as duration. Tile sums are not an independent GPU-frame duration.

Images appear separately by default. **Approximate grid over image** is an
optional visual aid, not calibrated screen-space registration. The image is a
composited Quest view and may be cropped, scaled, letterboxed or combined
differently from the profiled render surface.

## Clock and evidence limits

`observed_mono_ns` is when the collector received a complete profiler section,
not its GPU execution time. Image association uses the acquisition observation
from the same Android boot, with a maximum separation of one second. The signed
offset is shown in the interface. ImageReader's source timestamp is preserved as
`source_unspecified` and is not silently treated as an execution clock.

A native reader exports `quest-tiles-clock.json` only when it finds the matching
Unreal session and CPU clock frequency. A different boot, missing anchor or old
reader leaves native alignment unavailable; relative browsing still works and
the UI does not guess a frame number. The index rejects unsafe/duplicate paths,
incomplete counts, non-monotonic observations and inconsistent image offsets.

Render includes vertex and fragment work. Hot tiles help locate visual regions
for investigation; they do not establish a unique actor, mesh, shader or eye.

## SDK and developer bridge

The native Tools panel, SDK and MCP `capture_start` operation accept:

```json
{
  "ipv4": "192.0.2.5",
  "seconds": 5,
  "quest_tiles": {"enabled": true, "images": true, "hz": 1},
  "meta_gpu": {"enabled": true, "restart_game": false}
}
```

`capture_status` exposes `questCompanion` and `questTileMessage`. Acceptance is not
completion; wait for the completed archive. A cancelled SDK request does not
undo an accepted recording: use `capture_stop` explicitly.

`Tools/OvrBridge.py` also exposes `companion_capabilities`, `share_view_request`,
`tiles_record_start --package ... --hz 1 --images`, `tiles_record_status` and
`tiles_record_stop --recording-id UUID`. Stop requires the caller's recording ID.
The shell bridge adds its private local capability internally; clients do not
receive or supply it. The helper binds only to loopback on the headset.

## Validation status, 2026-10-29

Two real six-second recordings initiated by the desktop application's native
capture operation completed and opened automatically. Each archive contained
six measured tile snapshots and six associated JPEG files alongside the Unreal
trace and detailed Meta stages. One opened archive indexed 685,068 timing events
and 371 native frames; its matching session/boot clock anchor was exported.
The native-image recording contained 145,738 hardware stages, including 82,571
detailed Binning/Render/Load/Store events. File transfer and native alignment are
therefore exercised by real recordings, in addition to fixture regression tests.

**Game-image visibility is not yet validated.** Received game images were black,
while a separate native display probe captured visible Quest system UI. The user
confirmed the test scene was visible. Other active Quest development applications
repeatedly took foreground focus during these tests, making attribution of the
black images ambiguous. A controlled exclusive headset test is still required;
no system-UI image or synthetic archive is presented as game-image validation.
The external development game now starts with Vulkan, but its installed APK did not expose the
CyInsights capture endpoint or list the plugin in its startup log; combined trace
testing used the dedicated CyInsights GPU Test APK instead.

Sources: [Meta screen sharing](https://developers.meta.com/vr/documentation/native/native-media-projection/),
[Android MediaProjection](https://developer.android.com/media/grow/media-projection),
[Meta GPU profiler](https://developers.meta.com/vr/documentation/native/android/ts-ovrgpuprofiler/).
