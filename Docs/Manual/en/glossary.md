# Units, evidence and glossary {#title}

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
