# CyInsights Unreal plugin

Our Trace, Runtime, Editor and Quest integration sources are MIT-licensed (LICENSE).
Unreal Engine and OpenXR dependencies retain their own licenses and are supplied
by your licensed engine installation. See ../../Docs/Licensing.md.

Copy this plugin into your own Unreal project's Plugins directory. Windows GPU
capture optionally also uses the sibling CyGPUInspector plugin and a collector
built from ../../Tools/WindowsCollector. Quest metrics use Unreal's OpenXR hooks
and are embedded in the native trace; no ReShade runtime is used on Android.
Only Debug, DebugGame and Development client/editor configurations are enabled;
Shipping, Test and Server are excluded by the descriptors and build rules.
See ../../Docs/RemoteCapture.md and ../../Docs/CapabilityCoverage.md.

Source compatibility targets UE 4.27 and 5.0–5.8. Tested installations, the UE4
capture stop semantics and conditional Quest metrics are documented in
[Unreal compatibility](../../Docs/UnrealCompatibility.md). This plugin can also
capture native traces on Windows without installing CyGPUInspector.
