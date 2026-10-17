# .cycapture version 1

The format is independent of the collector and Unreal version. Native traces and collector-native payloads are opaque entries: do not rewrite a `.utrace`, translate a GPU frame number into an Unreal frame number without evidence, or silently remove unavailable capabilities.

The optional manifest field `capture_options` records the applied, versioned capture policy (v1): native channels, basic observations, selected stat groups, Meta per-frame observation/optional polling, native GPU pass detail and GPU snapshot options. Old captures may omit it. A policy describes requested collection, not proof of availability; source/sample statuses remain authoritative. Remote GPU requests do not imply that detailed collector payloads were transferred. See [CaptureProfiles](CaptureProfiles.md).

## Container

Uncompressed, streaming archive. Every integer in the container header is unsigned little endian. All paths are relative UTF-8 paths using `/`.

| Field | Bytes | Meaning |
| --- | ---: | --- |
| Magic | 8 | ASCII `CYCAP001`; unsupported container versions fail |
| Entry count | 4 | 1–100,000 |
| Per-entry path length | 4 | 1–4,096 |
| Per-entry payload length | 8 | Byte length |
| Per-entry SHA-256 | 32 | Digest of payload bytes |
| Per-entry path | path length | UTF-8 relative path |
| Per-entry payload | payload length | Unmodified file |

The first entry is `manifest.json`, maximum 16 MiB. A reader verifies checksums, lengths and referenced payload existence; rejects duplicate or colliding paths, traversal, absolute paths and Windows device names; and extracts only into a new directory. Limits are currently 128 GiB per archive. Writers create a `.partial`, verify it and rename it after success. Inputs must be closed, stable files; the archive packer detects changes during its two-pass copy. It is not a producer completion handshake.

Version 1 preserves all bytes and has no compression or random-access footer. A later container revision may add indexing/compression without changing the source payloads.

## Manifest

Required: `format: "cycapture"`, `version: 1`, `session_id` string and `sources` array. Unknown additional fields are preserved. Each source declares `id`, `status` (`measured`, `derived`, `unavailable`), an optional archive `path`, and a `reason` whenever a capability is unavailable. Source IDs identify instances; use distinct entries for every captured GPU frame or report.

`metadata` includes project, full Unreal engine version, platform, graphics API, CPU, GPU, UTC capture time and, when available, build configuration, device, driver and collector versions. Missing fields must remain missing or explicitly unavailable. Metadata from imported traces should not be guessed.

Suggested entries:

```text
manifest.json
unreal.utrace
sync.jsonl
gpu/<frame>/capture.json
gpu/<frame>/events.bin
gpu/<frame>/drawstates.bin
gpu/<frame>/pipelinestates.bin
gpu/<frame>/barriers.bin
gpu/<frame>/shaders/<sha256>.bin
images/<resource>/<mip>/<slice>.png
buffers/<resource>.bin
meta/metrics.csv
objects.jsonl
```

These are logical categories, not mandatory payloads. The existing CyGPUInspector capture layout is retained below each GPU frame directory, including any side files supplied by that collector. Buffers, shaders and thumbnails retain their raw format metadata and resource identity; a PNG alone does not represent the full underlying texture.

## Clock rules

### Native network transport

The remote receiver saves the native Unreal TCP stream byte for byte. The final device manifest and `unreal.utrace` are automatically packed after both a session-matched remote stop and native TCP EOF. Multiple sources may refer to this same payload: native timing, embedded Quest metrics and embedded `CyInsights.Sync` anchors. No separate synchronization payload is required for remote captures. The `network` manifest object records transport, receiver, received-byte count as a decimal string and completion evidence. Receipt timing is transport diagnostics, never an execution-clock calibration. Invalid/interrupted transfers retain their partial trace and do not produce a complete archive. See [RemoteCapture.md](RemoteCapture.md).

