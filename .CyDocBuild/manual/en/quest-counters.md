---
id: quest-counters
title: Draw calls, triangles and curve frequency
description: Submitted geometry, completeness and XR frame limits.
section: Guides
order: 15
parent: quest-hud
revision: 3e18daa185b5
---

Choose the exact Development Vulkan package. Start HUD prepares an unprepared target; Overview also offers Prepare draw counter layer and Start draw counters. Completely close/reopen the target once after setup or layer update. Commercial non-debuggable APKs are not supported external-layer targets.

Known submitted draws count tracked commands at command-buffer submission, including reused buffers, repeated secondary execution and instances. Known input triangles are before culling, clipping, shader-generated geometry and view expansion: not visible triangles, objects or per-eye totals. Our heatmap draws are excluded.

Indirect commands may provide known draws but unknown triangles. GPU-controlled counts, primitive restart, unknown topology and missing buffers mark totals incomplete. Check completeness and interval, not only the headline value.

## Rate versus frame {#rate-versus-frame}
Primary counters are draw/s and triangles/s. Per-presented-frame variants need a real Vulkan presentation boundary. Many XR eye paths do not use vkQueuePresentKHR; queue submissions are not invented as frames. These variants can stay N/A while rate counters work.

Curve updates offers 1/5/10/20 Hz, initially 10. Check measured source cadence. Slower system counters are not interpolated. Meta's integer custom HUD uses K triangles for useful precision; source/Overview values retain their units. Stale samples leave gaps. Metadata and IPC add workload; game overhead still needs measurement.


## Choose each destination {#hud-selection}

Graph/text flags are independent. Overview alone never requests an in-game graph.

```diagram
{"type": "flow", "title": "Choose each destination", "nodes": [{"id": "n0", "label": "Find metric row", "x": 0, "y": 0}, {"id": "n1", "label": "HUD graph: curve", "x": 310, "y": 0}, {"id": "n2", "label": "HUD value: text", "x": 0, "y": 100}, {"id": "n3", "label": "Overview: app only", "x": 310, "y": 100}, {"id": "n4", "label": "Start / Apply HUD", "x": 0, "y": 200}, {"id": "n5", "label": "Check source/display", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
