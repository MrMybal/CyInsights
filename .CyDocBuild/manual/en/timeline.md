---
id: timeline
title: Navigate and measure the timeline
description: Pan, zoom, synchronized frames and range filtering.
section: Guides
order: 4
parent: desktop
revision: ecda4e753dd0
---

## Navigate {#navigate}
- Right-button drag pans time horizontally and moves through tracks vertically.
- The wheel zooms around the pointer. Fit frames the capture; Frame controls navigate frames; F fits the selection.
- Collapse unused tracks. GPU is above GameThread. Useful track height follows visible nesting rather than reserving empty depth.

The upper ruler navigates. The finer additional ruler beneath it measures: left-drag A to B to select a range with endpoints and duration. Reversed drags still produce ordered intervals. Frames and timing share the viewport.

## Select and filter {#select-and-filter}
A scope click selects an event. Double-click a scope to filter all items with exactly that name. Double-click empty timing space to clear that view filter. Right-click a scope for histogram/exact-name actions; right-drag remains navigation, not a context click.

Distributions, function statistics, frames, counters and supported activity queries use the selected interval or the interval named in their panel. Check those bounds before comparing results. Fit selection changes the viewport, not recorded timestamps. Search and exact-name filtering are separate actions. No range operation creates missing capture data.

See [Functions](doc:functions), [Frames](doc:frames) and [Controls](doc:shortcuts).


## Actual CPU/GPU timeline {#timeline}

Recorded application view: frames, nested scopes and available GPU stages share an axis. Example trace, not your current result.

```image
{"id": "timeline", "src": "assets/cyinsights-timeline.png", "alt": "Actual CPU/GPU timeline", "controls": "minimal"}
```


## Navigation and measurement {#timeline-navigation}

Schematic controls, not a trace. Both rulers share one time axis.

```diagram
{"type": "flow", "title": "Navigation and measurement", "nodes": [{"id": "n0", "label": "Right drag: pan", "x": 0, "y": 0}, {"id": "n1", "label": "Wheel: zoom", "x": 310, "y": 0}, {"id": "n2", "label": "Fine ruler: A to B", "x": 0, "y": 100}, {"id": "n3", "label": "Read duration", "x": 310, "y": 100}, {"id": "n4", "label": "Range filters queries", "x": 0, "y": 200}, {"id": "n5", "label": "F: fit range", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
