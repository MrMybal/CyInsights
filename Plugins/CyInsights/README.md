# CyInsights Unreal plugin

Our Trace, Runtime, Editor and Quest integration sources are MIT-licensed (LICENSE).
Unreal Engine and OpenXR dependencies retain their own licenses and are supplied
by your licensed engine installation. See ../../Docs/Licensing.md.

Copy **only this plugin** into your own Unreal project's Plugins directory. Native
CPU/GPU capture works without the extra Windows collector. The optional internal
`CyInsightsGpuLoader` and `CyInsightsGpuWindows` modules use `CyInsightsGpu.addon64`,
built from ../../Tools/WindowsCollector, and are disabled by default. They reuse
collector code with its license notices, without the CyGPUInspector plugin or app.
Enable them in Project Settings > Plugins > CyInsights - Windows GPU collector
(restart required). Configure ReShade's add-on-capable runtime and connect the
local process in the CyInsights application's Session > Windows collector panel.
Capture detail options remain in the main CyInsights capture settings; texture
and buffer readbacks are off by default. Quest metrics use Unreal's OpenXR hooks
and are embedded in the native trace; no ReShade runtime is used on Android.
Only Debug, DebugGame and Development client/editor configurations are enabled;
Shipping, Test and Server are excluded by the descriptors and build rules.
See ../../Docs/RemoteCapture.md and ../../Docs/CapabilityCoverage.md.

Version 0.1.2 restores prompt Android TCP listener rebinding after an explicit
game restart and reports startup/frame readiness to the PC. The corrected app
captures an already-running Quest game by default; automatic restart is off.
Replace this source plugin and rebuild/reinstall a Development APK to apply
the Android listener change. Capturing without restart also works with the
earlier plugin. See [Remote capture](../../Docs/RemoteCapture.md).

Source compatibility targets UE 4.27 and 5.0–5.8. Tested installations, the UE4
capture stop semantics and conditional Quest metrics are documented in
[Unreal compatibility](../../Docs/UnrealCompatibility.md). This plugin can also
capture native traces on Windows with the extra collector disabled.

### Quest GPU completion timestamps

Le profil de base active `GPU completion timestamps` (Project Settings > Plugins > CyInsights > Quest). La même option `quest_gpu_completion` est accessible depuis le PC et le SDK. Elle demande des timestamps de fin de pipeline pour le profiler Vulkan historique, avec un adaptateur du dispatch exporté sur les versions dont la CVar est sans effet. Le moteur reste inchangé. Le mode réellement appliqué est conservé dans le manifeste ; les sous-passes par tuiles peuvent rester indissociables. Pour appliquer cette révision, remplacer le plugin source, reconstruire/ré-empaqueter le jeu Development et enregistrer une nouvelle capture. Voir `Docs/QuestGpuFrames.md` dans le dépôt pour les preuves et limites.

### Quest Vulkan pass boundaries

The basic Quest completion option also enables supplemental graphics render-pass boundary queries. These are embedded in the same `.utrace` and decoded by the updated local reader into a separate Vulkan render-pass track. Two queries per pass; no forced pass splits or image/buffer readbacks. CPU alignment remains approximate, and shared tile subpasses do not have independent measured costs. Rebuild/repackage the Development game after updating the plugin; old captures cannot gain these measurements retroactively. See [QuestGpuFrames](../../Docs/QuestGpuFrames.md) for capabilities and hardware validation limits.


The desktop app can optionally coordinate Meta Quest system GPU profiling over
authorized ADB and include raw Perfetto in the same capture. This plugin reports
its Android package/process and native clock/session identity for that join.
It does not start privileged system Perfetto from inside the game. See
[Meta GPU test defaults and limitations](../../Docs/MetaQuestGpu.md).


The Quest pass collector also records observed direct/indexed/indirect draw
arguments per subpass, including secondary buffers recorded during capture.
No additional GPU query or resource readback is introduced. Individual subpass
costs and Unreal group names remain unavailable. Make a new capture with this
plugin and an updated native reader. See [subpass contents](../../Docs/QuestSubpassContents.md).
