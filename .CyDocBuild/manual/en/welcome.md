---
id: welcome
title: Start here
description: Choose a workflow and use this offline guide.
section: Guides
order: 0
revision: 13bfb8db9fde
---

CyInsights helps you find where a recorded Unreal frame spends its time. Use the Windows application for CPU/GPU timelines, counters, tasks and activities. CyInsightOVR provides Quest statistics and optional development overlays. The Unreal plugin records the game data.

## Choose your workflow {#choose-your-workflow}
1. Have a recording? [Open it](doc:opening-traces), then [navigate the timeline](doc:timeline).
2. Want to record a game? [Install the plugin](doc:unreal-plugin) and [choose a capture profile](doc:profiles).
3. Using a Quest? [Set up CyInsightOVR](doc:quest-setup), [choose HUD graphs](doc:quest-hud) or [record detailed GPU stages](doc:quest-capture).
4. Investigating a hitch? Compare [frame costs](doc:frames), [function durations](doc:functions) and [GPU evidence](doc:gpu-desktop).

## Use this manual {#use-this-manual}
Search at the top finds pages, examples and public API signatures. **Ctrl+K** opens the global search dialog. Expand a sidebar branch to see its sub-pages. Hover a title, or focus it with Tab, for its complete label and description. English / Français changes the manual language independently of the application.

Internal links open the relevant page or section. Back/Forward follows visited pages; the reader keeps reading and sidebar positions. Explicit section links take you to that section. The theme menu offers light, dark and high-contrast choices.

Use the image controls to zoom, pan or enlarge an image inside this window. Escape reduces it. Controls are available on focus and touch screens. The screenshots show recorded examples; diagrams explain workflows and are not measurements from your recording.

The complete **Documentation/** folder comes with the application. Open **Documentation.html** directly, or use **Help > User manual / F1** on Windows and **Help** on Quest. Reading needs no account, server or connection. Both English and French contain the same complete manual.


## From capture to analysis {#capture-flow}

Workflow diagram. Only captured sources can be analyzed.

```diagram
{"type": "flow", "title": "From capture to analysis", "nodes": [{"id": "n0", "label": "Choose sources", "x": 0, "y": 0}, {"id": "n1", "label": "Record target", "x": 310, "y": 0}, {"id": "n2", "label": "Stop and finalize", "x": 0, "y": 100}, {"id": "n3", "label": "Open container", "x": 310, "y": 100}, {"id": "n4", "label": "Load providers", "x": 0, "y": 200}, {"id": "n5", "label": "Select and inspect", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
