# Native timing interface revision

The C++ application uses Dear ImGui with a custom vector toolbar and timing canvas. It contains no browser, web view, HTML or JavaScript interface. GPU command views retain the collector's data model while the application layout and timing views are implemented independently.

## Interaction

| Action | Behavior |
| --- | --- |
| Frames click | Select a native game frame or an explicitly derived tick candidate, on the visible time axis |
| Mouse wheel over tracks | Zoom around the pointer |
| Middle-button drag | Pan the timing axis |
| Shift + mouse wheel | Scroll vertically through tracks |
| Left-button drag on overview or time ruler | Horizontal movement pans; vertical movement zooms |
| Mouse wheel on overview | Zoom the displayed interval |
| Left-button drag over scopes | Pan while holding, including during asynchronous window updates |
| Shift + left-button drag on time ruler | Select an interval for statistics |
| `< Frame` / `Frame >` or left/right arrows with Timing focused | Select the previous/next tick candidate and display surrounding ticks |
| `F` with Timing focused | Fit the selected interval; otherwise focus three ticks |
| Escape with Timing focused | Clear the selected interval |
| Scope click / double-click | Select its timer / zoom onto the scope |
| Track header click | Collapse or expand that track |
| Timer column header click | Sort by name, count, inclusive or exclusive cost |
| Timer double-click or `Find in timing view` context action | Focus its nearest complete scope, reveal the track and scroll to its depth |
| Native timer context menu | Show the exported TimerId when available |
| Counter double-click / Show in timeline | Add or remove a continuous value track |
| Counter column header click | Sort interval sample count, extrema, derived mean, name or unit |
| Compare / Analysis > Compare native capture | Load a reference capture B without replacing A |
| Comparison double-click | Focus A when its name/domain group contains only one timer |

Custom glyphs, toolbar dimensions, ruler, track rows and scope labels scale with font size. Dear ImGui's Win32 DPI support is enabled. Rendering and movement between monitors still require runtime validation.

## Data behavior

For timing scopes, CSV ThreadId and TimerId define native identities. Newer export frame rows use a reserved frame track and put a frame ordinal in the TimerId column: the importer recognizes that separate namespace, preserves the full index from the frame label, and excludes these rows from timer costs and scope comparison groups. Same-name threads/timers with different IDs remain separate; duplicate thread labels include their ID. Legacy CSV exports without IDs use name-based grouping, and timer identity is shown as unavailable. Caller/callee edges and exclusive durations are derived only from complete, contained, immediate-depth scopes on the same thread. Open-ended scopes are retained for display but excluded from measured duration totals.

The timeline uses continuous timestamps across all tracks. Tick selection changes the visible interval and clipped statistics; it does not split rendering/worker scopes into per-frame records. Scopes beginning before or ending after the viewport retain their real timestamps and continuation flags. The time ruler and overview use measured native Game frame indices when the exporter supplies them. Otherwise they display engine tick guides labelled as derived candidates. Native frame indices remain local to the trace and do not establish GPU execution-frame correspondence.

All depth levels overlapping the visible interval are displayed by default. Track height follows the deepest visible scope, retaining its original depth and hierarchy. Deep scopes elsewhere in the capture no longer reserve empty rows. Inactive tracks retain only their header; hiding those headers or limiting depth remains an explicit View Mode setting. Track order remains stable across disk-window replacements. Tracks can be collapsed individually or together. A visible depth limit reports the hidden level count.

The Frames chart uses the same visible start/end and horizontal coordinates as the timing canvas. Its bars cover actual frame intervals, including frames crossing a viewport boundary. At wide zoom, subpixel frames are counted and the tallest duration is shown with explicit derived-display provenance. The earlier whole-capture overview no longer stays stationary above a moving detailed timeline.

Held navigation is owned until the mouse button is released, including outside the original widget. Interval reads complete and the latest requested view is queued, with nearby scopes prefetched. Navigation no longer cancels each read at every pointer movement. The update indicator stays in the toolbar; it does not create a focus-taking loading window. Initial capture loading and errors retain their visible panel. Interval statistics say they are updating when the displayed results belong to the previous interval.

At a wide zoom, subpixel scopes in the same depth row and pixel column are grouped. The tooltip states their count, execution range and any unfinished scopes; double-click zooms to resolve them. This is a derived display representation, not a unique measured scope or object association. Selection and search matches are retained across all group members. A zero-duration record remains visible as a point. Interval statistics continue to use the underlying native records.

