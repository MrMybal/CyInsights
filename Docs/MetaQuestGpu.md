# Meta Quest hardware GPU capture

CyInsights can coordinate an Unreal trace and the Quest's Perfetto GPU sources
from the native desktop capture window. No Meta Quest Developer Hub recording
or separate viewer is required. Authorized ADB access (USB or Wi-Fi) and Android
SDK platform-tools on the PC are required. This system capture currently needs
the PC; the standalone in-game plugin continues to record its native Vulkan
pass boundaries and Unreal trace without ADB.

## Test defaults

In **Session / Remote capture**, **Record Meta render stages and counters** is
enabled. **Restart the Quest game before capture** is off by default since the
13 October capture regression fix. The default preserves the running game's
existing profiling configuration. Explicit restart activates profiling for the
selected package, resolves its launcher activity, and resets the game session;
it can require accepting the headset's launch dialog. Probing an IP does
not change the device or start a recording.

The updated plugin supplies its Android package and process identity. Older
plugins require entering the package manually. ADB is resolved from the chosen
path, standard Android SDK environment variables or the standard SDK directory.
Device selection matches the game IP; an explicit authorized ADB identifier can
be supplied. No broad engine or software discovery is involved.

Native Unreal capture remains usable if the Meta source fails; the Meta reason
and separate hardware-stage/counter availability are reported. No texture,
buffer or shader readbacks are enabled by this option. Detailed Meta profiling
has a measurable cost and remains enabled on the headset after capture. The
official documentation describes roughly 10% GPU overhead; this is a diagnostic
test default, not the normal low-overhead capture preset.

## Common capture and clock alignment

The same `.cycapture` archive contains `unreal.utrace`, `meta-gpu.pftrace`, the
effective binary `meta-gpu.cfg`, `meta-gpu.info.json` and `manifest.json`.
The original `.utrace` is not rewritten. Perfetto writes its raw file on the
headset with a 32 MiB staging buffer and periodic flushes; the entire capture is
not kept in PC memory. Only the recording created by CyInsights is stopped.
Failure to verify its PID or flush retains the remote raw file for diagnosis.

The native reader uses Perfetto clock snapshots to convert GPU timestamps into
Android `CLOCK_MONOTONIC`. It then uses the same session's `CyInsights.Sync`
CPU ticks, the declared cycle frequency and Unreal analysis clock to join the native timeline. Android Unreal cycles use microseconds, whereas Perfetto uses nanoseconds;
the declared frequency conversion is required. Missing clock
relations, missing/mismatched sessions and invalid samples remain unavailable.
PC receipt time and GPU-result delivery frames are never substituted for
execution times. A snapshot-based relation is still subject to source clock
accuracy; it does not establish a unique Unreal frame or pass identity.

Available hardware stages appear as `GPU / Meta Game / <queue> / <stage>` tracks above
CPU threads. Other processes and unknown contexts have separate GPU tracks;
they do not participate in the game's bottleneck attribution. Names such as
Binning, Render, Load or Store are retained when the producer supplies them.
These are hardware stages, not reconstructed Unreal groups or shader costs.
Context, surface and submission information remains in the raw source.
Surface envelopes have a separate `Surfaces` display track. Unknown process
identity remains unknown even when a queue label contains the application name.
These tracks remain GPU tracks for display, but do not establish a measured
game bottleneck or an Unreal pass identity.

System counter samples appear under **Meta GPU system** in Counters. They have
source observation timestamps and unknown execution-frame identity; they are
not per-frame stage measurements. Units are currently marked `raw` rather than
guessing a normalization. Negative sentinel/nonfinite values are excluded.
Existing range selections filter the timing analysis and counter intervals.

## Availability and validation

Quest firmware, graphics context and producer capabilities govern available
data. Merely advertising `gpu.renderstages.oculus` does not prove that any stage
was recorded. Each capture reports actual valid stage and counter counts.
Compressed Perfetto packets and sequence-local custom clock relations are not
decoded in this first importer; the raw trace is preserved and unsupported
auxiliary data does not replace the native Unreal timeline.

