---
id: developer-api
title: SDK and public API
description: Connect an external client and use the supported operation contracts.
section: Reference / Developer API
order: 50
revision: 57261f95a005
---

The 39 business contracts below belong to the native CyMCPSDK registry. The same handlers serve the integrated Assistant, console and external MCP. Source availability always applies. Free permits all exposed operations; Local controlled and Protected also enforce the document's control/capture gates. Read [AI Inspector](doc:ai).

## Native CLI entry point {#python-entry}

```powershell
CyInsightsCli list
CyInsightsCli describe histogram
CyInsightsCli invoke get_state --url http://127.0.0.1:8765/mcp
CyInsightsCli invoke get_state --pid 1234
CyInsightsCli invoke histogram --trace timing.csv --json '{"function":"Work"}'
```

Use the PID/document shown in **AI Inspector > Connection** for a real open instance. No Python installation is needed. `mcp --pid PID` launches the native stdio MCP server; add `--full` for complete discovery. Typed results are returned directly.

## Durable jobs and resource utilities {#durable-jobs}

Nine native utilities complement the business contracts: `job_start`, `job_status`, `job_result`, `job_list`, `job_cancel`, `job_delete`, `job_import`, `read_resource`, `grep_resources`. Describe any operation for its exact JSON schema. Durable results survive restarts; unfinished actions are never replayed. Old JSON reports can be imported explicitly with app control enabled. Existing Python environments and databases are left intact.
