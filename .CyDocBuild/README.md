# Documentation development (outside the human manual)

The official CyDoc dependency is pinned at
`6ddf9a582defb2f11fbb3f992afef6f98dcadc93`, version 0.1.0. Its unmodified
`npm pack` archive and native helper are in `ThirdParty/CyDoc` with SHA-256 hashes.
The source workspace is `.CyDocBuild`, the distributable directory is
`Documentation`. No content is imported from repository READMEs, AI instructions,
history, migration records or source trees.

From the application root, using Python 3.11+ and Node.js 22+:

```text
npm --prefix .CyDocBuild ci --omit=peer --ignore-scripts
python Tools/build_docs.py
python Tests/DocumentationTests.py
python Tools/check_pages_export.py Documentation
```

`Tools/build_docs.py` and `Tools/sync_cydoc.py` are compatibility entry points;
their implementations and the npm lockfile belong to this authoring workspace.
The updater is `.CyDocBuild/sync_dependency.py`. It refreshes only from a clean
isolated checkout of the explicit pinned revision. Do not alter library bundles.
Version 0.1.0 is shared by upstream revisions, so compare the snapshot SHA, not
only the npm version. A refresh requires regenerating the lockfile and npm ci.

The 24 existing user guide IDs are retained. Five section pages and six public
API groups provide a tree; each operation is a child of its relevant group.
Legacy source/archive page IDs redirect to the home page rather than exposing
the former internal content. The source `cydoc.config.json` is usable with the
official CLI; the build wrapper writes a temporary sibling config to select the
current product version and a clean staging export. `cydoc check` writes output.

Both languages are complete. English guide revisions are canonical body hashes;
French sourceRevision values record human-reviewed counterparts. Update them
only after translating changed meaning. The API adapter uses CyDoc's documented
`cydoc/api-import` entry point and a neutral structured input from CyTool.json;
it writes Markdown inputs with parent metadata before the official CLI compiles
them. `api-fr.json` holds all translated prose, including schema descriptions.
`api-revisions.json` pins the reviewed contract. A contract change blocks the
build until the translation and its fingerprint are deliberately reviewed.
Only short public call signatures and schemas are included, never implementation
bodies, sourceIndex/source listings or source-directory copies.

The image module supports the three actual screenshots. Flow diagrams are
compiled SVG and the illustrative tile grid is a local SVG. Gallery, video,
GIF, graph and mesh add no justified reader here; Three.js is omitted. The
application logo is local. Minimal controls retain keyboard and touch access.
`host.js` is the application-owned preference/legacy-fragment adapter; it does
not rewrite the reader interface. The build adds the local-only CSP, dependency
notices, app-specific DocAI sidecar text and an export hash manifest from source
inputs automatically. Do not repair generated files by hand.

CMake copies the whole Documentation tree beside the EXE; Android packages it
under assets/Documentation. Help/F1 and contextual routes open Documentation.html
directly. The release packager includes the complete directory in desktop and
plugin archives. It excludes this authoring workspace and no longer includes the
repository's internal Docs tree as a second manual. AI SDK source/dependency files
remain a separate runtime deliverable and never enter the reader's search.

For GitHub Pages, `python Tools/build_docs.py --pages` stages only the checked
manual under `.CyDocBuild/work/pages`. Its generated index compatibility redirect
preserves former README fragments and query strings; Documentation.html remains
the actual reader. The Pages workflow is updated for this layout but is not
deployed until the application changes are explicitly pushed.

Optional real-browser QA uses a separately installed playwright-core development
dependency; no browser package is distributed with the application:

```text
node Tests/DocumentationBrowser.mjs <playwright-core/index.mjs> <browser-executable>
```

Keep test outputs and screenshots in work/, and useful private notes in internal/.
The previous integration/manual records are retained here as history only.
