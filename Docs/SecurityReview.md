# Executable alert investigation — 2026-10-08

Capture-profile revision, 11 October 2026: the rebuilt local application and optional Windows collector were scanned with Kaspersky, with iChecker/iSwift disabled for this scan. The scan completed at 13:49:47 local: 3 processed objects, zero detections, suspicions, skipped/corrupt objects or errors. Report: `build/light-profile-kaspersky.txt`. Owned UI tests opened the basic trace and exercised range filtering. No antivirus protections/exclusions were changed; the older alert remains unresolved. These binaries remain unsigned.

## Multithreaded revision, Release-20261009.2

Independent lanes, track indices and bounded CSV decoding batches use a reusable worker pool. Ordered ingestion/reduction preserves source identity and statistics; cancelled/failed batches drain running work before publication or cleanup. Nine suites pass 288 checks, and 180 benchmark windows have identical semantic fingerprints across worker counts. A real trace opens and renders, held navigation survives 120 model publications and the source remains unchanged. See [MultithreadedPreparation.md](MultithreadedPreparation.md).

The final unsigned executable's SHA-256 is `54a38ccd26ca5496de31ab03272303cd1163a86c3dc5b47c7e918a489c45616a`. Kaspersky's report-only scan completed at 19:39:59 local time on 9 October with zero detections, suspicions, skips, corruption or errors before launch. The original flagged file and antivirus settings were untouched; the earlier alert remains unresolved.

Evidence: [source manifest](../build/release-20261009.2-audit/source-manifest.json), [fresh build](../build/release-20261009.2-audit/build-full.log), [final build/tests](../build/release-20261009.2-audit/build.log), [scan](../build/release-20261009.2-audit/kaspersky-scan.txt), [opening/navigation](../build/release-20261009.2-audit/opening.json), [release manifest](../bin/Release-20261009.2/release-manifest.json).

## Graph performance revision, Release-20261009.1

The graph revision adds per-lane disk-block lookup, a reusable 32 MiB record cache and cached rendering/table work. All source timings and exact interval statistics are retained. Eight suites pass 266 checks. The 36-window before/after benchmark has identical display counts and native totals; the full overview represents all 14,192,325 events. See [GraphPerformance.md](GraphPerformance.md).

The final unsigned executable's SHA-256 is `32e5162e1cab035ee1b43493cc7a0648e9d7b3a20953c2133ae58192b6dfe703`. Kaspersky's report-only scan completed at 01:25:23 local time on 9 October with zero detections, suspicions, skips, corruption or errors before launch. The current source manifest and final opening/navigation results are preserved below. The original flagged file and antivirus configuration were untouched; its earlier alert remains unresolved.

Evidence: [source manifest](../build/release-20261009.1-audit/source-manifest.json), [fresh build](../build/release-20261009.1-audit/build-full.log), [final build/tests](../build/release-20261009.1-audit/build.log), [scan](../build/release-20261009.1-audit/kaspersky-scan.txt), [opening/interaction result](../build/release-20261009.1-audit/opening.json), [release manifest](../bin/Release-20261009.1/release-manifest.json).

## Navigation revision, 2026-10-09

The current `bin/Release-20261009/CyInsightsApp.exe` fixes continuous held gestures during asynchronous reads, aligns Frames to the visible timing axis and sizes tracks by visible depth. A new build directory compiled the sources, and eight suites pass 256 checks. The real 14,192,325-event trace test held navigation across 29 model replacements without interruption, verified frames/axis alignment, ruler dragging outside the widget and scope dragging, and reduced reserved rows from 118 to 55 for the tested interval. The source trace hash is unchanged and the application closed normally.

The final unsigned executable's SHA-256 is `f5a48f913b9ea79c0babec0466f13e28407723d48100551ba890a3a30303e833`. Kaspersky's report-only scan completed at 01:08:46 local time on 9 October with zero detections, suspicions, skips, corruption or errors before launch. Source hashes were checked unchanged afterward. The original flagged file and antivirus configuration were untouched; its earlier alert remains unresolved.

