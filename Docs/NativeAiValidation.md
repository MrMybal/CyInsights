# Native AI migration verification

Local Windows x64 verification, 20 November 2026. No GitHub workflow was run.
CyMCPSDK 0.7.1: `34d4bcae126bc83d1a2925b5047dac36e3d09d92`.
CyUI 1.2.0: `6ef01cfcdb197f3a89d64618ce5ef8818c704728`.
Both source snapshots and the shared Dear ImGui override are hash-checked.

## Performed checks

- Fresh MSVC 2022 / C++20 / Ninja configuration, full application/CLI build and 32 CTest suites.
- Existing capture, native timing, counters, activities, tasks, comparisons, UI modules, Quest/Windows capture contracts and update checks retained.
- Existing `CyCaptureTool pack`, `inspect` and `extract` executed on a synthetic archive, including byte-for-byte binary payload recovery.
- CLI help, list (48 operations), describe, scriptable JSON, invalid-schema exit 2, unavailable instance exit 3, denied mutation/authentication exit 4, successful native CSV histogram.
- Five real app configurations: Free, Local controlled, Token only, Protected, and Protected with application control enabled. Native stdio MCP discovery, reads, mutation and denial; HTTP connection-file read and incorrect bearer-token rejection.
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
- Windows update/portable payloads and NSIS installer built locally. Archive manifests and file hashes verified. The extracted delivery CLI discovers 48 operations and measures the synthetic `Work` scope once at 4 ms.

## Local access profiles

The profile verification launches real application instances with `--ai-enable`
(no control permission flag) for Free, Local controlled, Token only and Protected,
then a Protected instance with `--ai-control`. Native HTTP MCP clients discover
and invoke tools against these processes. Address-only reads/mutations succeed
in Free; Local controlled reads work without a token while mutations are gated.
Token only and Protected reject missing or invalid tokens. Token only allows
mutations with its token; Protected also requires its application-control gate.
CLI connections by URL, private connection file and named pipe follow the same
rules. Free chat tests use no application-control flag.

Configuration checks cover the default, migration of a missing profile without
losing other options, all explicit profiles on disk, a live profile change and
invalid settings falling back to Protected without rewriting the original file.

## Current run

The Alpha 7 build passed all 32 local CTest suites, including the native
access-profile checks. The real-application profile matrix and existing
chat/CLI/MCP end-to-end fixture passed. The five showcase checks and
English/French reader checks also passed.

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
