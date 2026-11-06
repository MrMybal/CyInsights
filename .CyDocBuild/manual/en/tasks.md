---
id: tasks
title: Tasks and execution delays
description: Native phases, dependencies and derived CPU candidates.
section: Guides
order: 8
parent: timeline
revision: 34baf854b597
---

Open Tasks and load the native provider when available. The recording needs TaskTrace; arbitrary CPU scope names cannot reconstruct missing native task identities. Bound queries by interval and lane before asking for individual events/links.

Creation, scheduling, queue delay, execution and completion are distinct task phases; some can be missing. A short function can still finish late because its task waited for a worker. Execution time is not queue delay.

The CPU-candidate fallback is explicitly derived from scopes. It has no native TaskTrace ID, measured queue delay or authoritative dependency chain. Do not present candidate relationships as recorded engine dependencies.

Inspect links around a long task, correlate execution with thread waits/budget and compare a good interval. A dependency only explains a bottleneck if its timing actually blocks the work. Do not change worker counts based on a link alone.

Task collection is optional in [Profiles](doc:profiles). Missing phases stay unavailable. See [Functions](doc:functions) for body costs and [Frames](doc:frames) for pacing.


## A focused investigation {#analysis-flow}

Summaries first; individual events support a finding.

```diagram
{"type": "flow", "title": "A focused investigation", "nodes": [{"id": "n0", "label": "Check sources/units", "x": 0, "y": 0}, {"id": "n1", "label": "Find slow frames", "x": 310, "y": 0}, {"id": "n2", "label": "Select short range", "x": 0, "y": 100}, {"id": "n3", "label": "Inspect one domain", "x": 310, "y": 100}, {"id": "n4", "label": "Compare good case", "x": 0, "y": 200}, {"id": "n5", "label": "Evidence + limits", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
