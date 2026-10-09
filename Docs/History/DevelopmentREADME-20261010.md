# Historical development notes

This is the pre-publication development README. Paths and licenses described
here are historical; current instructions are in the repository root README
and Docs/Licensing.md.

# CyInsights

Independent C++ / Dear ImGui profiling application, with an interface built around Unreal Insights' timing workflow: docked timing tracks, frame selection, interval statistics and GPU command detail. The CyGPUInspector interface is not reused. Its Windows collector, IPC, capture model and graph reconstruction are reused as separate components.

**Current state: first slice, not full Unreal Insights parity.** Native timing and counters are decoded by the matching installed `UnrealInsights.exe`, using its export commands. The native `.utrace` is preserved byte for byte. Memory, network analysis, loading and other Insights views still need their own Dear ImGui implementations. The application targets Windows; it can analyze archived Android traces using the installed native analyzer.

## Application logo

The 10 October release **Release-20261010.1** embeds the approved blue/cyan Cy logo in the Windows executable/window icons, main menu and Session header. The master image is preserved; nine icon sizes and the UI PNG are embedded resources, with no external image required at startup. See [branding](Docs/Branding.md). It retains the AI Inspector/MCP and existing profiling features.

## Native interface revision and executable alert

The 9 October AI revision (`Release-20261009.6`) adds a dockable **AI Inspector** and **17 CyToolsCore/MCP operations** for compact trace analysis, timeline control, findings and remote capture. Open Connection, enable the local bridge and copy the MCP configuration to your AI client. Application control and remote capture have separate permissions. Findings reacts to the selected/visible loaded interval; native stream analysis during recording is not implemented. See [AI integration](Docs/AiIntegration.md) and [capability coverage](Docs/CapabilityCoverage.md). 483 native checks pass across thirteen suites, plus five Python companion tests and packaged official MCP validation.

Exact-name filtering by double-click, function histograms by right-click and analyses that follow a selected timeline interval remain available. Function queries use indexed timer blocks; histogram pages reuse completed summaries. See [timeline function analysis and measured performance](Docs/TimelineFunctionAnalysis.md). **Distributions** and **Tasks** analyze thread costs with/without recognized waits, scope durations and recorded task execution/queue delays. Native task identities and links use optional adapters with installed Epic binaries for UE 5.5–5.8. Missing TaskTrace exposes CPU scope candidates explicitly. See [trace distributions and tasks](Docs/TraceAnalysis.md), [frame diagnostics](Docs/FrameDiagnostics.md) and [multithreaded preparation](Docs/MultithreadedPreparation.md).

The interface is native C++/Dear ImGui, with no web view. Custom navigation buttons and vector icons, Segoe UI from the local Windows installation, a full-width grey timing canvas, compact pastel scopes and a narrow Timers/Callers/Callees sidebar replace the earlier prototype layout. Frames remain visible above the timeline; session metadata moves into its own view. Mouse-wheel zoom, left-button horizontal pan/vertical zoom on the overview and ruler, middle-button pan, Shift+ruler interval selection, previous/next frame navigation, sortable statistics, double-click timer focus or exact scope-name filtering and `F` to fit the selection are implemented. Font and custom control geometry follow DPI scaling. CSV loading and timing indexing run in background workers; a disk index loads visible timing windows without CSV-byte or event-count caps. Loading shows phases, elapsed time, progress, cancellation and persistent errors. Native ThreadId and TimerId are preserved, so same-name objects remain distinct. Measured frame rows from newer exports retain a separate frame identity and populate the overview when available; older exports retain explicitly derived tick candidates. GPU command details open in a separate tab. Counters provides search, interval statistics and value tracks on the continuous timing axis, including embedded Quest metrics. Native Log remains unavailable. A native capture comparison pane supports independent A/B intervals, total/window-normalized/per-occurrence costs and explicit candidate/ambiguous name groups. See [counter analysis](Docs/CounterAnalysis.md), [capture comparison](Docs/CaptureComparison.md) and [interface behavior and verification](Docs/InterfaceRevision.md).