`CyInsights.Sync` records `CpuTicks` (uint64), `ObservedGameFrame` (uint64) and `SessionId` (wide string), using the plugin-owned `CyInsightsSync` channel enabled by the coordinator. This avoids linking to UE 4.27's unexported Core counter channel. Standard counters and Quest metrics still use the native Counters provider. These measured game-thread observations do not identify the GPU execution frame. Local captures retain `sync.jsonl` too. Decoding these events and correlating native time origins remains separate analyzer work.

Each clock is scoped to session/device/queue and identifies its unit, tick frequency, reset epoch and origin. **64-bit tick/frame/object IDs are decimal strings in JSON**; numbers above 2^53 must not pass through a JSON double. The binary event payload retains its original integer representation.

For a calibrated anchor `(ticks0, session_seconds0, frequency_hz)`:

```text
session_seconds = session_seconds0 + signed_integer_delta(ticks, ticks0) / frequency_hz
```

Subtract integer ticks before converting to floating point. Preserve each calibration sample's uncertainty, method, validity interval and producer. At least two separated samples are needed to detect drift; segmented mappings are required across device resets, clock discontinuities or excessive residuals. The current `MapClock` helper implements the single-anchor calculation only; drift fitting and automatic calibration are future work.

Wall-clock UTC is descriptive metadata, not a CPU/GPU synchronization clock. GPU queue clocks, Windows QPC, Unreal cycles, Android monotonic clocks, and a Meta report clock must not be assumed interchangeable. Unreal analyzer time has its own origin; correlate an explicit trace anchor before mapping the plugin-relative origin to it. Without that anchor/calibration, display separate lanes/axes with an unavailable relationship.

## Frames and delayed results

### Embedded Unreal frame statistics, schema 1

`CyInsightsStats.Spec` identifies a metric by a capture-local uint32 ID, floating/integer type, group, name and unit. `CyInsightsStats.Sample` preserves this ID, raw uint64 `BeginCycles`, `EndCycles`, `ObservedCycles`, uint64 `GameFrame`, int64 `StatsFrame`, uint32 `Flags`, int64 `Integer` and double `Real`. Flags are: source-frame bounds available (1), delayed result (2), execution identity unknown (4), derived aggregate (8), valid reading (16). Game and stats frame identities are separate; neither is substituted for a GPU execution-frame identity. Missing game identity uses UINT64_MAX; missing stats identity uses -1. Samples without known bounds retain observation time only.

The source `unreal.frame_stats` refers to the same `unreal.utrace`, with its sample count as a decimal string. A standard Counters mirror exposes values at observation time for other tools. The local custom analyzer converts the three cycle timestamps with the native trace clock and writes a disposable little-endian `CYFS0001` cache. Records are tag 1 + metric definition (ID/type/three uint32-length UTF-8 strings), tag 2 + 64-byte sample, or tag 255 + uint64 total sample count. The sample layout is defined in `Tools/NativeTiming/FrameStatsFormat.hpp`; the original native events remain authoritative. This cache is not an additional required capture payload. See [FrameStats.md](FrameStats.md).

### Embedded Quest metrics, schema 1

The Android plugin writes both standard Unreal `Counters` events and `CyInsightsQuest.Source`/`CyInsightsQuest.Sample` events into the existing `.utrace`. No separate Meta CSV is required. The source manifest entry `meta.openxr.metrics` refers to `unreal.utrace` once a valid reading has been observed in the current trace; before that it remains unavailable with a reason. It retains the valid sample count as a decimal string and reports unknown measurement interval/execution-frame association explicitly. Standard counter names use `CyInsights/Meta<OpenXR metric path> [<unit>]/uint` or `/float`, with `<metric>/available`, `<metric>/uint/available` and `<metric>/float/available` emitted independently on every poll. Per-type flags prevent a valid integer from making an invalid float appear usable. Their observation time is emitted by the native Counters provider, on Unreal's CPU clock.

