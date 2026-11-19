# Third-party notices

CyUI's own sources, documentation, Python modules, browser bridge and adapters
are distributed under the root [MIT license](LICENSE). Dependencies retain their
own copyright and licenses. Selecting a module does not relicense its dependencies.

| Component | License | Included / optional | Notice |
| --- | --- | --- | --- |
| JetBrains Mono Regular 2.304 | SIL OFL 1.1 | Embedded unmodified in Core; bundled with the Python package | [OFL notice](third_party/jetbrains-mono/OFL.txt) |
| Dear ImGui | MIT | Bundled: built and linked by CMake when `CYUI_IMGUI_TARGET` is empty, and by the demo; hosts normally supply their own target | [third_party/imgui/LICENSE.txt](third_party/imgui/LICENSE.txt) |
| Dear ImGui SDL3 backends | MIT | Unmodified backends matching the bundled core, only for optional native SDL3 examples | [License and pinned source](third_party/imgui-sdl3/README.md) |
| SDL3 3.4.16 | zlib | Optional example host only; installed SDL3 >= 3.4 or a SHA-256-verified pinned fetch; never linked into CyUI modules | [License](examples/markdown/SDL-LICENSE.txt), [fetch revision](examples/markdown/CMakeLists.txt) |
| stb_image and stb_image_write | MIT or public domain, at the user's choice | Optional native Markdown example codecs only | [License and pinned source](third_party/stb/README.md) |
| stb_truetype, stb_rect_pack, stb_textedit (Sean Barrett), as `imstb_*.h` | MIT or public domain (Unlicense), at the user's choice | Bundled inside the Dear ImGui copy and compiled with it | License text at the end of each [third_party/imgui/imstb_*.h](third_party/imgui) file |
| ImGuiColorTextEdit, Johan A. Goossens | MIT | Bundled only for the optional C++ CodeEditor target | [third_party/texteditor/LICENSE](third_party/texteditor/LICENSE) |
| Lexilla, selected lexical engines | HPND-style permissive license (Neil Hodgson and contributors) | Bundled and compiled only with Code or its dependents; no Scintilla editor is linked | [Lexilla license](third_party/lexilla/License.txt) |
| Scintilla public headers | Same permissive license | Header-only interface for Lexilla; no Scintilla UI sources | [Scintilla license](third_party/scintilla/License.txt) |
| SciTE keyword/property data | Same permissive license | Generated built-in language profiles | [SciTE properties license](third_party/SciTE-Properties-License.txt) |
| MD4C | MIT | Bundled CommonMark/GFM parser, only with DocumentViews | [License](third_party/md4c/LICENSE.md), [pinned source and C++ casts](third_party/md4c/UPSTREAM.md) |
| pugixml 1.15 | MIT | XML parsing only with OfficeIO | [License](third_party/pugixml/LICENSE.md), [pinned version](third_party/pugixml/UPSTREAM.md) |
| miniz inflate | MIT | Raw DEFLATE decompressor, only with DocumentIO | [License](third_party/miniz/LICENSE), [pinned source](third_party/miniz/UPSTREAM.md) |
| markdown-it-py and linkify-it-py | MIT | Python document parsing; external pure Python dependencies | Preserve the installed packages’ licenses, including mdurl and uc-micro-py notices |
| ImGuizmo | MIT upstream | Optional host-supplied target | Preserve the license of the exact host dependency |
| Chromium Embedded Framework | BSD-style, three conditions | Optional host-supplied SDK/runtime; no CEF binaries in this repository | Preserve the selected SDK's LICENSE.txt and Chromium CREDITS.html, plus its component notices |
| imgui-bundle | MIT package, with separately licensed bundled components | Optional external Python UI dependency | Preserve notices in the installed package/distribution |

The TextEditor source is a pinned upstream snapshot. File hashes, upstream URL
and the serialization patch are recorded in
[VERSION.json](third_party/texteditor/VERSION.json). The patches preserve an absent
final newline, add revision-aware shared syntax spans, per-view text zoom and brace-only scope guides; the original copyright and MIT license remain intact.

CEF is distributed separately for each OS/architecture. Its Chromium components
have additional notices and licenses; the CEF BSD license alone is not the complete
notice set for an application shipping the runtime. Preserve the distribution's
license/credits files, and review the exact codec/media build you choose to ship.
No CEF runtime is downloaded automatically by CyUI's CMake configuration.

The bundled JetBrains Mono font remains under SIL OFL 1.1, independently of CyUI's
MIT code. Preserve its copyright and OFL notice when distributing binaries.
The original font hash/version and generated C++/Python assets are checked by
`python tools/embed_code_font.py --check`. No font download or system installation
is needed at build or startup. The current native renderer does not shape ligatures.

System fonts (for example Segoe UI on Windows) are loaded from the operating
system or host-provided font paths. CyUI does not distribute those font files.

The optional Unreal Editor connector uses the host's Unreal Engine installation.
Unreal Engine is separately licensed by Epic; no engine source or binary is
included in CyUI or the prepared plugin's source snapshot. CyUI's own connector
remains covered by the root MIT license.

The Cy* applications surveyed for common UI requirements are not incorporated
dependencies. Their code, assets and web builds retain their separate licenses.

Syntax source revisions and archive hashes are recorded in
[code-syntax.lock.json](third_party/code-syntax.lock.json). Generated language data
comes from the pinned SciTE properties and original CyUI shader profiles, with
notices retained. No Notepad++ source, GPL configuration or plugin binary is bundled.
The built-in list describes syntax modes; it does not imply a compiler, language
server, debugger or compatibility with the Notepad++ plugin ABI.

## CyDoc documentation reader

The offline documentation uses CyDoc 0.1.0 (MIT), built from revision
`e038c474577e1f305df3eba421f2b9c03b0f1d8d` of
[MrMybal/CyDoc](https://github.com/MrMybal/CyDoc). Its MIT permission and copyright
notice is distributed in `Documentation/DocData/CyDoc-LICENSE.txt`.
The selected image and gallery modules add no Three.js runtime. Markdown parsing,
highlighting and bundling dependencies are generation tools, not reader dependencies.
