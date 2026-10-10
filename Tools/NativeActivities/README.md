# Installed Unreal activity reader

This optional, MIT-licensed adapter reads native network, file, asset loading,
LLM and allocation summary providers from a licensed **installed** Unreal
Insights. Only our adapter is compiled. No Epic source, DLL or import library is
copied into the repository or rebuilt. Epic's license applies to those separate
inputs; this directory's LICENSE covers our source only. Public adapter binary
distribution can qualify as Engine Tools and remains subject to the conditions
explained in [Licensing](../../Docs/Licensing.md). Git distributes source only.

Build in an x64 Visual Studio developer prompt, for each installed analyzer
version (tested 5.5–5.8):

```powershell
cmake -S Tools/NativeActivities -B build/NativeActivities-5.8 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCY_UE_ENGINE=C:/Example/UE_5.8/Engine `
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Example/CyInsights/bin/Release/NativeActivities/5.8
cmake --build build/NativeActivities-5.8
```

Use MSVC and its developer environment appropriate for the headers, including
MSVC 14.44 for the tested 5.8 installation. The reader links the installation's
UnrealInsights Core, TraceAnalysis and TraceServices import libraries. The main
application has no Epic linkage. The application runs the reader from the
selected `Engine/Binaries/Win64` so the installed dependencies are resolved.
An independent invocation takes three arguments:

```text
CyNativeActivities trace.utrace activity.csv engine-binaries-directory
```

`activity.csv` is an internal, versioned streaming journal (`CYACTIVITY1`) with
lane definitions, observations and a required count-bearing completion row.
Seconds use the providers' native session clock. An observation's 64-bit value
means bytes, content bits, or native memory sample/count according to its lane
and flags. Signed LLM gauges use the two's-complement bit pattern with flag 64.
The final LLM sample is queried separately to retain its actual native value.
Malformed/truncated analysis does not publish a complete application index.

`CY_ACTIVITY_BUILD_FIXTURE=ON` builds an optional **synthetic** network/LLM/file
and CPU/frame test writer. Unlike the reader, it links the installed Editor's
tracing-enabled Core/TraceLog; the Insights program disables its own writer.
It requires those local Editor import libraries. This is a test fixture, not a
measurement of hardware, disk or real network traffic. No engine module is built.

See [NativeActivities](../../Docs/NativeActivities.md) for UI and capture semantics.