Evidence: [source manifest](../build/release-20261009-audit/source-manifest.json), [full build](../build/release-20261009-audit/build-full.log), [final build/tests](../build/release-20261009-audit/build.log), [scan](../build/release-20261009-audit/kaspersky-scan.txt), [opening/interaction result](../build/release-20261009-audit/opening.json), [real screenshot](../build/release-20261009-audit/ui-navigation.png), [release manifest](../bin/Release-20261009/release-manifest.json).

## Disk-index revision, Release-20261008.1

The revised application removes timing CSV-byte/event-count caps and streams native timings into an on-disk cache. It shows loading/cancellation/errors and publishes matching window events and models together; the latter fixes a crash found during the opening smoke test. The source-owned navigation harness verifies horizontal drag, vertical drag zoom and wheel zoom on a real 49,648,645-byte trace containing 14,192,325 events. The trace hash is unchanged. Eight isolated suites pass 249 checks.

The final executable is unsigned, 3,530,752 bytes, SHA-256 `31f205bcfebcc301537e62f658ed4d0e7dd7f93002b9d8a01c188e333a411a5c`. Kaspersky's report-only scan completed at 23:27:36 local time with zero detections, suspicions, skips, corruption or errors before launch. The old executable and AV settings were untouched. Ordinary MSVC debug symbols are retained for diagnosing crashes; release optimization and security mitigations remain enabled.

Evidence: [source manifest](../build/release-20261008.1-audit/source-manifest.json), [build/tests](../build/release-20261008.1-audit/build.log), [completed scan](../build/release-20261008.1-audit/kaspersky-scan.txt), [opening test](../build/release-20261008.1-audit/user-opening.json), [loading](../build/release-20261008.1-audit/ui-user-loading.png), [render](../build/release-20261008.1-audit/ui-user-trace.png). This scan does not resolve the old alert or certify general safety.

The same executable then opened a 1,403,637,916-byte trace, indexed all 353,528,278 events into an 11,312,904,904-byte cache, rendered and passed the same navigation checks before closing normally. The source hash remains unchanged. Its approximately 1,050-second first opening exposes the remaining CSV bridge cost; it is not a performance guarantee. See [large opening result](../build/release-20261008.1-audit/large-opening.json).

## Rebuilt test release, 22:47–22:49 local time

The user explicitly requested a new release after Windows Restart Manager identified PID 5864, `Kaspersky Service 21.26` / service `AVP21.26`, as the owner of the original executable's lock. The old `bin/Release/CyInsightsApp.exe` was not modified, restored, renamed or launched, and its detection remains unresolved.

The current application sources were compiled from a fresh `build/Release-20261008` directory into the versioned `bin/Release-20261008` directory, retaining the `CyInsightsApp.exe` basename. This separates build artifacts and preserves the original for investigation. No binary packing, obfuscation, AV exclusion, protection change or self-signed trust certificate was introduced. The compiler, linker and CMake binaries have valid Microsoft signatures; the scanning CLI has a valid Kaspersky signature. All 59 staged dependency files were checked against the existing lock; only the two documented archive integration patches differ, and their complete diff was recorded. One hundred build inputs were hashed before compilation and checked unchanged afterward.

MSVC `/sdl`, `/guard:cf` and linker `/DYNAMICBASE`, `/NXCOMPAT`, `/HIGHENTROPYVA`, `/guard:cf` are enabled. PE headers confirm ASLR, high-entropy addressing, NX and CFG. These are ordinary mitigations, not a claim about the cause of the old detection. The new application is unsigned and is 1,647,104 bytes. SHA-256: `ec840978db8e0169b7242991b98a466514d35cfc25259755dc99da82afbc9b78`.