CSV import and timing-model construction run in background tasks that own their inputs. Completion publishes a complete result on the UI thread. Opening/reanalyzing another capture is prevented while those tasks run. Parsing exceptions and failed imports return an error and release the busy state. The indexed timing model uses a separate interval lookup at each depth, avoiding the effect of a long outer scope on all deeper scopes. Visible events are cached until the interval or depth setting changes. Interval statistics reset costs from the prior active set and query only overlapping scopes.

The CSV byte and event-count caps have been removed. Native timings are streamed into a disk index and loaded by visible window; display aggregation retains counts and interval statistics use full records. Loading shows phases, elapsed time, export bytes, indexing progress and cancellation; errors remain visible. Archive extraction is synchronous after the loading panel is first painted. Counter values are loaded on demand; see [CounterAnalysis.md](CounterAnalysis.md) and [LargeTraceLoading.md](LargeTraceLoading.md). Native logs and the remaining Insights providers are pending.

## Verification on 2026-10-08

- UI compilation into object files and static libraries succeeded without compiler diagnostics.
- Three isolated C++ test programs passed 62 checks (17 archive/clock, 13 native import, 32 timing-model/display checks). These cover cross-tick CPU/render/worker scopes, stable depth rows, continuation geometry, subpixel count conservation, search/selection retention and resolution after zooming. Interval index results and clipped totals were also checked against brute-force references across 100 deterministic windows.
- A diagnostic invocation read the existing UE 5.5 export: 2,754,493 events, 63 tracks, 8,491 distinct TimerIds and 549 derived tick candidates. One observed run took 3,790 ms to read the CSV and 257 ms to construct the timing model. The interval calculation and visible-scope query for three central tick candidates took 5.06 ms and returned 13,937 scopes at 24 depth levels. The whole continuous capture at all depth levels was represented by 82,287 glyphs at 1,600 pixels width, retaining all 2,754,493 source scopes. These are calculation observations from this machine, not screenshots or performance guarantees.

The MSVC/Ninja header dependency issue discovered during testing is fixed: CMake's corrupted French `/showIncludes` prefix is normalized, and `Tools/check_msvc_dependencies.py` verifies the compiler's raw prefix before each build. Stale object files were removed and components rebuilt. Ninja's stored dependency records now include the timing headers. Python 3 is required for this build check.

Logs: [compile-only](../build/ui-compile-only.log), [tests](../build/timing-tests.log), [real export](../build/timing-real-export.log).

Those initial checks did not relink or launch the application. Subsequently, at the user's request, the current sources were rebuilt into `bin/Release-20261008/CyInsightsApp.exe` using a fresh build directory. Its completed Kaspersky scan reported zero detections, suspicions, skips or errors. The revised UI opened an existing real `.utrace`, loaded 1,657,829 timing scopes plus native counters, rendered the frame overview, continuous nested tracks and sidebar, saved [a real screenshot](../build/release-20261008-audit/ui-native.png), and closed normally. This is an opening/rendering smoke test; input interactions, detached windows, monitor changes and GPU hardware paths have not been retested. The new build's seven isolated suites pass 232 checks. The original executable's alert remains unresolved; see [SecurityReview.md](SecurityReview.md).

## Native A/B comparison

The disk-index release `Release-20261008.1` additionally opened real traces of 49.6 MB / 14,192,325 events and 1.4 GB / 353,528,278 events. Both rendered and closed normally with unchanged source hashes. The source-owned ImGui input harness verifies horizontal pan, vertical drag zoom and wheel zoom on both; the index for the larger capture exceeds 4 GB. See [LargeTraceLoading.md](LargeTraceLoading.md) for exact sizes, processing time and evidence. Detached windows and monitor transitions remain unverified.

The independent comparison pane now compares native CPU/GPU scope groups using A’s selected/visible interval and a separately chosen B interval. Name/domain matches retain candidate or ambiguous provenance, including native IDs local to each session. Costs can be totals, normalized per second or averaged per clipped occurrence. Reference extraction/analysis/range changes run asynchronously and preserve the previous B on failure/cancellation. See [CaptureComparison.md](CaptureComparison.md) for data semantics and verification.
