---
id: glossary
title: Units, evidence and glossary
description: Availability, clocks, subpasses and tile interpretation.
section: Guides
order: 23
parent: reference
revision: 97043b5f68a2
---

| Term | Meaning |
| --- | --- |
| Measured | Source observation with its own coverage/clock limits |
| Derived | Calculation/reconstruction, not another hardware measurement |
| Unavailable / N/A | Missing, unsupported, stale or unusable; not zero |
| Partial / known | Only the instrumented known portion is counted |
| Inclusive / exclusive | With children / excluding known child intervals |
| Observation time | Reception/observation, not necessarily execution |
| Frame identity | CPU, render and GPU attribution can differ |
| ms / us | 1 millisecond = 1000 microseconds |
| Hz / FPS | Updates / frames per second; polling is not measurement cadence |
| Draw call | Drawing command, not visible mesh/object count |
| Input triangles | Submitted geometry before culling and amplification |
| Render pass / subpass | Attachment-based Vulkan work / internal stage |
| GPU tile | Reported surface region with sampled stage costs |
| Binning / Render | Envelope / rendering that can include vertex and fragment |
| .utrace / .cycapture / .cyovr | Unreal trace / combined container / tile snapshot |

Three subpasses mean three internal stages, not three separately timed GPU bars. Command/attachment contents do not establish each stage's hardware cost or raster start. A hot tile plus image localizes a region, not a unique mesh/shader or exact eye.

Neutral frames, empty histograms and missing counters may mean insufficient evidence. Retain units, interval, source, availability and uncertainty in every report. Return to [Start here](doc:welcome) or [Troubleshooting](doc:troubleshooting).


## Illustrative tile costs {#tile-grid}

EXAMPLE ONLY: colors and microseconds are illustrative, not live measurements. No unique mesh/shader/eye attribution.

![EXAMPLE ONLY: colors and microseconds are illustrative, not live measurements. No unique mesh/shader/eye attribution.](assets/tile-grid-en.svg)


## Pipelined work and waits {#cpu-gpu}

Conceptual pipeline. Adjacent bars need not describe one execution frame.

```diagram
{"type": "flow", "title": "Pipelined work and waits", "nodes": [{"id": "n0", "label": "Game frame N", "x": 0, "y": 0}, {"id": "n1", "label": "Render can lag", "x": 310, "y": 0}, {"id": "n2", "label": "GPU can lag again", "x": 0, "y": 100}, {"id": "n3", "label": "Wait is not work", "x": 310, "y": 100}, {"id": "n4", "label": "Check actual clocks", "x": 0, "y": 200}, {"id": "n5", "label": "Test the cause", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
