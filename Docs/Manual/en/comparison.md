# Compare two captures {#title}

Open Capture comparison or Analysis > Compare native capture, then select B. Both originals remain read-only and can also stay open in independent tabs/windows.

## Compare fairly {#compare-fairly}
1. Check platform, build, provider availability and capture profile on each side.
2. Select A/B intervals independently. Equivalent gameplay matters more than equal absolute timestamps.
3. Compare counts, distributions, active work and waits.
4. Normalize per second or per occurrence when duration/count differs. A larger total from a longer capture is not automatically a regression.
5. Inspect the functions/metrics explaining the change.

Names/domains are derived correspondences, not shared object identity. Missing channels cannot be compared as zero. Different instrumentation changes coverage and can change overhead; prefer equivalent collection settings.

Keep capture identity, interval, population/count, unit and normalization in the report. Optional GPU comparison only uses actual collector evidence; it does not invent matching passes in unrelated captures.

See [Histograms](doc:functions), [Frames](doc:frames) and [AI reports](doc:ai).
