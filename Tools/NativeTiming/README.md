# Installed Unreal timing reader

This optional MIT adapter reads `.utrace` through the licensed installed Unreal
Insights Core, TraceAnalysis and TraceServices providers. It exports CPU scopes,
old GPU timelines, supported GPU queue scopes, game/rendering frames, and a small
counter catalog. Only our adapter is compiled. No engine is modified or rebuilt.
Epic headers, import libraries and DLLs remain in the local engine installation;
none are included in this repository.

From an x64 Visual Studio developer prompt, with the matching installation:

```powershell
cmake -S Tools/NativeTiming -B build/NativeTiming-5.8 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCY_UE_ENGINE=C:/Example/UE_5.8/Engine `
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Example/CyInsights/bin/Release/NativeTiming/5.8
cmake --build build/NativeTiming-5.8
```

Build separately for each installed analyzer version, 5.5–5.8. The tested 5.8
headers require MSVC 14.44 and its developer environment. Select that analyzer
in CyInsights, or set `CYINSIGHTS_UNREAL_INSIGHTS` to its UnrealInsights.exe.
The application detects `NativeTiming/<major>.<minor>/CyNativeTiming.exe` beside
itself and runs it with the engine binaries as its working directory. Without
the adapter, the CSV bridge remains available. A failing adapter reports an error
rather than silently repeating the decode. Source trace compatibility depends on
the selected analyzer: older analyzers may not decode newer frame/GPU schemas.

Our code is MIT, see LICENSE. Epic dependencies retain their own license.
Distribution of adapter binaries may fall under Epic Engine Tools restrictions;
GitHub supplies source only. See [licensing](../../Docs/Licensing.md). The main
AGPL application does not link to Epic libraries.

## Internal transport

`timing.cynt` is a disposable uncompressed binary stream, not a capture format.
Integers are little endian, doubles IEEE-754. Layout:

- Eight bytes `CYNTIM01`.
- `uint32` timer count; each entry has a `uint64` provider ID and UTF-8 name
  (`uint32` byte length, then bytes).
- `uint32` track count; each track has a `uint64` ID, length-prefixed UTF-8 name,
  `uint64` scope count, then 32-byte `cynt::Event` records from Format.hpp.
- `uint64` frame count, then 32-byte `cynt::Frame` records.
- Eight bytes `CYNTEND1`, then `uint64` total scope + frame count, with no trailing
  bytes. Completion is required before the paged index is published.

Events contain start/end seconds, timer dictionary index, depth, flags
(bit 0: unfinished positive-infinite end), and zero reserved bits. Frames retain
their 64-bit ordinal and type (0 game, 1 rendering). Metadata-bearing timers
resolve to their provider timer identity. Timestamps retain full double precision.
GPU tracks use a separate high-bit identity namespace; CPU tracks keep thread IDs.

The parser validates references, lengths against remaining file bytes, finite
starts, durations, reserved fields and completion counts. Scopes are read in
bounded batches, cancellation is supported, and no event/file-size acceptance
cap is imposed. Dictionary/index metadata still requires RAM. Insufficient disk
space and provider errors remain explicit failures.

The small `counters.csv` catalog preserves the existing on-demand counter value
loader. Counter values still use that loader's CSV bridge. Tasks and activities
retain their adapters. Diagnostics go to `timing.cynt.log`. Source traces are
read-only, and analysis caches are written alongside our output. Independent runs
take three arguments: absolute source trace, absolute timing.cynt output, and
engine binaries directory; use that directory as the process working directory.
