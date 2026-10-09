# Installed Unreal task decoder

This optional adapter reads TaskTrace lifecycle events through the **installed**
Unreal Insights Core and TraceAnalysis DLLs. Only `NativeTasks.cpp` is compiled.
No Unreal source, engine binary or plugin is rebuilt by this CMake project.
Epic headers/import libraries are build inputs from a licensed local installation;
Epic DLLs, headers and import libraries are not copied into the application release.
Their original Unreal license applies. This project's own adapter source is
MIT-licensed; see LICENSE. This grant does not cover Epic headers, code or
binaries. Public distribution of an adapter binary that qualifies as Engine
Tools remains subject to Epic's permitted distribution channels; see
../../Docs/Licensing.md. This repository distributes the adapter source only.

From an x64 Visual Studio developer prompt:

```powershell
cmake -S Tools/NativeTasks -B build/NativeTasks-5.6 -G Ninja `
  -DCMAKE_BUILD_TYPE=Release -DCY_UE_ENGINE=C:/Example/UE_5.6/Engine `
  -DCMAKE_RUNTIME_OUTPUT_DIRECTORY=C:/Example/CyInsights/bin/Release/NativeTasks/5.6
cmake --build build/NativeTasks-5.6
```

Use the matching installed engine path and `NativeTasks/5.x` output for each
supported version (5.5–5.8). The tested 5.8 headers require its newer installed
MSVC 14.44 toolchain; select both its compiler **and** developer environment.
The tested matrix command is in `build/release-20261009.4-audit/build-helper-matrix.cmd`.

For an independent decoder run, use the corresponding `Engine/Binaries/Win64`
as the working directory so Windows resolves the installed Epic dependencies:

```powershell
$taskDecoder = 'C:/Example/CyInsights/bin/Release/NativeTasks/5.6/CyNativeTasks.exe'
Start-Process -FilePath $taskDecoder -WorkingDirectory 'C:/Example/UE_5.6/Engine/Binaries/Win64' `
  -ArgumentList @('C:/Example/captures/input.utrace','C:/Example/captures/tasks.csv') -WindowStyle Hidden -Wait
```

The application does this automatically from its Tasks panel. Output is an
internal streaming journal, not a new trace format. The completion row is
required before publishing an index. TaskTrace protocol 0 and recognized 64-bit
field schemas are supported; missing/unsupported data is reported explicitly.
Original traces are opened read-only. See `Docs/TraceAnalysis.md` for semantics,
missing phases, candidate scopes and currently undecoded relationships.
