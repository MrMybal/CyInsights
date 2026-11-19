---
id: ai
title: AI Inspector, MCP and SDK
description: Local tools, external clients, permissions and compact evidence.
section: Guides
order: 0
parent: developer-api
revision: 34786a4944ee
---

AI Inspector lets you run the application's native analysis tools and review findings. Open **Tools**, choose an operation, inspect its parameters, then run it. Preparing the form does not execute the tool. The native tools do not require an external model, Python or an MCP client.

## External connection {#external-connection}
1. Open **AI Inspector > Connection** and enable the local MCP/SDK bridge.
2. Copy the generated configuration into your MCP client. The native CyInsightsCli executable is already included; no runtime download or Python setup is needed.
3. Enable application control or remote capture only for the actions you need. These independent permissions apply equally to the assistant, CLI and MCP.
4. Re-copy the PID/document connection after restarting or changing documents. Tokens in the private HTTP connection file must not be published.

## Integrated assistant {#integrated-assistant}
Open **Assistant > Connection and options**, detect providers/local services, choose an explicit provider and model, then connect. The CyMCPSDK panel shows streamed Markdown, tool activity and errors, supports cancellation and provider-compatible attachments. Use an environment variable or a memory-only key. Saved conversations restore without resending turns. Model weights are not bundled; existing Codex, Ollama, LM Studio, llama.cpp and supported API profiles retain their own authentication and capabilities.

The 39 business operations and nine native job/resource utilities share one typed registry. Compact discovery, disk spool, TTL/cache, subscriptions, metadata-only performance logs and API context bytes are independent settings. Saving transport/storage settings requires restarting; reconnect for conversation changes. Full history remains saved when the API context window is reduced. Native agents manage their own context. No implicit summarizer is called.

Use job_start/job_status/job_result for durable results, job_list with offset/limit and job_cancel for cooperative cancellation. SDK chunk references expire, but saved job results remain available until explicit deletion. Follow a resource manifest's firstUri/nextUri to recover a large result fully. Expiration never replays actions. job_import can copy an existing JSON report after app control is enabled; old environments and databases are preserved.

Optional remote MCP endpoints need an explicit tool allowlist and token environment variable. They are never contacted by default. Outside loopback, HTTPS is required. The [native CLI and API reference](doc:developer-api) describes the public operations.

## Show evidence in the interface {#show-evidence}

Enable **Allow AI application control** to let the assistant use the native
interface actions: select and focus a time interval (`set_range`), highlight an
exact function (`filter_function`), open its histogram (`show_histogram`), show
an analysis panel (`focus_view`), pin a recorded counter (`pin_counter`) or
publish a finding (`publish_finding`). Try: "Find the slowest frames, show me the
interval and display the relevant counters."

Read-only analysis remains available without control. Starting or stopping a
remote recording needs the separate capture permission. The assistant accesses
the declared operations and recorded sources: it does not control every button,
browse arbitrary files or automatically inspect all images and tile heatmaps.
Missing channels remain unavailable. Inspect capabilities before drawing conclusions.

## Review an investigation {#compact-workflow}
Select a short suspicious interval before running an analysis tool. In **Method**, follow the proposed steps and check the resulting parameter form. Inspect a finding's frame range, units and source alongside [Frames](doc:frames) or [GPU evidence](doc:gpu-desktop). A probable cause remains a hypothesis.

Loading a provider can continue after a request is accepted; use the visible progress and availability state. Cancel loading if necessary. Stopping an analysis does not stop a remote recording: use **Stop remote trace** for that recording. Reports cannot recover channels absent from the capture.

## A focused investigation {#analysis-flow}

Summaries first; individual events support a finding.

```diagram
{"type": "flow", "title": "A focused investigation", "nodes": [{"id": "n0", "label": "Check sources/units", "x": 0, "y": 0}, {"id": "n1", "label": "Find slow frames", "x": 310, "y": 0}, {"id": "n2", "label": "Select short range", "x": 0, "y": 100}, {"id": "n3", "label": "Inspect one domain", "x": 310, "y": 100}, {"id": "n4", "label": "Compare good case", "x": 0, "y": 200}, {"id": "n5", "label": "Evidence + limits", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
