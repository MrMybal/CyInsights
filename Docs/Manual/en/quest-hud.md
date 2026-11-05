# Choose HUD graphs and text {#title}

Open Display & HUD > Graphs & HUD.

| Column | Destination |
| --- | --- |
| HUD graph | In-game curve |
| HUD value | In-game numerical text |
| Overview | Configuration-app curve only |

Choose graph only, value only or both. Add draw calls + triangles adds both for the primary rate counters without clearing other choices; it does not select only per-presented-frame variants. All HUD graphs selects listed curves; Clear HUD clears selections. Search and GPU/CPU/Memory/Device filters reduce the table.

Start HUD applies choices and leaves configuration open. Running checkbox changes are saved/applied after a short debounce; Apply HUD retries explicitly. Stop HUD is separate from the heatmap. Nine placements include Top center, Center and Bottom center. Shared statistics size/position and tile-overlay opacity/position are independent.

Native Meta values use its installed HUD. Extra raw GPU/geometry metrics use the public custom-metric route through the selected Development game's Vulkan layer: Meta scopes those metrics to the actual VR application. Use the Vulkan APK, exact target and one complete target reopen after preparation/update. Accepted definitions/settings do not prove visibility; this alpha's new shared custom-curve route still awaits final visual validation, and publication failures remain explicit.

[N/A] means absent/stale/unusable, not zero. [partial] means incomplete known totals. See [Counters](doc:quest-counters) and [Troubleshooting](doc:troubleshooting).
