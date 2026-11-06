---
id: quest-hud
title: Choose HUD graphs and text
description: Independent destinations, live apply and nine positions.
section: Guides
order: 14
parent: quest
revision: 4e325ce604e9
---

Open Display & HUD > Graphs & HUD.

| Column | Destination |
| --- | --- |
| HUD graph | In-game curve |
| HUD value | In-game numerical text |
| Overview | Configuration-app curve only |

Choose graph only, value only or both. Add draw calls + triangles adds both for the primary rate counters without clearing other choices; it does not select only per-presented-frame variants. All HUD graphs selects listed curves; Clear HUD clears selections. Search and GPU/CPU/Memory/Device filters reduce the table.

Start HUD applies choices and leaves configuration open. Running checkbox changes are saved/applied after a short debounce; Apply HUD retries explicitly. Stop HUD is separate from the heatmap. Nine placements include Top center, Center and Bottom center. Shared statistics size/position and tile-overlay opacity/position are independent.

Native Meta values use its installed HUD. Extra raw GPU/geometry metrics use the public custom-metric route through the selected Development game's Vulkan layer: Meta scopes those metrics to the actual VR application. Use the Vulkan APK, exact target and one complete target reopen after preparation/update. If a custom graph is absent, inspect the target layer connection and the publication error shown in the app; accepting the settings alone does not mean the game is publishing values.

[N/A] means absent/stale/unusable, not zero. [partial] means incomplete known totals. See [Counters](doc:quest-counters) and [Troubleshooting](doc:troubleshooting).


## Choose each destination {#hud-selection}

Graph/text flags are independent. Overview alone never requests an in-game graph.

```diagram
{"type": "flow", "title": "Choose each destination", "nodes": [{"id": "n0", "label": "Find metric row", "x": 0, "y": 0}, {"id": "n1", "label": "HUD graph: curve", "x": 310, "y": 0}, {"id": "n2", "label": "HUD value: text", "x": 0, "y": 100}, {"id": "n3", "label": "Overview: app only", "x": 310, "y": 100}, {"id": "n4", "label": "Start / Apply HUD", "x": 0, "y": 200}, {"id": "n5", "label": "Check source/display", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
