# Open and manage recordings {#title}

Use File > Open capture or Ctrl+O. Multiple recordings open in independent tabs. The tab context menu can detach a capture to a native window. Each document keeps its own viewport, filter, selection and analysis. Closing a document does not delete its source.

## Formats {#formats}
- .utrace: original Unreal trace, decoded through available native readers/export providers.
- .cycapture: versioned container for available Unreal timing, optional Meta GPU events, counters and tile/image recordings.
- .pftrace: supported Meta/Perfetto source, not automatically a complete Unreal session.
- .cyovr: companion tile snapshot; not a full Unreal timing session by itself.

Preparation runs in the background. Use progress/cancellation instead of launching duplicate analyses. Persistent disk indexes accelerate later reads, but a first scan still costs time. Cancelled/incomplete providers are unavailable, not zero cost. Native providers and ordinary counter loading can follow different read paths.

Original traces remain read-only. Indexes, exports and containers are separate files. Incoming .utrace data is finalized before native analysis; received byte counts alone are not live decoded frame analysis.

Next: [Timeline](doc:timeline), [Counters](doc:counters), [Comparison](doc:comparison).
