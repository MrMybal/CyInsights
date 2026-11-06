---
id: counters
title: Counters and per-frame statistics
description: Load selected sources and inspect curves or frame tables.
section: Guides
order: 7
parent: timeline
revision: 80b5ee98e1da
---

Counters and Frame stats expose distinct sources. A declared counter proves its name/type exists, not that samples were captured. Load samples on request, search/filter availability, then inspect or pin useful series.

Basic plugin snapshots include lightweight GameThread/RenderThread/RHI/GPU observations, frame interval and UObject slot counts/capacity. Optional Stats aggregation can collect configured active groups such as Game, UObjects and GPU. Enable it before recording. The plugin neither recreates groups absent from the build nor executes every stat HUD command.

Frame stats offers synchronized curves and per-frame tables. Check unit, availability, source frame and observation time before selecting an interval. Missing frames remain absent; delayed GPU/RT scalar observations do not prove exact GPU-frame identity.

A gauge (memory in use, utilization) is not a cumulative byte/event total. Large integer counters can exceed floating-point precision; SDK outputs preserve exact integer forms where available. Display bins reduce data, not create observations. A 20 Hz interface cannot generate 20 Hz measurements from a 1 Hz Meta source.

See [Profiles](doc:profiles), [Activities](doc:activities) and [Quest counters](doc:quest-counters).


## A focused investigation {#analysis-flow}

Summaries first; individual events support a finding.

```diagram
{"type": "flow", "title": "A focused investigation", "nodes": [{"id": "n0", "label": "Check sources/units", "x": 0, "y": 0}, {"id": "n1", "label": "Find slow frames", "x": 310, "y": 0}, {"id": "n2", "label": "Select short range", "x": 0, "y": 100}, {"id": "n3", "label": "Inspect one domain", "x": 310, "y": 100}, {"id": "n4", "label": "Compare good case", "x": 0, "y": 200}, {"id": "n5", "label": "Evidence + limits", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
