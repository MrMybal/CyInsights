---
id: quest-setup
title: Set up CyInsightOVR
description: APK variants, standalone runtime and one-time permissions.
section: Guides
order: 13
parent: quest
revision: 812ab254a04b
---

Install one APK: Standard for the basic companion or Vulkan for the optional Development-layer features, including draw/triangle counters and experimental heatmap. Preserve the signing key for in-place updates; do not uninstall to fix a signature mismatch without backing up your data.

Open Library > Unknown Sources > CyInsightOVR. Launcher shortcuts depend on Horizon OS; the quick-configuration action only requests one where supported. The large configuration window is separate from the compact VR HUD.

CyInsightOVR starts its own foreground GPU service when opened. Connection has Start / Stop GPU service and status. No separate CyInsightCompagnon or PC-hosted collector is required for normal operation. After reboot, open the app again to start its service.

Controlling Meta HUD settings and configuring the external Vulkan layer need the documented one-time WRITE_SECURE_SETTINGS developer authorization. The optional full Quest installer grants it through authorized ADB. This setup is distinct from requiring a PC for every HUD start. A reinstall/permission reset can require it again. Image sharing has a separate Android consent dialog.

Navigate Overview, GPU heatmap, Display & HUD, Heatmap overlay, Connection, Updates and Help. Start HUD keeps configuration open. See [HUD](doc:quest-hud), [Counters](doc:quest-counters) and [Tile recording](doc:tile-recording).


## Quest sources and displays {#quest-architecture}

Complementary components, with separate prerequisites and evidence checks.

```diagram
{"type": "flow", "title": "Quest sources and displays", "nodes": [{"id": "n0", "label": "Unreal: native trace", "x": 0, "y": 0}, {"id": "n1", "label": "OVR: local service", "x": 310, "y": 0}, {"id": "n2", "label": "Optional game layer", "x": 0, "y": 100}, {"id": "n3", "label": "Meta GPU sources", "x": 310, "y": 100}, {"id": "n4", "label": "HUD / tile overlay", "x": 0, "y": 200}, {"id": "n5", "label": "PC recording/analysis", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
