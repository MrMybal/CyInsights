# Frames and probable causes {#title}

Frames follows the timing viewport. Choose the target budget: 60 FPS is about 16.67 ms, 72 FPS 13.89 ms, 90 FPS 11.11 ms. Slow-frame navigation finds candidates; it does not prove their cause.

## Interpret colors {#interpret-colors}
A cause background highlights suspected GameThread, RenderThread, GPU or mixed bottlenecks when enough evidence exceeds the budget. Neutral means no attributed cause, not proof that everything is fast. Scope category colors are independent of this diagnostic background.

Recognized waits/pacing are separated from active work. EngineLoop/Frame durations may include synchronization or idle time. Compare active intervals and known waits. A long wait on one thread may be a consequence of another thread or the GPU.

## Pipeline limits {#pipeline-limits}
CPU and GPU can process different frames concurrently. A GPU scalar observed during a game frame may be delayed and does not establish exact GPU execution-frame identity. Parent scopes overlap children. Nested inclusive totals across threads are not utilization percentages.

Use a short interval, investigate the suspected domain, compare several bad frames with a nearby good interval and verify with a controlled capture. See [GPU evidence](doc:gpu-desktop) and [Comparison](doc:comparison).
