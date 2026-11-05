# Counters and per-frame statistics {#title}

Counters and Frame stats expose distinct sources. A declared counter proves its name/type exists, not that samples were captured. Load samples on request, search/filter availability, then inspect or pin useful series.

Basic plugin snapshots include lightweight GameThread/RenderThread/RHI/GPU observations, frame interval and UObject slot counts/capacity. Optional Stats aggregation can collect configured active groups such as Game, UObjects and GPU. Enable it before recording. The plugin neither recreates groups absent from the build nor executes every stat HUD command.

Frame stats offers synchronized curves and per-frame tables. Check unit, availability, source frame and observation time before selecting an interval. Missing frames remain absent; delayed GPU/RT scalar observations do not prove exact GPU-frame identity.

A gauge (memory in use, utilization) is not a cumulative byte/event total. Large integer counters can exceed floating-point precision; SDK outputs preserve exact integer forms where available. Display bins reduce data, not create observations. A 20 Hz interface cannot generate 20 Hz measurements from a 1 Hz Meta source.

See [Profiles](doc:profiles), [Activities](doc:activities) and [Quest counters](doc:quest-counters).