The installed Kaspersky CLI scanned this exact new executable at 22:47:15 with bases dated 2026-10-08 19:03:00. Although the CLI printed an action-parameter warning for non-interactive mode, the report records **Report only**, no disinfection and no deletion. The scan completed with **2 processed objects, 2 OK, 0 detections, 0 suspicions, 0 skipped, 0 corrupted and 0 errors**. This is a completed scan result for this file; it does not certify future behavior or establish that the original detection was a false positive.

After that scan, the new application was launched for a bounded snapshot test against the existing UE 5.5 `.utrace`. It loaded 1,657,829 timing scopes, exported and imported native counters through the installed Epic analyzer, rendered the revised layout and closed normally. All seven clean-built console suites passed 232 checks. The snapshot was inspected; broader UI interactions and hardware GPU capture were not retested.

Evidence: [build log](../build/release-20261008-audit/build.log), [dependency diff](../build/release-20261008-audit/dependency-patches.diff), [source manifest](../build/release-20261008-audit/source-manifest.json), [PE headers](../build/release-20261008-audit/pe-headers.txt), [PE imports](../build/release-20261008-audit/pe-imports.txt), [Kaspersky scan](../build/release-20261008-audit/kaspersky-scan.txt), [real UI screenshot](../build/release-20261008-audit/ui-native.png), [release manifest](../bin/Release-20261008/release-manifest.json).

## Observed alert and limits

The user supplied a Kaspersky Standard screenshot identifying `CyInsightsApp.exe` in this project's Release directory as an active threat: `VHO:Trojan-PSW.Win32.ReaderDB.gen`. No additional event details or SHA-256 were available. This is an unresolved detection, not a confirmed false positive.

The existing `bin/Release/CyInsightsApp.exe` has a size of 1,324,032 bytes and a last-write timestamp of 2026-10-08 15:18:57 (local Europe/Paris time). Attempts to read its hash and Authenticode information failed because the file was in use. The lock owner could not be established. No running CyInsightsApp process was observed during inspection. Binary integrity and the cause of detection therefore cannot be established from this review.

## Checks performed

- Installed MSVC 14.40 `cl.exe`, `link.exe` and Visual Studio's bundled CMake have valid Microsoft Authenticode signatures. The installed Kaspersky `avp.com` has a valid AO Kaspersky Lab signature.
- At the time of the initial inspection, application and consumed Core/Session/Analysis sources contained no credential-store API, network client or remote-process injection primitive. The subsequently added remote-capture client is described separately below. Legitimate process operations include launching the installed Unreal analyzer, terminating an owned analyzer on teardown, and querying collector process liveness. This is a bounded source review, not proof that the inaccessible binary matches these sources or is safe.
- Fifty-nine consumed staged dependency source files were compared with their recorded upstream hashes. Differences were confined to the known `CaptureArchive.hpp/.cpp` integration changes. Upstream provenance and per-file hashes are retained in `DEPENDENCIES.lock.json`; the original CyGPUInspector repository was not changed.
- A separate source issue was fixed: shader signatures read from GPU capture metadata must parse as SHA-256 before constructing the shader filename. This prevents using a metadata signature as an arbitrary relative path. The preparation script reproduces the fix. This issue does not explain or resolve the antivirus detection.

## Report-only scan

The signed, installed Kaspersky CLI was invoked against only the flagged file with `/i0` and `/RA:build/kaspersky-scan-report.txt`. Its report header confirms `Action on detect: Report only`, `Try disinfect: No`, `Try delete: No` and `Try delete container: No`.

The scan task `Scan_Objects$11038` started at 15:46:28 and the CLI stalled without a file verdict. Stopping this specific task through the CLI required a Kaspersky login and was refused; no credentials were requested or supplied. The waiting CLI was interrupted with Ctrl+C. The report has only its settings and startup records, with no completed verdict. Whether the underlying scan task subsequently completed is unknown.

Report: [kaspersky-scan-report.txt](../build/kaspersky-scan-report.txt).

## Development state before the rebuilt release

