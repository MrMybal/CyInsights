# CyDoc source dependency

Official repository: https://github.com/MrMybal/CyDoc

Pinned revision: `6ddf9a582defb2f11fbb3f992afef6f98dcadc93` (version 0.1.0).
The archive is the unmodified result of the documented `npm ci` / `npm pack`
workflow in an isolated checkout. `snapshot.json` pins its bytes and the native
route helper. The package is not installed from the public npm registry.

The application uses the image reader only. Three.js is not an application or
manual dependency. Source authoring, the locked npm toolchain and refresh scripts
belong to `.CyDocBuild/`; only the generated `Documentation/` tree is shipped.
The native C++ helper remains here for application builds. MIT license preserved.
