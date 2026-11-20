# Native assistant, CLI and MCP

CyInsights embeds **CyMCPSDK 0.7.1**, revision
`34d4bcae126bc83d1a2925b5047dac36e3d09d92`, compiled from the local source
snapshot in `ThirdParty/CyMCPSDK`. No Python runtime or CyToolsCore installation
is required. The Unreal recording plugin and Quest APK remain separate; this
integration concerns the Windows desktop profiler.

## Integrated assistant

Open **AI Inspector > Assistant > Connection and options**. Detect installed
providers and local services, select a provider and an explicit model ID, then
connect. Supported host-tool paths include Codex and the API/local-service
profiles OpenAI, Anthropic, OpenRouter, Ollama, LM Studio, llama.cpp and
OpenAI-compatible Chat Completions. Other SDK profiles can report that their
native transport needs its own MCP configuration; presence in discovery does
not imply host-tool support. Existing service installations and authentication
are reused. No weights, Node, Python or inference engine are downloaded.

The official **CyMCP::ChatUI** panel renders streamed Markdown, tool activity,
provider errors, cancellation and attachments. Use an environment variable or
a memory-only API key. Only non-secret provider/model/endpoint preferences are
saved. Attachments and conversation content are private user data. Choose a
stable conversation ID and enable Restore to resume saved history without
resending a turn. Model capabilities determine supported attachments and history
editing. Link opening requires a click; there is no automatic remote preview.
CLI agents retain native context and read-only sandbox defaults; the application
does not approve their terminal/filesystem actions automatically.

## Shared operations and permissions

The 39 existing business operations retain their names and native handlers:
state/capabilities/method/metadata; trace opening; interval/filter/view controls;
thread/function discovery, hotspots, distributions, frames, scopes and relations;
counters/frame statistics; tasks/dependencies; activities; A/B comparison;
GPU commands/Vulkan details; findings; remote capture and cancellation of loads.
Their typed contracts are in `AI/app/operations.json`. Nine native utilities add
job start/status/result/list/cancel/delete/import and resource read/grep.

The same registry serves the assistant, console and MCP. Trace generations are instance-specific decimal identities: restoring a conversation never authorizes a finding from a previous process/document. Rediscover get_state after opening or restoring. Native Tools and the
guided method also invoke it. App actions are marshalled to the UI thread;
queries use the existing immutable indexes on workers. No large trace copy is
made for a job. UI-owned data, calculations and selection remain in CyInsights.

**Free is the default access profile.** Enable the local bridge, copy its HTTP
URL and connect using that address alone. It uses
`local_access_settings(LocalAccessProfile::free)`, `Policy::allow_all()` and the
SDK's `UnauthenticatedLoopback` server bound to `127.0.0.1`. All exposed business
operations are allowed without another confirmation; schemas, trace availability,
recording ownership and other technical preconditions still apply.

Choose a profile in **AI Inspector > Connection > MCP access profile**:

| Profile | HTTP token | Operation control |
| --- | --- | --- |
| Free | Not required | All exposed operations |
| Local controlled | Not required | Application/capture gates |
| Token only | Required | All exposed operations |
| Protected | Required | Application/capture gates |

Existing options without `accessProfile` migrate to `free` while keeping other
settings. Explicit `free`, `local_controlled`, `token_only` and `protected_access`
choices survive restarts. Invalid or unreadable explicit settings fall back to
Protected without rewriting the original file. Selecting a profile saves it and
restarts a running HTTP endpoint; copy its new address afterwards. Enabling the
bridge remains an explicit action.

In Local controlled and Protected, **Allow AI application control** and
**Allow AI remote capture** remain independent, per-document gates, initially off.
The same operation rules apply to chat, CLI, named-pipe and HTTP/MCP calls.
Explicit manual Tools actions are trusted user actions. External clients and
the assistant never receive that manual identity from their arguments. Capture
start/stop uses its existing ownership/state checks. Cancelling a query is
cooperative; an accepted UI change is not undone and recording is not stopped.
Use `capture_stop` explicitly. Missing recorded channels remain unavailable.
Shader recompilation, project/engine modification, raw shell execution, native
log parsing and incoming-trace decoding are not exposed as fabricated tools.

## Showing evidence in the interface