No replacement application executable has been linked or launched during this investigation. The UI revision is compiled only into C++ object files and static libraries using `build.cmd Release --compile-only`. Separately, isolated import/calculation test programs were built and executed using `build.cmd Release --tests-only`, with 62 checks passing. A read-only diagnostic processed an existing native CSV export through the new model. Those initial tests did not include or launch the application UI, a collector, or an Unreal analyzer process. They validate data calculations and background CSV import, not the application's visual appearance or the flagged binary's safety. The sample Unreal configuration sets `bLaunchStandalone=False` to prevent automatic application launch.

No antivirus exclusions, protection changes, quarantine restoration, sample uploads or executable renaming were performed. Runtime validation remains pending resolution of the detection and verification of the executable's provenance. Existing screenshots and runtime tests refer to the earlier prototype, not the revised interface.

## Subsequent network and counter development

The current sources include an authorized Winsock TCP capture client/receiver (default command port 41855), described in [RemoteCapture.md](RemoteCapture.md). Counter analysis launches only the selected installed Epic analyzer without UI and saves exports into its local analysis cache. Process cancellation/timeout is restricted to its owned analyzer. These additions are ordinary profiling functionality and do not establish why the earlier binary was flagged.

Six isolated console suites now pass 180 checks. Separate source-owned console diagnostics launched installed Epic analyzers 5.5–5.8 with `-NoUI` on an existing trace; the application UI and flagged executable were not launched, relinked, renamed or replaced. Android checks compile only plugin objects. These validations do not provide an antivirus verdict or visual/runtime verification of the revised application.

The subsequent native A/B comparison uses the same selected installed Epic analyzer with `-NoUI` and local cache exports. Seven console suites pass 232 checks; cancellation is verified against a source-owned fake analyzer process. Separate console diagnostics analyzed existing network captures without launching or relinking the application UI. This does not change the unresolved antivirus verdict.

## Release-20261009.3

Fresh source build into its own versioned directory. Kaspersky report-only scan completed at 2026-10-09 20:08:22 local: zero detections, suspicions, skipped/corrupt objects or errors. The executable was then launched in an owned UI smoke test, opened a real native trace, rendered frame diagnostics, validated navigation and closed normally. Its source SHA remained unchanged. No protections or exclusions were changed. The new binary is unsigned; the old alert remains unresolved. See [FrameDiagnostics.md](FrameDiagnostics.md) and the versioned release manifest.

## Release-20261009.4

Fresh source build of the application and four optional native task adapters. Kaspersky report-only scans completed at 2026-10-09 20:56:06 local for the new release directory, with zero detections, suspicions, skipped/corrupt objects or errors. The adapters launch only as owned child processes, read the selected trace and use the installed Epic DLLs; no engine DLLs are redistributed. Task indexing publishes only a completed export. Cancellation terminates only the owned decoder. The 436 component checks cover identities, paging, missing phases and cancellation. The new application and adapters are unsigned. Protections, exclusions and the old alert were not changed. See [TraceAnalysis.md](TraceAnalysis.md) and the new release manifest.

## Release-20261009.5

Fresh application/tool build into its own versioned directory; the unchanged 5.5–5.8 task adapters are retained with verified hashes from release 9.4. Kaspersky report-only scanning of this directory completed at 2026-10-09 21:30:31 local with zero detections, suspicions, skipped/corrupt objects or errors before the final application interaction tests. The 452 component checks and owned ImGui interaction tests cover indexed function queries, histogram summary reuse and selection changes. Function indexes and caches are derived local data; no native trace records are discarded. Source traces remain unchanged. Protections and exclusions were not changed; the old binary's alert remains unresolved. This release is unsigned. See [TimelineFunctionAnalysis.md](TimelineFunctionAnalysis.md) and its release manifest.

## Release-20261009.6

Fresh native source build with a CyToolsCore 0.10.1 companion and pinned official MCP SDK 1.30.0. The native transport restricts its named pipe to the application's Windows user and rejects remote pipe clients. Application control, annotations and remote capture are governed by SDK permissions and native UI gates; normal startup leaves the bridge/control/capture disabled. Persistent trace documents are read-only references, while generated reports belong to private SDK jobs. No source trace is uploaded by the adapter. External AI findings are plain text hypotheses tied to the current trace generation.

