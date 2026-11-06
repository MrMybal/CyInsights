---
id: functions
title: Functions, callers and histograms
description: Exact-name filters, duration distributions and example events.
section: Guides
order: 6
parent: timeline
revision: a948f47d5d3c
---

Timers gives function counts and timing aggregates. Inclusive time includes nested children; exclusive time excludes known child intervals. Derived exclusive cost depends on the recorded nesting. Summing every inclusive row is not total frame time.

## Investigate one function {#investigate-one-function}
1. Choose a short suspicious timeline interval.
2. Double-click a scope or use its exact-name context action.
3. Right-click and choose Histogram of this function.
4. Inspect count, distribution and available percentiles. Select a bin to request exact example events and locate them on the timeline.
5. Change the timeline range; verify the histogram's displayed interval/filter before comparing.

A duration histogram describes occurrences, not a time-series curve. A low average can coexist with a few costly calls. Percentiles concern the selected population and are unstable with few samples. Caller/callee relationships depend on provider/nesting; aggregated views can require zooming to individual scopes.

Double-click empty timing space to clear the exact-name filter. See [Comparison](doc:comparison) for separate captures and [Tasks](doc:tasks) for queue delays.


## Actual function histogram {#histogram}

Recorded application view: a function distribution in a selected interval. Values belong to that example trace.

```image
{"id": "histogram", "src": "assets/cyinsights-function-histogram.png", "alt": "Actual function histogram", "controls": "minimal"}
```
