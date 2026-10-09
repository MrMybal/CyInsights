# CyGPUInspector Unreal integration for CyInsights

This copy's owned loader, runtime, editor and plain-C interface sources are
MIT-licensed (LICENSE), authorized by copyright owner Cyberalien. The original
upstream repository is unchanged. Epic code is not included in this source tree.
See ../../Docs/Licensing.md for scope and distribution constraints.

Use the separately buildable MIT collector in ../../Tools/WindowsCollector.
The runtime ReShade DLL is installed separately and keeps its own license.
Older copied binaries are excluded from Git and retain their historical licenses.
All modules permit only Debug, DebugGame and Development; Server is excluded.
The integration targets UE 4.27 and 5.0–5.8. See the tested-version matrix and
remaining validation gaps in [Unreal compatibility](../../Docs/UnrealCompatibility.md).
