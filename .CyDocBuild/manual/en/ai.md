---
id: ai
title: AI Inspector, MCP and SDK
description: Local tools, external clients, permissions and compact evidence.
section: Guides
order: 0
parent: developer-api
revision: 050746cc6da1
---

AI Inspector lets you run the application's native analysis tools and review findings. Open **Tools**, choose an operation, inspect its parameters, then run it. Preparing the form does not execute the tool. The native tools do not require an external model, Python or an MCP client.

## External connection {#external-connection}
1. Open the recording you want to analyze.
2. Open **AI Inspector > Connection > Install MCP/SDK** on Windows. The application downloads verified private Python and installs the included dependencies offline. Follow progress in the panel; you can cancel, retry, repair or copy the installation log. No global Python installation is required.
3. Once the runtime is ready, enable the local MCP/SDK bridge and copy the displayed configuration into your external client. Keep connection tokens private.
4. Leave application control and remote recording disabled when you only need analysis. These permissions are separate.
5. If you restart the application or change documents, use the connection shown for that document.

The Windows installer and Portable ZIP already include the Python adapter and its dependencies. The separate AI SDK archive distributes these tools for advanced use. Installed builds keep their private runtime in application data; portable builds keep it under `Data/AI/Runtime` beside the app. A failed setup does not activate an incomplete runtime. An internet connection is required for the initial Python download; later repairs can reuse its verified cached archive. It does not include a conversational model. The client you choose supplies that model. [Public API references](doc:developer-api) describe the operation contracts.

## Review an investigation {#compact-workflow}
Select a short suspicious interval before running an analysis tool. In **Method**, follow the proposed steps and check the resulting parameter form. Inspect a finding's frame range, units and source alongside [Frames](doc:frames) or [GPU evidence](doc:gpu-desktop). A probable cause remains a hypothesis.

Loading a provider can continue after a request is accepted; use the visible progress and availability state. Cancel loading if necessary. Stopping an analysis does not stop a remote recording: use **Stop remote trace** for that recording. Reports cannot recover channels absent from the capture.

## A focused investigation {#analysis-flow}

Summaries first; individual events support a finding.

```diagram
{"type": "flow", "title": "A focused investigation", "nodes": [{"id": "n0", "label": "Check sources/units", "x": 0, "y": 0}, {"id": "n1", "label": "Find slow frames", "x": 310, "y": 0}, {"id": "n2", "label": "Select short range", "x": 0, "y": 100}, {"id": "n3", "label": "Inspect one domain", "x": 310, "y": 100}, {"id": "n4", "label": "Compare good case", "x": 0, "y": 200}, {"id": "n5", "label": "Evidence + limits", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