Validation uses synthetic wire/clock/loss fixtures, an isolated fake ADB running
the production orchestration, and the production binary GPU exporter through
the native paged index. A licensed native reader also decoded an existing
Unreal trace plus two explicitly synthetic stages and one system counter; the
actual native UI/IPC confirmed the GPU lane, 0.2 ms duration, histogram and
range selection. Unsupported auxiliary packets retained all 8,156,558 native
events. Android plugin modules compile with Meta UE 5.5.3 and official UE 5.8.3;
timing adapters compile for UE 5.5–5.8. Real Quest recordings on 12 October 2026 supplied GPU
counter packets but **zero hardware stage packets**. Beyond's remote service
was listening but its game loop stopped advancing and control requests timed
out. Full Beyond `.cycapture` and hardware Binning/Render display remain
unvalidated. This build must not be described as proving those stages on Beyond.

## GPU prerequisites and diagnostics, 13 October 2026

For a manual MQDH cross-check, open **Trace Analysis by Perfetto / Perfetto
settings**, enable **GPU Render Stage Trace** and **Enable high precision GPU
render stage tracing**, then **GPU traces / Enable**, and launch the target game
after activation. **Metrics HUD** is an overlay; it is not the hardware-stage
capture. **Render Pass Diagnosis** is the preset for this investigation.
These settings govern MQDH recordings; they do not change an already saved trace
or our separate CyInsights configuration.

When restart is explicitly selected, CyInsights enables detailed profiling for
the selected package using `ovrgpuprofiler -e <package>`. It reads `-i` and preserves the
driver response, activation response, advertised source flags and requested
restart state in `meta-gpu.info.json` and the source manifest. An explicitly
disabled response rejects Meta preparation while the native capture remains
available. Without restart, an already-enabled configuration is preserved and
no activation or launch command is sent. An unsupported query stays explicitly unverified. Driver state is
not evidence that an existing game GPU context is instrumented.

The decoder separately counts raw stage packets, packets containing durations,
valid aligned stages and stages rejected for missing clock relations. Lost
incremental dictionaries do not erase the raw packet counts. A counters-only
capture no longer implies that hardware steps were received. The result message
explains missing packets versus rejected packets, and the raw source is retained.
The legacy configuration requests separate load/store stages (`full_loadstore=true`)
and disables `low_overhead`. This is insufficient for the high-precision path on
the tested runtime; see the 26 October correction below. Per-draw
capture remains off: it changes scheduling and is not a low-impact default.

Read-only inspection of the connected Quest 3 on 13 October confirmed Android
14, detailed driver profiling enabled, and both Meta GPU sources advertised.
No game was started/restarted and no new hardware recording was made. Production
decoding of three retained hardware recordings found **zero stage packets**;
one contains 96,992 valid counter samples. This is not a UI visibility problem
or loss of clock alignment for received stages. The effective target context
and real stage delivery require a new authorized game capture for validation.

The device's installed profiler help is older than the latest Meta documentation
and does not advertise the new `-s` per-stage metrics option. Do not assume a
feature documented for newer runtimes works on this firmware. Capability checks
must precede adding advanced per-stage/per-draw metric selection.

