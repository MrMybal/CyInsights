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
Our existing config requests separate load/store stages (`full_loadstore=true`)
and disables `low_overhead`, whose workload aggregation removes detail. Per-draw
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
