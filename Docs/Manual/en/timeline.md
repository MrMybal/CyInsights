# Navigate and measure the timeline {#title}

## Navigate {#navigate}
- Right-button drag pans time horizontally and moves through tracks vertically.
- The wheel zooms around the pointer. Fit frames the capture; Frame controls navigate frames; F fits the selection.
- Collapse unused tracks. GPU is above GameThread. Useful track height follows visible nesting rather than reserving empty depth.

The upper ruler navigates. The finer additional ruler beneath it measures: left-drag A to B to select a range with endpoints and duration. Reversed drags still produce ordered intervals. Frames and timing share the viewport.

## Select and filter {#select-and-filter}
A scope click selects an event. Double-click a scope to filter all items with exactly that name. Double-click empty timing space to clear that view filter. Right-click a scope for histogram/exact-name actions; right-drag remains navigation, not a context click.

Distributions, function statistics, frames, counters and supported activity queries use the selected interval or the interval named in their panel. Check those bounds before comparing results. Fit selection changes the viewport, not recorded timestamps. Search and exact-name filtering are separate actions. No range operation creates missing capture data.

See [Functions](doc:functions), [Frames](doc:frames) and [Controls](doc:shortcuts).