With Free/Token only, or **Allow AI application control** enabled in a controlled
profile, the assistant can select and focus
a time interval (`set_range`), highlight an exact function (`filter_function`),
open its distribution (`show_histogram`), show an analysis panel (`focus_view`),
pin a recorded counter curve (`pin_counter`) and publish a trace-bound finding
(`publish_finding`). For example: "Find the slowest frames, show me the interval
and display the relevant counters." Read-only analysis remains available when
control is disabled in a controlled profile. In those profiles remote recording
requires its separate capture permission.

Access is through the declared business operations, not arbitrary mouse/keyboard
control. The assistant cannot manipulate every button, browse arbitrary files
or automatically inspect every screenshot, texture or tile heatmap. Opening a
supported trace by an explicit path and adding a chosen attachment are distinct,
supported actions. Missing capture channels cannot be reconstructed by the model.
Use `get_capabilities` and the operation schemas for the current scope.

## Console

`CyInsightsCli.exe` ships beside the application. It emits JSON on stdout and
sanitized diagnostics on stderr. `--help` describes its syntax. Existing
`CyInsightsApp.exe` options and `CyCaptureTool pack/inspect/extract` are preserved;
`--ai-runtime-install` now opens the native connection page (no runtime download).

```powershell
.\CyInsightsCli.exe list
.\CyInsightsCli.exe describe histogram
.\CyInsightsCli.exe invoke get_state --url http://127.0.0.1:8765/mcp
.\CyInsightsCli.exe invoke get_state --pid 1234 --document 0
.\CyInsightsCli.exe invoke histogram --pid 1234 --json '{"function":"Work","limit":20}'
.\CyInsightsCli.exe invoke filter_function --pid 1234 --json '{"function":"Work"}'
.\CyInsightsCli.exe invoke histogram --trace timing.csv --json '{"function":"Work"}'
.\CyInsightsCli.exe providers
```

Use `--params-file parameters.json` instead of `--json` for complex requests.
The PID and document ID are shown in **AI Inspector > Connection**. Each document
has its own bridge, even in detached windows. Headless `--trace` accepts an
exported timing CSV and builds/reuses the real native disk index. It supports
state, native timing analyses and scope/frame queries; GUI/capture/provider-load
actions need a connected instance. Native `.utrace`/`.cycapture` decoding uses
an open instance and its licensed Unreal reader. Headless analysis is read-only.
Exit codes: 0 success, 2 input/schema, 3 unavailable instance, 4 denied,
5 operation failure, 130 cancelled. Ctrl+C cancels the current request.

## External MCP

Enable the bridge in the target document and copy its generated client JSON:

```json
{"mcpServers":{"CyInsights":{"command":"CyInsightsCli.exe","args":["mcp","--pid","1234","--document","0"]}}}
```

Use the actual absolute command path shown by the app. This is the SDK's native
stdio MCP server, connected through the existing owner-only Windows named pipe.
Remote named-pipe clients are rejected. The default compact catalogue exposes
`cy_tools_search`, `cy_tools_describe`, `cy_tools_call`, `cy_resources_read` and
`cy_resources_grep`. Discover any of the 48 operations through those tools.
Append `--full` to expose every operation directly. Operation calls return their
native structured result, rather than the old Python job envelope.

The HTTP MCP server listens only on `127.0.0.1`. In Free and Local controlled,
connect to the displayed URL without a token, for example
`cymcp::McpClient("http://127.0.0.1:8765/mcp")`. CLI clients can use `--url URL`,
and stdio clients can use `mcp --url URL`. Discovery stays compact by default;
all 48 operations remain accessible through the gateway.

Token only and Protected require a random bearer token. An owner-only
`connection-PID.json` in the document AI directory contains its URL and token;
`CyInsightsCli invoke ... --connection FILE` uses both. Address-only profiles
write the same connection file without a token field. Do not publish token-bearing
files. On normal shutdown the connection file is removed. Re-copy settings after
restarting or changing profiles. The named-pipe transport retains Windows-user
access control; the HTTP profile determines whether HTTP callers need a token.

SDK HTTP clients can subscribe to the bounded change journal; stdio clients use
TTL and explicit refresh. No custom MCP framing or authentication parser is used.

## Results, jobs and old data

