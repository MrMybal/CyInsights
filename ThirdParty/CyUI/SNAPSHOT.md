# CyUI 1.2.0 modular source snapshot

Upstream: https://github.com/MrMybal/CyUI

Pinned revision: `ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3`
(verified against upstream HEAD on 2 November 2026).

This is a minimal, unmodified selection of upstream files: Core and the Canvas,
Plots, Timeline, Nodes, Inspector, Images and Diagnostics implementations, public
headers, CMake, integration documentation and MIT LICENSE. `source-sha256.json`
records every imported byte. No bundled ImGui, Audio/Gallery implementation,
lexers, text editor, CEF, fonts, binaries or repository metadata are copied.
The distribution retains `LICENCES/CyUI-MIT.txt`.

The host CMake files select the production modules before `add_subdirectory` and
supply the existing ImGui target. The desktop uses all seven optional modules;
Quest only uses Canvas, Plots, Inspector and Images. Dependencies are resolved by
upstream CMake. Demo, Gallery, Audio and CEF are explicitly disabled. Only the
selected families' headers are packaged; another family requires a reviewed
snapshot refresh.

To reproduce the snapshot from an upstream checkout:

```text
python Tools/sync_cyui.py --source <CyUI-checkout>
python Tools/prepare_dependencies.py
```

The sync tool reads the pinned Git objects, never modifies the source checkout,
and removes only obsolete files owned by the previous snapshot manifest. Change
the reviewed revision and module selection in that tool before a future update.
See `Docs/CyUIMigration.md` for the application adapters and remaining API gaps.
