# Rendered Quest GPU test project

The test is a separate Development APK, `org.cyinsights.gputest`, built with
the current CyInsights plugin. It never uses or changes the user's game. It
contains 64 rotating cubes, a movable directional light and a camera. An extra
512 × 512 scene capture alternates on/off every ten seconds, with a native
bookmark and log message identifying the change. These are real rendered views;
the test does not supply synthetic GPU events or split a Vulkan pass to invent
subpass durations.

Generate into a new, disposable directory:

```powershell
python Tools/create_quest_gpu_test.py --output build/quest-gpu-test
```

Build with an explicitly selected, licensed UE 5.5 installation and its Android
SDK/NDK. Example arguments to that engine's `RunUAT.bat`:

```text
BuildCookRun -project=<generated-project.uproject> -nop4 -platform=Android
-cookflavor=ASTC -clientconfig=Development -build -cook -stage -pak -package
-archive -archivedirectory=<output> -map=/Engine/Maps/Entry -unattended -utf8output
```

The test's Android manifest declares optional hands support and its permission.
The rendering workload needs no controller or hand interaction. This lets the
headset's hand-driven shell launch it without a controllers-required gate; it
does not change the CyInsights plugin's permissions or the user's game.
See [Meta input declarations](https://developers.meta.com/vr/essentials/app-compatibility/).

The diagnostic project disables persisted Vulkan pipeline cache loading and
chunked PSO caching (`r.Vulkan.PipelineCacheLoad=0`,
`r.Vulkan.UseChunkedPSOCache=0`). On the tested Adreno/runtime combination,
reloading this test's existing cache repeatedly crashed in `vkCmdBindPipeline`;
starting without loading it rendered successfully. Two cold starts of the
updated APK succeeded with existing cache files left in place. These settings
apply only to this disposable workload, not to the plugin or the user's game,
and its startup time is not a representative game-startup benchmark.

The build compiles the project and plugin against installed engine libraries.
The engine's basic shape and Entry map remain external licensed inputs; their
cooked copies and the APK must stay outside Git. The generator refuses to
overwrite an existing project. It neither installs nor launches anything.

For device validation, install only this test package. Enable detailed GPU
profiling before launching it, keep the headset active, and confirm its game
frame counter advances via the plugin's capture endpoint on **41856** (the
separate test port avoids an existing game's endpoint on 41855). Record more than
twenty seconds so the two workloads appear in the same recording. CyInsights'
normal remote capture can collect the Unreal and Meta sources together. A
successful command, advertised source, nonempty trace or nonzero counter count
does not establish hardware stage delivery.

The developer probe uses the production capture coordinator:

```text
CyMetaGpuProbe --remote <Quest-IP> org.cyinsights.gputest 25 <new-output>
  --no-restart --port=41856
```

Launch the installed test first and accept any Meta controller/launch dialog.
The no-restart option preserves that running instance during the recording.

Decode the result with the native timing reader, then inspect the actual cache:

```text
python Tests/QuestGpuTimingSummary.py <timing.cynt> --output <gpu-summary.json>
```

The report preserves recorded group names/depths and distributions of their
timestamp intervals. It counts intervals above 2 µs to distinguish visible
instrumentation from unresolved subgroup costs. This threshold is a diagnostic,
not a claim that smaller costs are zero. Inspect Meta raw stage packets and valid
aligned stages separately using `CyMetaGpuProbe --decode <meta-gpu.pftrace>`.
Do not run competing collectors at the same time or assign surface ordinals to
Unreal passes without evidence. On the Quest 3 firmware tested on 17 October,
an additional `ovrgpuprofiler -t1 -v` cross-check was followed by a crash in the
Meta/Adreno profiler layer. It is not part of the normal validation procedure;
preserve the crash log and verify process readiness before another recording.

Finally open the common `.cycapture` in the native app and inspect the GPU tracks.
An isolated Vulkan probe or a Windows rendered capture does not validate this
packaged Quest workflow. Record device/runtime, exact APK/plugin versions, source
packet counts, observed groups and any missing/unaligned data in private local
reports before declaring this test successful.

To regression-test the optional stock-5.5 dispatch slot separately, compile
`Tools/QuestVulkanPassProbe.cpp` together with the production
`Plugins/CyInsights/Source/CyInsights/Private/CyInsightsVulkanPasses.cpp`, using
the Android NDK with `--target=aarch64-linux-android29 -std=c++17 -O2
-DCY_TEST_COLLECTOR=1 -DCYINSIGHTS_VULKAN_PASSES=1
-DCYINSIGHTS_VULKAN_END_PASS2=0 -I Tests/QuestVulkanShim -lvulkan
-static-libstdc++`. This variant deliberately omits the EndPass2 dispatch symbol
and pairs BeginPass2 with the classic end. Omitting the last definition exercises
the Meta-style BeginPass2/EndPass2 pair. The shim only replaces Unreal lifecycle
and trace plumbing; Vulkan calls execute on the device's actual driver.
