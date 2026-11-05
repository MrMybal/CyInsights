# Capture Unreal and Meta GPU together {#title}

Unreal native GPU groups and Meta hardware profiling are complementary. GPU Profiling, GPU Metrics and GPU Render Stage Trace are distinct controls; enabling Meta does not add Unreal child scopes to an old .utrace.

Detailed profiling must be prepared before the target creates its GPU context. In MQDH Trace Analysis by Perfetto settings/presets, choose the required GPU metrics/render stages; Render Pass Diagnosis requests detailed stages. Save settings. A game already running can need one complete manual close/reopen. Detailed profiling adds overhead, so keep diagnostics short.

## Record in CyInsights {#record-in-cyinsights}
1. Connect to the plugin and verify its response.
2. Enable optional Meta collection in Session / Capture options; authorized Android tooling and a supported device route must be available.
3. Keep the game scene visible and headset awake. Normal capture does not restart it; a restart is an explicit option.
4. Start/stop remote trace. The app finalizes one .cycapture with received Unreal timing, Meta stages, counters and metadata, then can open it automatically.
5. Check event types, precision and clock-alignment notices, not just startup acceptance.

A coarse source stays coarse. Binning, Render, LoadColor/LoadDepthStencil and StoreColor/StoreDepthStencil appear only when received. A stage alone does not identify a shader, mesh, eye or exact execution frame. See [Heatmaps](doc:quest-heatmap) and [Troubleshooting](doc:troubleshooting).