**The previous `bin/Release/CyInsightsApp.exe` was flagged by Kaspersky as `VHO:Trojan-PSW.Win32.ReaderDB.gen` and remains locked by its AVP service. Its detection is unresolved.** At the user's request, the current sources were rebuilt from a new build directory into the versioned `bin/Release-20261008.1/CyInsightsApp.exe`. Kaspersky completed a scan of that new executable with zero detections, suspicions, skips or errors. The revised UI opened a real `.utrace`, loaded all 14,192,325 events from the user's 49.6 MB trace and rendered successfully. A second real 1.4 GB trace opens with all 353,528,278 events retained. Horizontal drag, vertical drag zoom and wheel zoom pass the owned UI smoke tests on both. The larger first opening takes about 17 min 30 s with the current CSV bridge. The application is unsigned; this result does not certify general safety or resolve the original binary's detection. Automatic standalone launch remains disabled in the sample configuration. See [the test release](Docs/TestingRelease.md) and [the security investigation](Docs/SecurityReview.md).

For the current test release, launch `bin/Release-20261010.1/CyInsightsApp.exe` and use **Open capture** to select a `.utrace` or `.cycapture`. Native decoding still requires a matching installed Epic `UnrealInsights.exe`; change it in **Session > Select UnrealInsights.exe**, then run **Analysis > Analyze native timing** if needed. **Session > Analysis workers** controls concurrency for the current application session.

To check the current sources without linking or launching an application:

```powershell
./build.cmd Release --compile-only
```

The isolated calculation/import tests can be built and run without linking or launching the application:

```powershell
./build.cmd Release --tests-only
```

## Build and run (after resolving the executable alert)

Visual Studio 2022 with C++ and CMake, plus Python 3, are required. The preparation command reads the existing repository and writes only into this project. Builds verify MSVC/Ninja header dependency parsing, including the French compiler locale.

```powershell
python Tools/prepare_dependencies.py --source C:/Example/CyGPUInspector
./build.cmd
./bin/Release/CyInsightsApp.exe
```

The preparation script copies the collector plugin into `Plugins/CyGPUInspector`, preserves its notices, and restricts **all** its modules to Debug, DebugGame and Development. CyInsights has separate Runtime and Editor modules with the same restrictions; Shipping, Test and Server are excluded. The runtime module records traces on the target device. The Editor module supplies Tools menu entries.

## Capture

### Capture réseau par IP

Dans **Session**, saisir l'IPv4 du jeu/Quest, puis **Start remote trace**, **Stop remote trace** et **Open received capture**. Le plugin démarre son service automatiquement dans les builds autorisés. Le PC reçoit la trace native et les métriques Quest embarquées, puis crée le `.cycapture` après la fermeture du writer. Port de commande par défaut : TCP 41855 ; le port de réception et l'adresse de retour du PC sont choisis automatiquement. L'analyse s'effectue après l'arrêt. Voir [fonctionnement, options et validation réseau](Docs/RemoteCapture.md).

### Capture locale

Enable CyInsights in an Unreal project. On Windows configure the reused collector's `StandalonePath` to `CyInsightsApp.exe`; this sample uses `bin/Release/CyInsightsApp.exe`. ReShade and the add-on must be available in the collector's ThirdParty binary directory, as in CyGPUInspector. No engine file is modified and only project/plugin modules are compiled.

Console commands:

- `CyInsights.Start`: start a native capture without replacing an active native trace.
- `CyInsights.Gpu [1..8]`: start tracing if necessary, then request detailed GPU frames from the Windows collector.
- `CyInsights.Stop`: stop the trace owned by this plugin.
- `CyInsights.Capture [seconds]`: timed native capture, default 5 seconds.

Native staging is saved in `Saved/CyInsights/<session-id>`. The standalone receives collector frames and writes them into `bin/Release/Captures/<collector-session>/<frame>`. It associates timing results with their **execution frame**, not the frame in which results arrived. Texture readback is currently disabled in this integration; externally supplied shader/image/buffer payloads can still be preserved in the archive.

