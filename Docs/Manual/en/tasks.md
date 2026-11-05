# Tasks and execution delays {#title}

Open Tasks and load the native provider when available. The recording needs TaskTrace; arbitrary CPU scope names cannot reconstruct missing native task identities. Bound queries by interval and lane before asking for individual events/links.

Creation, scheduling, queue delay, execution and completion are distinct task phases; some can be missing. A short function can still finish late because its task waited for a worker. Execution time is not queue delay.

The CPU-candidate fallback is explicitly derived from scopes. It has no native TaskTrace ID, measured queue delay or authoritative dependency chain. Do not present candidate relationships as recorded engine dependencies.

Inspect links around a long task, correlate execution with thread waits/budget and compare a good interval. A dependency only explains a bottleneck if its timing actually blocks the work. Do not change worker counts based on a link alone.

Task collection is optional in [Profiles](doc:profiles). Missing phases stay unavailable. See [Functions](doc:functions) for body costs and [Frames](doc:frames) for pacing.
