---
id: unreal-plugin
title: Unreal plugin and connection
description: Install sources for the right build and connect to capture.
section: Guides
order: 2
parent: capture
revision: 7d53a4ed6140
---

Copy the CyInsights plugin source folder into the project's Plugins directory, enable it and rebuild for that engine/platform. The plugin is CyInsights; the optional internal Windows collector does not require the separate CyGPUInspector application.

## Connect {#connect}
1. Open Project Settings > Plugins > CyInsights, or the plugin's Capture settings.
2. Run an authorized Editor, Debug, DebugGame or Development build. Do not assume services exist in Shipping.
3. In desktop Session, enter the target LAN IPv4 address and select Connect to device. The normal plugin port is 41855.
4. Inspect announced defaults/capabilities, then Start remote trace. Stop remote trace finalizes it; analysis starts after stop.

The plugin targets UE 4.27–5.8 in Editor, Debug, DebugGame and Development client builds. Rebuild it for your engine minor version and target platform; an APK or binary from another engine installation is not interchangeable. The desktop native readers require a compatible registered UE 5.5–5.8 installation.

If connection fails, check that the game is running, the capture service is allowed in this build, the IP is current and the network/firewall permits the port. A green Meta GPU Profiling switch does not prove the Unreal capture service responds. See [Troubleshooting](doc:troubleshooting).


## From capture to analysis {#capture-flow}

Workflow diagram. Only captured sources can be analyzed.

```diagram
{"type": "flow", "title": "From capture to analysis", "nodes": [{"id": "n0", "label": "Choose sources", "x": 0, "y": 0}, {"id": "n1", "label": "Record target", "x": 310, "y": 0}, {"id": "n2", "label": "Stop and finalize", "x": 0, "y": 100}, {"id": "n3", "label": "Open container", "x": 310, "y": 100}, {"id": "n4", "label": "Load providers", "x": 0, "y": 200}, {"id": "n5", "label": "Select and inspect", "x": 310, "y": 200}], "edges": [{"from": "n0", "to": "n1"}, {"from": "n1", "to": "n2"}, {"from": "n2", "to": "n3"}, {"from": "n3", "to": "n4"}, {"from": "n4", "to": "n5"}]}
```