Kaspersky report-only scanning of the product directory completed at 2026-10-09 22:00:53 local: 7727 objects processed, zero detections, suspicions, skipped/corrupt objects or errors. The native executable, official wheel packages and installed private environment were scanned. Subsequent manifest/documentation changes do not replace native binaries or wheels. Owned UI and official MCP tests open the native trace, control intervals, cancel analytical jobs and exercise a synthetic local capture endpoint. The original trace remains unchanged. The release remains unsigned; no antivirus protections or exclusions were changed and the old alert remains unresolved.

## Release-20261010.1

Fresh source build with Windows ICON/PNG resources for the user-approved logo. The PNG is decoded once with Windows Imaging Component and uploaded to D3D11; it is not read from an external path at startup. Resource bytes and native Windows icon extraction were validated against the approved derived assets. No new external image decoding dependency was bundled.

Kaspersky report-only scanning of the release directory completed at 2026-10-10 00:07:52 local: 5551 objects processed, zero detections, suspicions, skipped/corrupt objects or errors. Owned application tests then rendered the branding and opened an existing trace normally. Protections and exclusions remain unchanged; the old alert remains unresolved. The release is unsigned. The existing AI companion and unchanged native task readers are retained with their notices and source hashes.

Per-frame Quest revision, 11 October 2026: the rebuilt local `bin/Release-20261011/CyInsightsApp.exe` passed a completed Kaspersky CLI file scan at 16:06:33 (2 processed objects, 0 detections, suspicions, skips or errors). Report: `build/quest-per-frame-kaspersky.txt`. The native capture-options snapshot then ran and exited normally. No antivirus settings/exclusions were changed, no binary was uploaded, and the earlier detections remain unresolved.

After the paged GPU-lane ordering fix, the final rebuilt executable was scanned again at 16:12:29: 2 processed objects, 0 detections, suspicions, skips or errors (`build/quest-per-frame-final-kaspersky.txt`). It then opened the real rendered native trace, displayed Graphics/Compute above GameThread and exited normally via its owned screenshot harness.

Default GPU group revision: the rebuilt local app completed a Kaspersky file scan at 16:40:44 on 11 October 2026 (2 processed objects; 0 detections, suspicions, skips or errors; `build/quest-gpu-groups-kaspersky.txt`). The options and real native timeline screenshot harnesses both exited normally. No protection or exclusion was changed, and no executable was uploaded. This scan does not resolve the earlier detections or guarantee future antivirus verdicts.
## Vérification locale du collecteur intégré — 11 octobre 2026

Le backend optionnel `CyInsightsGpu.addon64` est compilé à partir des sources MIT
conservées dans ce dépôt. Le plugin CyGPUInspector séparé a été sorti du packaging ;
aucun code de lancement/recherche de son application n'est repris dans les nouveaux
modules. Sessions et exports sont propres à CyInsights. L'audit du working tree
(515 fichiers, dépendances verrouillées et nouvelles sources incluses) passe sans
secret ni chemin machine détecté. Aucune publication distante n'a été effectuée.

Le scan Kaspersky de l'application recompilée et du backend intégré a terminé avec
3 objets traités, 0 détection, suspicion, saut ou erreur. Rapport local :
`build/integrated-gpu-kaspersky.txt`. Ce résultat concerne ces fichiers précis et
ne résout pas rétrospectivement les alertes des anciens exécutables. Aucun réglage
de protection ni exclusion antivirus n'a été changé.


- 2026-10-11, Quest timestamp revision: publication audit of 521 working-tree source files passes. Targeted Kaspersky scan of the newly rebuilt local CyInsightsApp.exe completed at 20:55:43 (2 objects OK, 0 detections/suspicions/skips/errors). No exclusions or protection settings changed. This describes this scan only.