After capture writers have closed, assemble and package:

```powershell
python Tools/assemble_capture.py --native-staging Saved/CyInsights/<session-id> --gpu bin/Release/Captures/<collector-session> --output build/combined-staging
./bin/Release/CyCaptureTool.exe pack build/combined-staging build/example.cycapture
./bin/Release/CyInsightsApp.exe build/example.cycapture
```

Packaging also exists under File > Pack staging manifest. Local native/ReShade assembly remains explicit; remote native/embedded Quest captures finalize automatically after confirmed stop and writer EOF. ReShade file transfer still needs its own producer completion handshake. Native analysis runs asynchronously in a separate process. Select the correct installed analyzer in Session; the executable is not bundled or linked into this AGPL application.

## Synchronization and truthfulness

See [the format contract](Docs/CaptureFormat.md). The plugin records CPU ticks and game-frame identities in `sync.jsonl`. Its game-thread samples of the collector's present counter are **approximate associations**, not proof of GPU execution correspondence. The current ReShade protocol has GPU durations and offsets, but no calibrated CPU/GPU clock pairs. Its lane therefore stays on a separate GPU clock; no artificial CPU alignment is shown.

Native unfinished scopes with an infinite end time are retained, labelled unavailable for duration and excluded from duration totals. Reconstructed pass names, dependencies and frame candidates are derived. Missing metrics stay unavailable rather than becoming zero.

## Quest / Android and consoles

On Quest, the plugin now embeds available Meta GPU/XR metrics **directly into the same native `.utrace`**, through `XR_META_performance_metrics`. It does not require OVR Metrics Tool, MQDH, Perfetto or RenderDoc to record these metrics. The Android-only `CyInsightsQuest` module registers before OpenXR instance creation and enables the extension optionally; an unsupported runtime does not prevent XR startup. It follows the Unreal OpenXR backend's instance/session lifecycle, discovers the actual counter catalog, and activates metric collection only while a trace and its `counters` channel are active. Debug, DebugGame and Development are allowed; Shipping, Test and Server exclude the module.

The option is enabled by default:

```text
CyInsights.QuestGpuMetrics 1
CyInsights.QuestGpuMetricsHz 10
CyInsights.Capture 5
```

`CyInsights.QuestGpuMetrics 0` disables polling. Polling defaults to 10 Hz and can be set between 1 and 120 Hz; it does **not** define the runtime's underlying measurement period. `CyInsights.Start`/`Capture` ensure the `counters` channel is included on Android even with custom channel selection. An existing trace started outside this coordinator also receives metrics when `counters` is enabled. Values appear in Epic Insights' Counters provider under `CyInsights/Meta/perfmetrics_meta/...`, with their native unit and `/available` companion counter. Depending on the runtime, the catalog can include app/compositor GPU time, CPU time, GPU utilization, dropped frames and other exposed metrics. Missing/failed/nonfinite readings are not replaced with zero durations.

The same trace contains `CyInsightsQuest.Source` and `CyInsightsQuest.Sample` events with raw flags/results, units, query begin/end CPU cycles and the **observed** game-frame counter. OpenXR does not supply the original GPU execution frame or measurement interval: these readings are not draw intervals or per-frame execution timestamps. This is embedded metrics collection, not a Vulkan command/shader/resource collector. Standard counters are imported and displayed in the application through the installed Epic export bridge, with native units and availability per value type. The dedicated raw custom-event decoder remains to be implemented. See [counter analysis and limits](Docs/CounterAnalysis.md).

In local capture mode the trace is saved on the headset in the project's `Saved/CyInsights/<session>` directory; file retrieval is a separate step. Remote capture streams the same native trace, including embedded metrics, directly to the PC and packages it automatically after stop; no monitoring application is required on the headset. The manifest is finalized on Stop and reports the metric source, observed valid sample count and unavailable execution-frame correspondence. Builds using an XR backend that does not invoke Unreal's OpenXR extension hooks report the collector unavailable and need a backend-specific adapter. Android module compilation and fake-runtime tests do not replace validation on an actual headset.

