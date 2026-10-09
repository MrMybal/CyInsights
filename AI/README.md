# CyInsights AI companion

The UI and analysis engine remain native C++/Dear ImGui. CyToolsCore **0.10.1 MIT**
owns CLI, JSONL, local authenticated HTTP, official **MCP SDK 1.30.0**, private jobs,
sessions, reports, cancellation and resource scheduling. There is no dependency
on the CyToolsCore checkout or an AI provider account at runtime.

Versioned local test releases can include an installed private environment.
The Git repository excludes that environment; create it locally as described below.
For a fresh installation, run `AI/SetupRuntime.ps1` with **Python 3.11 x64** available;
the supplied wheels are offline and version pinned. Other Python ABIs/platforms
need their own dependency builds and validation. This is not portable Python.
The profiler itself works without the companion installed.

In the application open **AI Inspector > Connection**, enable the local bridge,
and **Copy MCP configuration**. Paste that configuration into your AI client's
MCP settings. The PID selects this application instance; copy it again after
restarting the application. Control and capture permissions are off by default;
enable the two separate controls for the actions you want the AI to perform.

The named pipe permits only the application's Windows user and rejects remote
pipe clients. Jobs/reports are isolated by CyTools identities. The visible trace,
timeline and remote capture receiver are shared application state. Standalone
stdio clients are created with SDK permissions for the local Windows user; native
UI gates still enforce application/capture access. A shared HTTP service requires
explicit client tokens, as defined by CyToolsCore. Do not expose it remotely.

SDK calls use `op_<operation>` with `{"parameters":{...}}`. Poll `cy_get_job`
until Completed, Failed or Cancelled. Results are in `outputs.data`, with a private
JSON report at `outputs.reportFile`; export before `cy_finish_task` if retaining it.
Opening a document or starting capture returns **accepted**, not completion.
Poll `get_state` / `capture_status`. SDK cancellation stops a native analytical
request; it does not undo a UI action or stop recording. Use `capture_stop` explicitly.

Typical sequence: get_state → detect_issues → set_range → hotspots → histogram →
publish_finding. Start/stop a development game or Quest trace with capture_probe,
capture_start, capture_status and capture_stop, then open the returned archive.
All commands and schemas are discoverable in the manifest and the native Tools tab.

Trace files are persistent read-only application documents, referenced by absolute
path. They are intentionally not copied or purged as temporary job artifacts:
this avoids duplicating multi-GB captures and preserves subsequent native task
decoding. Generated reports remain task-owned. No trace is uploaded by this adapter.

The native Findings panel follows the selected/visible interval and distinguishes
measured durations, derived coverage and probable causes. An external MCP AI can
publish hypotheses and focus the corresponding interval/function. It does not
embed a chat model. Native incoming trace decoding **during recording** is not
implemented: streaming, stopping and packaging work; analyze after stop. No real
Quest hardware capture was added to the tests in this revision.

See [AI integration](../Docs/AiIntegration.md) and `../release-manifest.json`
in the delivered application directory for evidence and limitations. Licences are
included in the product's root LICENCES, alongside unchanged existing notices.
