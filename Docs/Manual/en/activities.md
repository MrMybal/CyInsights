# Network, storage, loading and memory {#title}

Open Network / storage / memory (Activities) and load the provider you need. Activity readers are separate optional adapters. A timing-only trace does not guarantee activity channels. Filter kind/lane and select a short interval.

- Network: recorded packets/content require NetTrace and suitable instrumentation. Content bits are not automatically total wire bytes, socket traffic or a complete external packet capture.
- Storage: recorded Unreal file operations, not all OS disk operations of every process.
- Loading: engine asset/load events when present, distinct from raw filesystem traffic.
- Memory: available gauges/tags. LLM needs its instrumentation; detailed allocations require startup tracing flags. Full memory-callstack exploration is not complete here.

Start with lane summaries and request events only for a suspect lane. Compare overlap with CPU/GPU work while preserving clock/availability. A packet near a hitch is correlation, not proof of its cause.

Enable sources before recording in [Profiles](doc:profiles). Opening this panel or an AI query cannot instrument an old trace. Messages contains application diagnostics; a complete native Unreal log reader is not provided in this build.