## Local subpass-content build, 12 October 2026

The new native detail reader uses a read-only mapping of an explicitly opened
trace's disposable `.cyvp` analysis cache. Its block index, GPU identities and
quantities do not read credentials, arbitrary engine installations, textures
or indirect GPU buffers. Source publication checks are recorded separately.
Kaspersky reported a cloud heuristic detection `VHO:Backdoor.Win64.Agent.gen`
for the new `PassContents-20261012/CyInsightsApp.exe`, attributed to the linker
before application launch; the file was subsequently absent. The exported event
does not establish the precise removal/quarantine action. It was read from the product's
file/behavior reports. This is unresolved and does not establish a false
positive. No exclusions, protection changes, quarantine restoration or binary
renaming workaround were performed. The earlier local build is retained.
UI visual validation and delivery of the new app binary remain blocked.

## Clean source build and delivery, 13 October 2026

A new source build in a fresh CMake/Ninja directory writes to the same
`PassContents-20261012/CyInsightsApp.exe` path. It includes the implemented
subpass UI, explicit Meta driver/packet diagnostics and ordinary Windows
version/publisher resources. No packing, obfuscation, antivirus exclusion,
protection change, quarantine restoration or executable rename was used.
The metadata is not a digital signature; no usable code-signing certificate was
found and no self-signed certificate was created or trusted.

The new executable SHA-256 is
`24ad3604e1d068725c8f62062dd4cabab3e0f463611d047dcf3c1c645fcba76d`.
Kaspersky scans completed on this file and then its product directory: two and
155 objects processed respectively, zero detections, suspicions, skips,
corruptions or errors. Reports are under ignored `build/` as
`security-verified-kaspersky.txt` and `security-verified-directory-kaspersky.txt`.
The file hash remained identical after startup tests. The earlier detected binary
was unavailable for hashing; this scan does not classify the earlier event as a
false positive, prove its cause or guarantee future antivirus verdicts.

The new app opens normally and renders three explicitly synthetic subpass
records from the native Unreal reader. A real named-pipe test exercises the
new readonly query, pagination, selected intervals and unavailable GPU costs.
All 21 CTest suites, eight SDK tests and eleven Python tests pass. Visual and
runtime delivery of this new binary is therefore available locally, superseding
the preceding build's blocked delivery. No new Beyond launch or recording was
performed; read-only Quest queries confirmed driver state and source advertising.

[Windows signing](WindowsSigning.md) describes trusted-certificate signing,
SHA-256/RFC3161 verification, actual per-file signature manifests and the manual
GitHub provenance workflow. Attestations and signatures do not certify malware
absence. No upload to Kaspersky or publication of a new GitHub release occurred.

## Capture-start regression correction, 13 October 2026

The new `RemoteFix-20261013` local app restores capture of an already-running
Quest game by default. The existing driver profiling mode is queried and
preserved; no reactivation, game termination or launch is issued unless the user
explicitly selects restart. The optional restart resolves the selected package's
launcher before stopping it, then waits for its new capture-service identity.
Startup stop/cancellation and local phase diagnostics are documented in
[RemoteCapture](RemoteCapture.md). No game was started/stopped during development.

Kaspersky scanning of the new product directory completed on 13 October: 152
objects, zero detections, suspicions, skips, corruptions or errors. The app
SHA-256 is `b5745d1430090853478b0aa77a75dc660d04943e23fdb68f74044947e95c7b30`.
It remains unsigned. The UI startup/capture-options test passed in an isolated
local profile. An earlier sandboxed UI test could not write the user's global
engine registration; the isolated test preserves those settings and confirms
native reader readiness. The existing running user app was not terminated.
Twenty-one CTest suites, 45 Meta/TCP startup checks, 77 existing TCP checks, eight
SDK tests and eleven Python tests pass. Plugin-only Android builds succeed on
Meta UE 5.5.3 and UE 5.8.3. A fresh real Beyond capture remains unverified.
