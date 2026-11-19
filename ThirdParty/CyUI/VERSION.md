# CyUI version notes

[Français](VERSION.fr.md) · [README](README.md) · [Published releases](https://github.com/MrMybal/CyUI/releases)

## Versions and status

| Component | Version | Availability |
| --- | --- | --- |
| C++ library and Python package | 1.2.0 | Current source version in CMake and `pyproject.toml`. |
| Standalone gallery demo | 1.2.0 | Published portable Windows x64 / DirectX 11 build. |
| Bundled CyDoc reader | 0.1.0 | Documentation dependency in current source builds, with image and gallery modules. |

**CyUI is in active development.** The project does not declare an Alpha, Beta
or Stable product stage. The library's version number and the published demo's
release flag do not establish a stable API or platform validation.

## Published demo 1.2.0

The [v1.2.0 release](https://github.com/MrMybal/CyUI/releases/tag/v1.2.0) provides
the Windows gallery and its SHA-256 checksum. Its documented contents include:

- Native widgets, layouts, five themes and live English/French selection.
- Audio displays, plots, editable curves and timelines.
- Node graphs with synthetic image previews, a content browser and property inspector.
- Code editing and syntax modes, Diff views, commands and menu/panel filters.
- Dependency licenses and source revision information.

[Download the Windows x64 ZIP](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip)
and [its checksum](https://github.com/MrMybal/CyUI/releases/download/v1.2.0/CyUI-Demo-1.2.0-windows-x64.zip.sha256).
Extract the whole archive before running the executable.

## Current sources after the published demo

The source version remains 1.2.0. The newer sources include the complete
English/French CyDoc manual, indexed public-header references and local help
entry points. Current demo builds, Python wheels and Unreal packaging include
the reader and its resources. The refreshed static showcase also includes it.

These source additions are not part of the older published 1.2.0 demo ZIP.
Build from current sources to use this help integration; reading the exported
manual itself needs no build tools, server or connection.

## Documented limitations

- **Platforms:** the published executable is Windows x64 only. Linux/macOS library
  sources and CI configuration exist; local Windows checks do not validate
  execution on those systems. CEF deployment requires validation for each target.
- **Host responsibilities:** graphs and views display application-supplied data.
  Audio decoding, FFTs, DSP, shader execution, file operations, repository access
  and language services remain in the application.
- **Native code editor:** CodeEditor requires C++20. Syntax modes provide lexical
  highlighting, not a compiler, debugger or language server. The Python editor
  uses multiline editing with a syntax preview and does not expose the full
  native TextEditor backend.
- **Optional integrations:** the ordinary demo bundles no CEF or Unreal runtime.
  The Unreal connector targets Editor C++/Slate, not runtime/UMG. External SDKs,
  fonts and dependency notices remain the host's responsibility.
- **Documentation:** keep the entire `Documentation/` folder together. The
  reader uses JavaScript; saved preferences and personal annotations depend on
  browser storage permissions. External references and downloads need a connection.

See the [platforms guide](Documentation/Documentation.html#/en/overview/platforms),
[Web guide](Documentation/Documentation.html#/en/web) and
[Unreal guide](Documentation/Documentation.html#/en/unreal) for the documented scope.

## Bugs and feedback

The published 1.2.0 release notes do not provide a bug-by-bug list. Consult the
[issue tracker](https://github.com/MrMybal/CyUI/issues) for reports and discussion.
