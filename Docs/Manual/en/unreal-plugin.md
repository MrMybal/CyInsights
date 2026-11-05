# Unreal plugin and connection {#title}

Copy the CyInsights plugin source folder into the project's Plugins directory, enable it and rebuild for that engine/platform. The plugin is CyInsights; the optional internal Windows collector does not require the separate CyGPUInspector application.

## Connect {#connect}
1. Open Project Settings > Plugins > CyInsights, or the plugin's Capture settings.
2. Run an authorized Editor, Debug, DebugGame or Development build. Do not assume services exist in Shipping.
3. In desktop Session, enter the target LAN IPv4 address and select Connect to device. The normal plugin port is 41855.
4. Inspect announced defaults/capabilities, then Start remote trace. Stop remote trace finalizes it; analysis starts after stop.

The intended engine range is 4.27–5.8, not proof that every platform/version was run. Windows build coverage is documented for 5.5–5.8; local full Editor validation on 4.27 has SDK-file gaps. Android/Quest tests cover specific builds. Rebuild the plugin for your actual minor version rather than reusing an unrelated engine binary.

If connection fails, check that the game is running, the capture service is allowed in this build, the IP is current and the network/firewall permits the port. A green Meta GPU Profiling switch does not prove the Unreal capture service responds. See [Troubleshooting](doc:troubleshooting).
