# Capture profiles and overhead {#title}

Start light, then add the source needed for your question. Basic defaults retain CPU/GPU scopes, frames, anchors and lightweight per-frame observations. Extra Stats aggregation, process memory, task/network/file/loading/log/LLM channels, detailed Windows payloads and image/texture readbacks are separate options.

Unreal Project Settings > Plugins > CyInsights saves defaults for future captures. Session > Capture options initially uses Use plugin project defaults. Connect retrieves them; Copy plugin defaults for this capture permits a session override without changing the project. SDK capture options use the same concepts.

Stats groups must exist, be compiled and active. LLM needs instrumentation; detailed allocations need startup flags. Native Quest groups need RHI timestamps and compiled scopes. Per-frame observation does not establish exact execution-frame identity.

## Escalate only as needed {#escalate-only-as-needed}
1. Basic scopes/frames locate suspicious intervals.
2. Selected stats/activities test a hypothesis.
3. Detailed GPU stages/command timing provide deeper evidence.
4. Images/readbacks are explicit extra diagnostics.
5. Recheck final performance with a light profile.

There is no universal zero-overhead promise or fixed cost for every game. Note the active profile in reports. Faster polling cannot accelerate a slow provider. Keep detailed modes bounded/cancellable and distinguish their cost from the game's baseline.
