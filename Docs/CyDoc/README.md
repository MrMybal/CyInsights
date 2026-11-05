# CyDoc integration

**[Documentation](https://mrmybal.github.io/CyInsights/#/en/welcome)** ·
[Documentation en français](https://mrmybal.github.io/CyInsights/#/fr/welcome)

CyInsights uses [CyDoc](https://github.com/MrMybal/CyDoc), MIT, version 0.1.0,
pinned at `774f2a7eabf7dd181a98ffc5add115b580b67f42`. The official npm package is
vendored as a package subset with a SHA-256 manifest in `ThirdParty/CyDoc/`. Its
unused upstream demo-authoring script is omitted because it contains a local
development path; every retained source/runtime file is unmodified. Git authentication is
needed only to refresh that dependency; a reader needs neither Git nor Node.

## Read the manual

- Windows: **Help > User manual**, **F1**, or the contextual **Help > Documentation**
  entries. The default browser opens `help/index.html` beside the executable with
  a properly encoded `file://` address.
- Quest: **Help > Open manual**, **English / Français**, or **Help for this page**.
  A dedicated WebView opens `file:///android_asset/help/index.html`. Its Back
  control returns to configuration. It exposes no JavaScript/native interface;
  network loading and access outside the packaged help tree are blocked.
- Keep the complete exported tree. No HTTP server, CDN, remote font, account,
  Node runtime or documentation download is used at reading time. GPU service
  availability does not gate reading. The app's other network tools are separate.

The reader defaults to English and the Nord theme; other reader themes, French,
keyboard navigation, touch scrolling, code tabs, local search, reading positions
and in-window image enlargement are CyDoc features. Escape reduces enlarged
media. Reading data depends on file-origin/WebView storage; session positions
are maintained by CyDoc. Browser preferences and the Android app persist language
independently of the main interface. Exact contextual routes take precedence.

## Content and modules

The export preserves all 24 bilingual user chapters, three actual app screenshots,
the nine explanatory figures, existing application guides, native-reader/plugin
notes, API examples, SDK references and release/validation history. References
keep their original language and display an archive/reference notice. Dates and
old test results are not assertions about the current build.

Only **image** is enabled: screenshots need zoom, pan, annotations and in-window
enlargement. Flow diagrams compile to SVG; the illustrative tile grid is SVG too.
No video/GIF/model/interactive-node assets justify video, gif, mesh, gallery or
graph. Three.js is absent from the application's npm dependencies and export.

The 39 operation pages are generated from the actual `AI/app/CyTool.json` via
CyDoc's neutral `cydoc` API import. Full input/output schemas, permissions and
platform contracts are preserved. The lexical source index contains only five
allowlisted distributable C++/Python/Java source files plus the application's
annotated `HelpContract.hpp`. It is not a semantic C++ index; no Unreal source,
private/device data, build output or dependency tree is scanned. Doxygen/TypeDoc
would add no justified capability to this adapter.

English is the default/fallback. The 24 guides share French page IDs and anchors.
`translations.json` explicitly records approved source revisions; changing English
without updating that record marks French stale. Exact technical/API references
remain untranslated and are reported honestly. CyDoc's annotation extractor does
not accept translation revision metadata, so its one bilingual extracted contract
is reported as untracked. `translation-report.json` and the reader's Translation
status page expose all gaps; no translation is silently invented.

Stable topic IDs and the existing `--manual-view`, `--manual-topic`,
`--manual-language`, `--manual-media` arguments are retained. New
`--manual-anchor` targets a section. The aliases overview/manual/help point to
welcome. A legacy `#manual:topic` fragment is adapted by the host shell.
Repository Markdown links between imported references are mapped to local pages;
recognized historical heading fragments are retained, with any unmatched fragment
reported in `legacy-links.json`. Repository-source links remain readable path references. External citations
are preserved as citations; they are not needed to read the manual, and Android
intentionally blocks opening them in its restricted reader.

## Build and package

Generation requires Python 3.11+, **Node.js 22+** and npm. Run from the app root:

```text
npm --prefix Tools/Documentation ci --omit=peer --ignore-scripts
python Tools/build_docs.py --output build/help
python Tests/DocumentationTests.py
```

The first generation installs locked build dependencies if absent; an initial npm
cache fill needs registry access. Later generation works with those dependencies
installed, and runtime is fully offline. `package-lock.json` pins all transitive
dependencies; optional Three.js is omitted. Do not use a bare registry install of
CyDoc. To refresh the library deliberately from its pinned Git revision:

```text
python Tools/sync_cydoc.py
npm --prefix Tools/Documentation ci --omit=peer --ignore-scripts
```

The maintainer refresh builds the upstream package in an isolated checkout;
its development tools (including optional-reader test dependencies) are not
application or reader dependencies. Do not edit the packaged library or generated
bundles to fix application content.

`Tools/build_docs.py` stages authoring inputs under `build/cydoc-project`, runs the
official CLI **check** (which also exports), copies license notices and produces
an inventory and export hash manifest. Its small app-owned shell selects Nord,
adapts old routes and labels CyDoc's MIT license separately from the application's
AGPL license. CyDoc's compiler, data, resource providers and reader bundles stay
unmodified. Output is restricted to a generated `help` directory under build/bin.

Normal CMake builds validate help and copy it next to CyInsightsApp. Quest
`build.py` packages the same checked tree into `assets/help/` for both APK variants.
`Tools/package_release.py` verifies the export hashes and adds every file to the
Windows ZIP, without Node, npm, caches or build tooling.

Optional real-browser checks (development dependency only):

```text
npm --prefix build/cydoc-browser install playwright-core
node Tests/DocumentationBrowser.mjs build/cydoc-browser/node_modules/playwright-core/index.mjs <browser-executable>
```

See [validation and remaining limits](VALIDATION.md). No repository push or release
is performed by documentation generation.

## GitHub Pages

The public site runs the same complete export as the application's local help.
`.github/workflows/documentation-pages.yml` generates and validates it on pushes
to main, then publishes only `build/help/` with the official Pages artifact and
deployment actions. Pull requests build and validate without deploying.
`Tools/check_pages_export.py` checks the resource allowlist and export hashes.
The build does not collect application captures, runtime settings or caches;
source-backed API pages use the documented distributable-source allowlist.

Pages must use **GitHub Actions** as its publishing source. No generated files
need to be committed, no separate Markdown renderer is involved, and no change
to the application's offline `file://` reader is required. Hash routes select
the locale, page, section and media under the project site's base path.
