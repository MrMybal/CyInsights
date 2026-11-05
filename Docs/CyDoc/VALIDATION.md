# CyDoc migration validation — 5 November 2026

## Scope

This replaces the native embedded manual in CyInsights and CyInsightOVR. The
application's trace engines, GPU service, HUD/capture behavior and unrelated
local work are preserved. CyDoc itself is an unmodified pinned package.

## Completed checks

- The official CyDoc CLI compiled and validated every imported page, local
  document link, explicit section/media identifier and asset. The exported
  inventory enumerates all original application reference sources.
- All 24 user chapters have English/French pages and identical explicit anchors.
  Their approved source revisions are tracked separately. The report honestly
  lists untranslated technical/API archives and the annotation extractor's one
  untracked bilingual page; it is not a claim of universal translation.
- The exact operation contract produces 39 API pages with input/output schemas,
  permission/platform metadata and preserved examples. The lexical source index
  contains six allowlisted distributable sources only.
- Structural tests check common anchors, document links, inventory coverage,
  operation parity, translation status, selected readers, dependency/license
  files and every export SHA-256. Native C++ route tests cover fragments,
  Unicode/query escaping and locale fallback.
- A clean Windows x64 Release configuration compiled CyInsightsApp, CyHelpTests
  and CyUiModuleTests. The complete 30-test native component suite passed, including CyDocumentationTests.
- Actual Edge tests opened file URLs with the browser explicitly offline:
  English/French navigation, page/section links, Back, alias resolution, fallback
  references, local image decoding, in-window enlargement/Escape, keyboard focus,
  search and language selection. No external requests or JavaScript exceptions
  were observed. Narrow-window screenshots were inspected. This is the real
  exported reader, not a DOM replacement fixture.
- The Quest Vulkan APK compiled and passed Android APK signature verification;
  its packaged help is the same exporter output. Additional final packaging and
  device checks are recorded below after the last build.

## Remaining limits

- The connected Quest was asleep at the start of migration QA. A successful APK
  build or WebView probe does not prove headset-visible layout, controller focus
  or touch comfort. No game, Beyond project, GPU capture or profiling setting is
  needed for documentation validation.
- Linux/macOS were not available for native host testing. The product's desktop
  UI remains Windows-only; portable CyDoc files/route helpers add no unsupported
  desktop launcher claim.
- CyDoc currently loads all page/search data together; this manual does not add
  segmentation. The export includes technical/source archives, so its initial
  payload is larger than a 24-chapter quick guide. No live C++ semantic index,
  automatic translation, video conversion or mesh capability is implied.
- Reader bookmarks/notes depend on browser/WebView storage. Runtime does not
  download anything. The initial build dependency installation needs registry
  access unless the npm cache is already populated.
- Historical relative page links are remapped locally. Recognized heading
  fragments are retained. Any unmatched old fragment resolves to its page and
  is listed in `legacy-links.json`; unknown repository file links remain visible
  path references rather than broken runtime links.

The older `Docs/Manual/VALIDATION.md` records tests of the retired native reader.
It is historical evidence, not validation of CyDoc or of current GPU/HUD work.

## Native entry and device checks

- The actual Windows executable opened the French timeline section through its
  help launcher from a directory containing spaces and a hash character. Its
  snapshot report confirmed an encoded file URL and no launcher error.
- The Vulkan APK was updated on the Quest using the existing signing key, without
  uninstalling or clearing settings. Before installation, no snapshot, tile
  recording or heatmap was active; configuration preferences were preserved.
- The real packaged Android WebView loaded the English timeline, navigated to the
  French section, selected Nord, and confirmed blocked network/universal-file
  access settings. This validates local text/routes inside Android, not visual
  comfort while wearing the sleeping headset.
- A stricter image probe did not complete PNG decoding while that WebView was
  hidden by the sleeping Quest: the local image URL remained pending, without
  a confirmed decode or visible surface. The asset bytes in the APK are verified,
  but Android image rendering and wearer/controller interactions remain unverified.
  The probe is retained for the next awake-headset check; a text-route success is
  not reported as an image-rendering success.
- Browser tests also exercised high-contrast/Nord themes, touch input and 150%
  device pixel density. The image remained inside the documentation window.
- Final export: 202 locale-specific pages, including 24 English/French guide
  pairs, 99 retained reference/history pages, 39 contract-derived operation pages,
  the six-file source index, extracted help contract and translation report.
- The Windows ZIP packaging check passed with the entire checked help tree and
  license notices. Both Standard and Vulkan APKs built and passed signature
  verification; every packaged help resource matched its export SHA-256.
- The final 30-test native suite passed. The worktree publication audit passed,
  and the retained files inside the pinned CyDoc package were inspected too.
  A long disk-index heading had resembled a token after slug generation; an
  explicit stable section ID fixes that false signal without weakening the audit.