`Source` records the schema version, CPU cycle, discovered catalog size, state, clock and timestamp meaning. Every `Sample` retains `ReadBeginCycle`, `ReadEndCycle`, `ObservedGameFrame`, OpenXR `PathId` and path string, API result, raw unit/flags/values and separate accepted integer/float validity flags. Path IDs are local to the XR instance and must not be matched across captures; use resolved paths when available. Invalid float values are retained only as raw data, never emitted as a valid standard counter. Measurement source is the Meta runtime; the recorded query interval is the plugin's observation bracket, **not** a GPU execution interval. Repeated polls may read the same underlying runtime measurement. Do not turn the GPU-frame-time scalar into an execution bar anchored at its observation time, or assume the observed game frame is the GPU frame.

The extension enumerates capabilities; the plugin does not assume that GPU frequency, thermal counters or any other particular path exists. The first collector provides sampled metrics only. Vulkan commands, GPU hardware stage traces, shaders/resources and draw timestamps remain unavailable until their respective adapters are implemented. Standard Counters are imported by the application through the installed Epic export bridge, retaining native units and per-type availability; see [CounterAnalysis.md](CounterAnalysis.md). The dedicated raw Quest-event decoder remains pending; preserve the trace byte for byte.

### Execution identity

`Source` and `Sample` also retain instance/session generations so that raw path IDs remain scoped across XR session recreation. Unresolved path names are isolated by instance in standard counter names. Future/unknown units retain their raw numeric ID in the unit suffix instead of merging them into one guessed unit.

Execution identity is `(session, collector, device, queue, execution_frame, event_index)`. Retain `received_frame`/receipt time only as transport diagnostics. Keep pending frames until their result fence is resolved, explicitly fails or times out; never hard-code "subtract three frames". Missing results and event losses must be recorded. Do not attribute a late result to the UI's current frame.

The current plugin records measured game-thread frame IDs and CPU ticks; sampled collector present IDs have `frame_relation: "approximate"`. Game, render, RHI and present frames differ, especially in editor multi-window rendering. The reused collector selects its main swapchain and optionally scopes detailed captures to the main viewport. A frame correspondence must retain both source identities and its provenance/confidence.

## Objects and analysis

An object link should include session ID, source object ID, lifetime/generation, package/soft object path, object kind and exact/candidate/unavailable relation. Multiple candidates are valid. Actor names alone are not persistent keys; raw pointers are not persistent IDs. Instancing, Nanite and shared materials prevent unique associations in many captures. The Unreal/CyEE bridge should resolve only an explicit known identity and report stale or ambiguous mappings.

Measured event intervals can be aggregated within an interval. Pass names, inferred dependencies and shader/resource cost attribution require separate provenance from the underlying measurements. A missing timestamp is unavailable, not zero. An unfinished native scope has measured start and unavailable end/duration; any visual clipping is a display estimate.


## Optional Meta Quest system GPU source

Desktop-coordinated Quest recordings can add `meta-gpu.pftrace`, the effective
`meta-gpu.cfg` and `meta-gpu.info.json` to the same archive. Source id
`meta.quest.gpu` retains independent stage and counter availability/counts,
selected Android package/processes, native capture session id and declared
Unreal CPU cycle frequency. The `.utrace` remains intact. Raw Perfetto clocks
map through snapshots to Android MONOTONIC, then the same native Sync session;
missing relations remain unavailable. Hardware stage-to-Unreal-frame identity
is unknown. See [Meta GPU capture and validation](MetaQuestGpu.md).


## Vulkan subpass contents

`CyInsightsVulkan.Contents` adds capture-local pass/submission identities and
per-subpass counts for intercepted direct/indexed/indirect commands. Parent
GPU bounds are retained; no per-subpass duration or Unreal group identity is
implied. Indirect buffer values and pre-capture secondary recording coverage
remain explicitly unavailable/partial. The disposable `CYVP0001` detail cache
uses fixed 112-byte records, a completion footer and a uint64 count. See
[contents, UI/SDK and validation](QuestSubpassContents.md).
