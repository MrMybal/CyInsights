# Read GPU evidence {#title}

Start with measured native GPU groups. A single MobileSceneRender bar means only that timing level is available. A different panel cannot recreate missing child timestamps.

## Distinguish sources {#distinguish-sources}
- Native Unreal GPU scopes: measured when the RHI/build supplies timestamps and instrumentation.
- Vulkan pass/subpass contents: command arguments, attachments and structure. CPU-side command contents are not per-subpass hardware costs.
- Optional Windows collector: commands, pipelines, shaders, resources and available GPU timestamps. Enable explicitly; ordinary .utrace analysis needs no separate CyGPUInspector app.
- Meta hardware stages/tiles: a distinct [Quest source](doc:quest-capture).

GPU frame can show pass timelines, draw/dispatch counts and reconstructed dependencies. Derived passes/links are labeled. Command timestamps are opt-in and can be frame-local. A shader/resource description does not establish its duration.

High Render cost alone does not prove a pixel-shader bottleneck: Render can include vertex and fragment work. Correlate shares, texture/memory stalls, dimensions, tiles and controlled changes. Binning is an envelope, not extra independent time to add to all children. Waits and overlap matter.

Images/textures/buffer readbacks are off initially. Detailed profiling can perturb scheduling: use short diagnostics, then retest with a light [profile](doc:profiles). See [Glossary](doc:glossary).
