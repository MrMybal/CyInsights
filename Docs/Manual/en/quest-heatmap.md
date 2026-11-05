# GPU tiles and heatmap overlay {#title}

GPU heatmap displays a recorded snapshot. Heatmap overlay controls an additional live development overlay, separate from statistics. It needs the Vulkan APK, selected debuggable Vulkan target, prepared external layer, supported Meta detailed tile data and a complete game reopen after setup.

1. Check target and readiness in Heatmap overlay.
2. Prepare layer/profiling as requested before the new GPU context; reopen the game manually once.
3. Choose Extra HUD or Fullscreen (approximate), position, cost ceiling and opacity.
4. Start heatmap; Apply changes an active overlay. Stop hides it but retains setup. Uninstall target integration removes setup and requires preparation/reopen next time.

Color follows the selected sampled cost/scale; low opacity preserves the game image. Fullscreen alignment is approximate. Surface dimensions alone do not identify an eye, exact frame or projection. The manual grid is explicitly an example, not live data.

Binning is an envelope. Render includes vertex and fragment work. LoadColor/LoadDepthStencil and StoreColor/StoreDepthStencil describe surface traffic when available. A hot tile localizes suspect work, not a unique mesh/shader. Do not add overlapping envelopes as independent costs.

There is no validated universal custom compositor overlay for every unmodified commercial APK. Missing/stale/wrong-target data is unavailable, not simulated. See [Tile recording](doc:tile-recording) and [GPU evidence](doc:gpu-desktop).
