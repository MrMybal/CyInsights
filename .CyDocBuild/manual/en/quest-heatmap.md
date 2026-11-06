---
id: quest-heatmap
title: GPU tiles and heatmap overlay
description: Stage interpretation, extra HUD and approximate fullscreen.
section: Guides
order: 16
parent: quest
revision: 92bfae0c38f3
---

GPU heatmap displays a recorded snapshot. Heatmap overlay controls an additional live development overlay, separate from statistics. It needs the Vulkan APK, selected debuggable Vulkan target, prepared external layer, supported Meta detailed tile data and a complete game reopen after setup.

1. Check target and readiness in Heatmap overlay.
2. Prepare layer/profiling as requested before the new GPU context; reopen the game manually once.
3. Choose Extra HUD or Fullscreen (approximate), position, cost ceiling and opacity.
4. Start heatmap; Apply changes an active overlay. Stop hides it but retains setup. Uninstall target integration removes setup and requires preparation/reopen next time.

Color follows the selected sampled cost/scale; low opacity preserves the game image. Fullscreen alignment is approximate. Surface dimensions alone do not identify an eye, exact frame or projection. The manual grid is explicitly an example, not live data.

Binning is an envelope. Render includes vertex and fragment work. LoadColor/LoadDepthStencil and StoreColor/StoreDepthStencil describe surface traffic when available. A hot tile localizes suspect work, not a unique mesh/shader. Do not add overlapping envelopes as independent costs.

There is no validated universal custom compositor overlay for every unmodified commercial APK. Missing/stale/wrong-target data is unavailable, not simulated. See [Tile recording](doc:tile-recording) and [GPU evidence](doc:gpu-desktop).


## Illustrative tile costs {#tile-grid}

EXAMPLE ONLY: colors and microseconds are illustrative, not live measurements. No unique mesh/shader/eye attribution.

![EXAMPLE ONLY: colors and microseconds are illustrative, not live measurements. No unique mesh/shader/eye attribution.](assets/tile-grid-en.svg)


## Quest sources and displays {#quest-architecture}

Complementary components, with separate prerequisites and evidence checks.

```diagram
{"type": "flow", "title": "Quest sources and displays", "nodes": [{"id": "n0", "label": "Unreal: native trace", "x": 0, "y": 0}, {"id": "n1", "label": "OVR: local service", "x": 310, "y": 0}, {"id": "n2", "label": "Optional game layer", "x": 0, "y": 100}, {"id": "n3", "label": "Meta GPU sources", "x": 310, "y": 100}, {"id": "n4", "label": "HUD / tile overlay", "x": 0, "y": 200}, {"id": "n5", "label": "PC recording/analysis", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