For a previously recorded native trace, including embedded metrics, package on the PC:

```powershell
python Tools/assemble_capture.py --utrace quest.utrace --platform Android --engine 5.5 --project MyQuestProject --output build/quest-staging
./bin/Release/CyCaptureTool.exe pack build/quest-staging build/quest.cycapture
```

Alternatively, retrieve the complete native staging directory and use `--native-staging` to preserve its manifest/synchronization metadata. Optional `--meta-csv metrics.csv` still accepts an external OVR report; its original columns are displayed without a report-version units/clock adapter. A Quest 3 is now detected by ADB, but this project's Android APK is not installed and no game was packaged/launched during the module compilation checks; hardware capture validation remains outstanding. Vulkan interception is a separate collector project; ReShade is not the Quest collector. Console adapters require the platform's authorized SDK and profiling interfaces.

To include the isolated fake-OpenXR-runtime tests, point `UE_OPENXR_INCLUDE_DIR` at an existing SDK header directory before `build.cmd Release --tests-only`, for example `<UE>/Engine/Source/ThirdParty/OpenXR/include`. The test suite checks lifecycle/ownership, runtime errors, validity flags, nonfinite readings, enumeration limits and native units; it does not load a headset runtime.

## Validation and remaining work

The current desktop release passes 452 checks across twelve suites and opens the previously failing 14.19-million-event trace through a disk index with parallel preparation. Earlier disk-index validation passed 249 checks across eight suites. The earlier compile-only checks did not link an application executable. The three base isolated test suites pass 69 checks covering archive/clock behavior, native CSV identities, background import and failures, nested scope relationships, clipped inclusive/exclusive statistics and indexed interval lookup. The optional Quest fake-runtime suite passes another 31 checks; the remote receiver adds 42 checks, 45 counter checks and 45 comparison checks bring the earlier total to 232 across seven suites; the disk-index suite now brings the total to 249 across eight suites. The same UE 5.5 network trace was analyzed with installed Epic 5.5–5.8 analyzers: the original timing scope count is 1,657,829, with 274 counters containing 1,997,547 samples. Newer exports also include measured frame rows, which are kept separate from timer identities and duration totals. See [counter validation](Docs/CounterAnalysis.md). A real two-second native network capture was packaged, analyzed by Epic Insights and imported successfully (1,499,153 timing events); 28 additional remote lifecycle checks passed against a test-owned Unreal 5.5 process. See [network validation](Docs/RemoteCapture.md). Android ARM64 Development module compilation passes for both the Quest collector and capture coordinator against UE 5.5–5.8; the UE 5.5 Win64 Runtime/Editor DLL regression build also passes. These are compilation checks, not a linked/packaged Quest application or headset validation. A read-only diagnostic loaded a real export with 2,754,493 events, 63 tracks, 8,491 timer identities and 549 derived tick candidates. This validates data calculations, not the rendered interface. Earlier prototype validation included packaged and displayed UE 5.5 native and D3D12/ReShade captures, including a detailed GPU frame with 311 draws and 147 dispatches. Runtime and Editor modules were compiled against installed UE 5.5, 5.6, 5.7 and 5.8. This does not yet validate packaged Debug/Development builds or every platform/API combination. Native CSV size and event-count caps have been removed; timings use streamed disk storage and asynchronous visible-window queries. See [large trace loading](Docs/LargeTraceLoading.md). See [implementation status](Docs/Status.md) for detailed boundaries.

## Licensing

CyGPUInspector-derived components retain **AGPL-3.0-or-later**; Dear ImGui retains MIT and the ReShade SDK/runtime retain their notices. See `LICENSE`, `LICENCES/` and `THIRD-PARTY-CyGPUInspector.md`. The engine, its source and its analyzer are not redistributed. Before distribution, include corresponding source for the staged derived components and the integration changes in `Tools/prepare_dependencies.py`, and reconcile notices with the components actually included.
