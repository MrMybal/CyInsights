# Native AI migration verification

Local Windows x64 verification, 19 November 2026. No GitHub workflow was run.
CyMCPSDK 0.7.0: `791d4d7d7680213b282ec8529d248a10f8494d77`.
CyUI 1.2.0: `6ef01cfcdb197f3a89d64618ce5ef8818c704728`.
Both source snapshots and the shared Dear ImGui override are hash-checked.

## Performed checks

- Fresh MSVC 2022 / C++20 / Ninja configuration, full application/CLI build and 32 CTest suites.
- Existing capture, native timing, counters, activities, tasks, comparisons, UI modules, Quest/Windows capture contracts and update checks retained.
- Existing `CyCaptureTool pack`, `inspect` and `extract` executed on a synthetic archive, including byte-for-byte binary payload recovery.
- CLI help, list (48 operations), describe, scriptable JSON, invalid-schema exit 2, unavailable instance exit 3, denied mutation/authentication exit 4, successful native CSV histogram.
- Two real app processes: read-only and control-enabled. Native stdio MCP discovery, reads, mutation and denial; HTTP connection-file read and incorrect bearer-token rejection.
- Real SDK Chat Completions streaming protocol fixture: progressive response, native filter mutation on the real application, cancellation and history restoration without replay.
- Portable text attachment delivered through the API and restored after deleting its test source file.
- Actual app render-loop smoke: captures of the displayed partial response and cancellation; proof also checks the actual function filter changed.
- Explicit remote MCP allowlist: real HTTP MCP success and tool error through chat; unknown tool rejected and a revoked connection denied.
- Durable job completion, cancellation, owner isolation, restart recovery without replay, old JSON report import without altering the original.
- Multi-megabyte UTF-8 disk-spooled content reconstructed from bounded chunks; expired reference rejected.
- HTTP resource cache hits, then change-subscription invalidation before TTL.
- Actual operation telemetry excludes test input/output markers; context projection preserves full stored history, system instructions and active turn.
- Native provider discovery returns 23 profiles without exposing credential fields.
- Generated English/French offline documentation, native embedded contracts and translations.
- Clean Windows update/portable payloads and NSIS installer built locally. Archive manifests and file hashes verified; no Python runtime/wheels, CyToolsCore, test/user data or trace files are shipped. The extracted delivery CLI discovers 48 operations and measures the synthetic `Work` scope once at 4 ms.

## Commands

Use the normal `build.cmd` MSVC build or a clean CMake/Ninja build. After building:

```powershell
ctest --test-dir build/NativeMcpClean --output-on-failure
python Tests/NativeAiE2E.py --binaries bin/NativeMCP --tests build/NativeMcpClean/CyNativeAiTests.exe --output build/NativeAiVerification
python Tools/prepare_dependencies.py
python Tools/check_publication.py --worktree
```

The fixture is a protocol/render test, not an inference model. It does not use a paid account or touch the headset/game. Production model authentication, generation quality and other desktop platforms require separate validation. The Windows UI was rendered and captured by the application test harness; interactive mouse/keyboard control of the new executable was unavailable in this environment.

See [integration, transports, permissions, migration and remaining SDK limits](AiIntegration.md).
