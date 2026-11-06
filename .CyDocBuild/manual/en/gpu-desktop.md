---
id: gpu-desktop
title: Read GPU evidence
description: Measured scopes, derived passes and optional collectors.
section: Guides
order: 11
parent: timeline
revision: 342badc87e59
---

Start with measured native GPU groups. A single MobileSceneRender bar means only that timing level is available. A different panel cannot recreate missing child timestamps.

## Distinguish sources {#distinguish-sources}
- Native Unreal GPU scopes: measured when the RHI/build supplies timestamps and instrumentation.
- Vulkan pass/subpass contents: command arguments, attachments and structure. CPU-side command contents are not per-subpass hardware costs.
- Optional Windows collector: commands, pipelines, shaders, resources and available GPU timestamps. Enable explicitly; ordinary .utrace analysis needs no separate CyGPUInspector app.
- Meta hardware stages/tiles: a distinct [Quest source](doc:quest-capture).

GPU frame can show pass timelines, draw/dispatch counts and reconstructed dependencies. Derived passes/links are labeled. Command timestamps are opt-in and can be frame-local. A shader/resource description does not establish its duration.

High Render cost alone does not prove a pixel-shader bottleneck: Render can include vertex and fragment work. Correlate shares, texture/memory stalls, dimensions, tiles and controlled changes. Binning is an envelope, not extra independent time to add to all children. Waits and overlap matter.

Images/textures/buffer readbacks are off initially. Detailed profiling can perturb scheduling: use short diagnostics, then retest with a light [profile](doc:profiles). See [Glossary](doc:glossary).


## Actual optional GPU inspector {#gpu-passes}

Windows collector example: pass timeline and reconstructed dependencies. Derived relationships are labeled.

```image
{"id": "gpu-passes", "src": "assets/cyinsights-gpu-passes.png", "alt": "Actual optional GPU inspector", "controls": "minimal"}
```


## Pipelined work and waits {#cpu-gpu}

Conceptual pipeline. Adjacent bars need not describe one execution frame.

```diagram
{"type": "flow", "title": "Pipelined work and waits", "nodes": [{"id": "n0", "label": "Game frame N", "x": 0, "y": 0}, {"id": "n1", "label": "Render can lag", "x": 310, "y": 0}, {"id": "n2", "label": "GPU can lag again", "x": 0, "y": 100}, {"id": "n3", "label": "Wait is not work", "x": 310, "y": 100}, {"id": "n4", "label": "Check actual clocks", "x": 0, "y": 200}, {"id": "n5", "label": "Test the cause", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