Synchronous calls remain available. Use `job_start` with `operation` and
`arguments` for background work, then `job_status` and `job_result`. Jobs own
cooperative cancellation tokens. `job_list` supports offset/limit; `job_delete`
explicitly removes a finished result. Work interrupted by app exit is marked
interrupted on recovery and is never restarted automatically.

Durable JSON results survive restarts and SDK resource expiration. Storage is
bounded to 1 GiB, with at most 32 live jobs and 64 MiB per result. Quota exhaustion
fails explicitly; existing results are not evicted. Large results are returned
as private resource references and fully recoverable bounded chunks during the
configured SDK retention. Read their manifest and follow firstUri/nextUri until
eof, or retrieve a fresh reference from the durable job. Expired references never
re-execute actions. Data directories are user-private: LocalAppData/CyInsights
or Data/ beside a portable executable, under AI/Native/document-ID.

Old Python environments, SQLite databases and reports are left intact. The old
CyTools job/session wire protocol is replaced, not silently emulated. Import an
existing JSON result with `job_import {"path":"ABSOLUTE_REPORT.json"}` after
enabling app control. It accepts a raw result or an `outputs.data` envelope,
retains the original file and saves a new native durable result. No old database
is rewritten and interrupted old jobs are not replayed. The old runtime can be
removed manually after exporting wanted reports; it is no longer shipped or run.

## Independent optimizations

Assistant settings control compact discovery, temporary disk spooling,
notifications, metadata-only performance logging, cache TTL, retention, chunk
size, disk quota and API context bytes separately. Save and restart to apply the
transport/storage settings; reconnect to change conversation settings.

Defaults: compact discovery on, 32 KiB chunks, 300 s SDK retention, 256 MiB disk
spool, 30 s cache TTL, notifications on, performance logging off, 128 KiB API
context projection. Durable job storage has independent retention (explicit
deletion). Metrics contain only operation/stage/status/duration/byte counts/cache
metadata, never prompts, results, credentials or trace identifiers. The full
conversation remains saved when context projection omits old turns. System
instructions and active user/tool exchanges remain together. No implicit summary
model is invoked. Ripgrep is not enabled: trace data has typed filtered/paginated
queries and exposed text uses the SDK's native resource grep.

## Optional remote tools

The Assistant accepts an explicit HTTP MCP endpoint, a token environment-variable
name and an exact tool allowlist. It never enables all discovered tools implicitly.
Remote tools are qualified with `remote.` and added only to this assistant after
reconnection; they do not grant control in another application automatically.
SDK client errors/tool failures are propagated to the assistant. HTTPS is required
outside loopback. Remove remote tools revokes their calls immediately, including in a retained conversation; reconnect to refresh the displayed tool definitions. Set allowResources to true only when you also want the qualified remote resource helpers. Read remote handles with remote.resources_read, not the application resource store.
Remote schemas must fit the SDK's checked schema profile; unsupported contracts
fail visibly instead of losing validation. No remote server is contacted by default.

## Source build and validation

CMake 3.24+, MSVC C++20, the existing build-time Python/Node documentation tools.
Core, Platform, Http, MCP, AI and ChatUI are built; Local, examples, ChatApp and
SDK test/demo executables are off. CyUI DocumentViews/Code are added to the
existing explicit module set; Audio, Gallery, CEF and CodeEditor remain off.
One host ImGui target is shared. CyUI revision is
`6ef01cfcdb197f3a89d64618ce5ef8818c704728`. SDK sources are unmodified.
The host retains its UTF-8 std::string path ABI; SDK sources compile with native
char8_t. The JSON library uses its documented C++17 feature-profile override in
both to keep matching inline definitions. This is a Windows integration; Linux
and macOS desktop backends were not introduced by this change.

```powershell
python Tools/prepare_dependencies.py
# Configure/build with the project's existing MSVC + Ninja procedure.
cmake --build build/WindowsDistribution --target CyInsightsApp CyInsightsCli CyComponentTests
ctest --test-dir build/WindowsDistribution --output-on-failure
python Tests/NativeAiE2E.py --binaries bin/WindowsDistribution --tests build/WindowsDistribution/CyNativeAiTests.exe --output build/NativeAiE2E
```

The end-to-end fixture tests streaming/tool protocol plumbing against the real
application, cancellation, history restoration, CLI and external MCP. It does
not claim a paid/provider inference test. No GitHub workflow is needed.
