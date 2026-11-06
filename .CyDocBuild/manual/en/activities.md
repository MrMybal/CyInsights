---
id: activities
title: Network, storage, loading and memory
description: Optional native lanes and their capture requirements.
section: Guides
order: 9
parent: timeline
revision: c24479c87d39
---

Open Network / storage / memory (Activities) and load the provider you need. Activity readers are separate optional adapters. A timing-only trace does not guarantee activity channels. Filter kind/lane and select a short interval.

- Network: recorded packets/content require NetTrace and suitable instrumentation. Content bits are not automatically total wire bytes, socket traffic or a complete external packet capture.
- Storage: recorded Unreal file operations, not all OS disk operations of every process.
- Loading: engine asset/load events when present, distinct from raw filesystem traffic.
- Memory: available gauges/tags. LLM needs its instrumentation; detailed allocations require startup tracing flags. Full memory-callstack exploration is not complete here.

Start with lane summaries and request events only for a suspect lane. Compare overlap with CPU/GPU work while preserving clock/availability. A packet near a hitch is correlation, not proof of its cause.

Enable sources before recording in [Profiles](doc:profiles). Opening this panel or an AI query cannot instrument an old trace. Messages contains application diagnostics; a complete native Unreal log reader is not provided in this build.


## From capture to analysis {#capture-flow}

Workflow diagram. Only captured sources can be analyzed.

```diagram
{"type": "flow", "title": "From capture to analysis", "nodes": [{"id": "n0", "label": "Choose sources", "x": 0, "y": 0}, {"id": "n1", "label": "Record target", "x": 310, "y": 0}, {"id": "n2", "label": "Stop and finalize", "x": 0, "y": 100}, {"id": "n3", "label": "Open container", "x": 310, "y": 100}, {"id": "n4", "label": "Load providers", "x": 0, "y": 200}, {"id": "n5", "label": "Select and inspect", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