See [Meta's prerequisites](https://developers.meta.com/vr/documentation/native/android/ts-perfettoguide/)
and [profiler activation](https://developers.meta.com/vr/documentation/native/android/ts-ovrgpuprofiler/).

## Isolated rendered test, 17 October 2026

The separate Development project described in [QuestGpuTestProject.md](QuestGpuTestProject.md)
was packaged against installed stock UE 5.5.4 libraries and run on Quest 3.
A 25-second recording contained 219,366 timed Meta stage packets and 20,204 valid
system counter samples. The actual stage names were `Workload`, `Dispatch`,
`Blit`, `Preempt` and surface envelopes. **Binning, Render and separate load/store
stages were not present in this recording**, despite the requested detailed
configuration. The native reader aligned 213,791 Meta intervals with the Unreal
timeline, and their separate GPU tracks were inspected in the native app.
This proves received stage display; it does not prove the missing fine stages
or unique correspondence with Unreal groups.

That recording exposed an ADB cleanup bug: a nested `sh -c` lost its script
argument boundary, preventing automatic retrieval. The raw file was recovered
manually for the analysis above. Cleanup now sends the complete polling script
directly to `adb shell`, with a regression fixture reproducing the original
failure. The recovered archive is not evidence of automatic retrieval success.

The stock UE 5.5 Vulkan loader also lacks the exported `EndRenderPass2KHR` slot
present in the Meta fork. Requiring it disabled the entire optional pass
collector. The build now treats this slot independently and observes stock
5.5's `BeginRenderPass2KHR` / classic end pair. A production-collector hardware
probe with the EndPass2 symbol omitted resolved both legacy and stock-5.5 pairs
on Adreno 740: two resolved, zero pending/dropped. Blocked-generation tests also
rejected stale query values. The test APK compiles and links with this change;
its packaged capture validation remains necessary.

During an additional direct profiler cross-check, the test process crashed
through the Meta/Adreno profiling layer. Another instrumented startup also
crashed before capture. Preserve the driver stack and compare with profiling
disabled; do not report a complete stable hardware workflow from compilation
or a manually recovered recording alone.

## High-precision source correction, 26 October 2026

The current Meta producer advertises the `gpu_renderstage` TrackEvent category.
The previous CyInsights configuration selected only the legacy
`gpu.renderstages.oculus` source and ATrace categories. Setting the modern
`gpu_renderstages_config` flags there did not select the high-precision path on
the tested Quest. Removing just `surfaceworkload` did not fix it.

CyInsights now queries the full source/category inventory and selects
`track_event` with only `gpu_renderstage` enabled when advertised. Other
TrackEvent categories are disabled, absolute timestamps are requested, and
real-time counters remain a separate source. It does not request the competing
legacy render-stage transport in that mode. Headsets without that category use
the explicit legacy fallback; availability is still determined from received
packets, never the advertised name or the profiler toggle alone.

A real five-second recording of the separate Development test on Quest 3
(Adreno 740, runtime build 52433670048800520) produced **154,797 timed GPU stage
packets**, all decoded with a clock relation. They include **1,287 Binning,
41,195 Render, 12,076 LoadColor, 26,280 StoreColor, 4,890 LoadDepthStencil and
11,450 StoreDepthStencil** intervals, plus Bin, Dispatch, Blit, Preempt and surface
envelopes. This is hardware capture, not a synthetic fixture. The same test
session's legacy-source captures contained only Workload/Dispatch/Blit/Preempt
and surfaces. No test used or relaunched Beyond.
The raw high-precision capture above was collected with an isolated Perfetto
configuration. Its requested duration was five seconds, but the producer emitted
separated timestamp batches spanning roughly 452 seconds. These raw counts do
not establish a five-second execution window or alignment with Unreal frames. An earlier combined attempt was interrupted when the headset rebooted and
produced no complete archive. The later real in-app validation below completes
the native recording, auxiliary collection, common archive and timeline import;
the isolated raw recording alone was not that validation.

The manifest now records `render_stage_transport`, `stage_types`,
`fine_stage_count` and `hardware_detail_status`. A nonempty coarse trace reports
`coarse_only`, not measured fine rendering detail. Per-type counts describe the
auxiliary source as a whole; they do not establish game process identity or
mapping to Unreal groups. The existing decoder/importer retains the original
labels and timestamps, so there is no reconstructed allocation of tile time to
BasePass, translucency, shaders or individual subpasses.

This correction is in the **desktop capture coordinator**. It requires a new
capture from the updated app and opening the complete `.cycapture`; it cannot
add absent Meta packets to an old `.utrace`. It does not itself require rebuilding
the APK. Keep a current CyInsights plugin for native scopes, session clock anchors
and Android process/package metadata. Profiling must still be active before GPU
context creation; capture does not restart the game by default. Detailed profiling
remains an opt-in source with measurable overhead outside this test default.

## In-app validation and startup diagnostics, 26 October 2026

The small test APK repeatedly crashed in Adreno `vkCmdBindPipeline` on startup,
with detailed profiling both enabled and disabled. Moving only that test
package's existing `RHICache` to a backup allowed the same installed APK to
render and open TCP 41856. This is an observed recovery on that test package,
not evidence that clearing users' game caches is generally required; CyInsights
does not clear game caches.

A recording requested through the actual desktop application's capture service
received 1,833,341 Unreal bytes before control timed out and the Quest rebooted.
It produced no complete `.cycapture`. Its raw hardware source and this failure
must not be described as a successful combined on-device capture. The subsequent
post-reboot Meta launch gate was removed from the unattended test package by
declaring optional hands support; a separate start while asleep also produced
an Android ANR before the first window. The successful runs below started the
rendered game while awake.

The coordinator now waits up to 30 seconds for a cold game's TCP listener,
publishes its waiting status, and can be stopped during that wait. The app opens
a successfully completed archive automatically; the capture view has a checkbox
to keep manual opening instead. An incomplete recording never triggers opening.
The Meta source records Android boot identity before and after recording when
available. A changed identity reports a reboot and rejects that auxiliary data;
it never signals a stale recorder PID or joins timestamps across device boots.

## Complete desktop-owned Quest capture validated, 26 October 2026

The native app's capture service recorded the running separate Development APK
on Quest 3 (Adreno 740, runtime 52433670048800520). A first five-second native
control recording was received, packaged and opened automatically: 3,014,943
Unreal bytes, 507,186 indexed events and 414 native game frames. This control
recording deliberately disabled Meta collection and the optional completion
hook to isolate transport and opening.

The following ten-second recording used the plugin's normal basic defaults,
including completion timestamps, plus Meta high-precision collection. The game
was not stopped or restarted between these captures. The app received 6,823,579
Unreal bytes, collected the auxiliary data and wrote a single `.cycapture`:

- Raw Meta source: 368,881 timed hardware stages, including 235,938
  Binning/Render/Load/Store stages, and 13,521 valid counter samples.
- The native reader imported 360,804 hardware stages and 13,521 counter samples
  against the matching Unreal session's Sync anchor; 8,077 stage intervals were
  rejected. No GPU frame IDs were invented.
- The opened common timeline contained 1,376,407 events and 689 native game
  frames. Both CPU tracks and the test queue's Binning, Render, LoadColor,
  LoadDepthStencil, StoreColor and StoreDepthStencil tracks were present.
- Desktop decoding took about 2.08 s and indexing 0.21 s on the validation PC.
- Android boot identities before and after recording matched. The game was
  still alive after transfer and the app reported a complete capture.

The actual archive was reopened by a second normal app instance and its timing
view was visually checked. This validates the integrated capture/open workflow,
not just a developer probe or an independently recorded `.pftrace`.

After disabling persisted Vulkan cache loading in only the diagnostic workload,
two further cold starts rendered successfully without moving or clearing its
existing cache files. A final five-second capture from that packaged version
also completed through the desktop app: 630,034 indexed events, 360 native frames,
181,911 raw hardware stages (121,953 Binning/Render/Load/Store stages) and 23,642
valid counter samples. That common archive was opened automatically and remained
open for user inspection.

Counts include runtime/system GPU queues as well as the test queue. The producer
supplied queue labels but no resolved PID for these stage packets, so the UI
keeps **Process unknown** and the original queue label. Hardware execution is
aligned with the CPU timeline through clock anchors; its exact Unreal pass,
subpass, shader, actor and game-frame identity remains unavailable. Vulkan pass
correlation retains its approximate-alignment label.

## SDK/MCP

`capture_start` accepts the same `meta_gpu` options as the desktop window:
`enabled`, `restart_game`, `adb_path`, `device`, `package`. `capture_status`
includes `metaGpuMessage` and the eventual archive path. Capture permission is
required; cancelling an SDK job does not stop an already accepted recording.
Use `capture_stop` explicitly.

## Sources and licenses

The GPU wire decoder is an independently written MIT component. No Epic,
Perfetto or Meta implementation is copied into it. The desktop coordinator
retains the application's AGPL-3.0-or-later license. Installed ADB and headset
services are external tools, not redistributed executables.

- [Meta GPU profiling, activation and overhead](https://developers.meta.com/vr/documentation/native/android/ts-ovrgpuprofiler/)
- [Meta Perfetto integration](https://developers.meta.com/vr/documentation/native/android/ts-perfettoguide/)
- [Perfetto TracePacket schema](https://github.com/google/perfetto/blob/main/protos/perfetto/trace/trace_packet.proto)
- [GPU render stages schema](https://github.com/google/perfetto/blob/main/protos/perfetto/trace/gpu/gpu_render_stage_event.proto)
- [GPU counters schema](https://github.com/google/perfetto/blob/main/protos/perfetto/trace/gpu/gpu_counter_event.proto)
- [Clock snapshot schema](https://github.com/google/perfetto/blob/main/protos/perfetto/trace/clock_snapshot.proto)

The optional developer workload in `QuestVulkanPassProbe.cpp` is reproducible
with `python Tools/build_quest_meta_probe.py --ndk <NDK-root>`; it is never
launched by the product and makes no promise of firmware stage availability.
