# CyInsights showcase

Static, bilingual showcase with the complete CyDoc reader beside it. English is
the default; `?lang=fr` selects French. No service, remote font, CDN, analytics or
web framework is required. Keep this source workspace out of application packages.

From the repository root:

```text
npm --prefix .CyDocBuild ci --omit=peer --ignore-scripts
python Tools/build_site.py
python -m unittest discover -s Tests -p test_site.py
python -m http.server 8765 --bind 127.0.0.1 --directory build/site
```

Open `http://127.0.0.1:8765/` or add `?lang=fr`. The output is `build/site`.
The build may reuse an already checked manual with `--skip-docs`. It refuses to
replace an unrecognized or edited output directory. Only the allowlisted reader,
showcase and hashed local assets are copied. The build never publishes anything.

Styles, scripts and images use content-addressed filenames, renewed when their
bytes change. The default HTML is complete English even before JavaScript runs.
The language selector updates text, image descriptions, menu labels, title,
description, the URL and every contextual documentation link. Native dialogs
provide image enlargement, Escape, arrow-key browsing and focus restoration.
Reduced-motion preferences are honored. GitHub stays visible on small screens.

`Documentation.html` and `DocData` remain the byte-identical CyDoc export. The
showcase recognizes older `/#/en/...` and `/#/fr/...` links and redirects them to
that reader. Application help and offline packaging continue to use the existing
documentation build; they do not depend on the showcase.

The pinned download tag follows `VERSION.json`; check that its seven linked assets
exist before a future publication. No `/releases/latest` link is used for alpha
downloads, because that route intentionally selects stable releases.

Image provenance and dimensions belong to `assets/images.json`. The desktop
overview, histogram and GPU graph use the published application screenshots.
Other views are actual development renders. Earlier interfaces, synthetic
activities and synthetic HUD samples are labeled in both languages. Crops are
details of those renders, not redesigned interfaces. The export contains the
rendered image assets. New screenshots must be reviewed visually and
have metadata stripped before replacing an asset.

The existing Pages workflow can later publish `build/site`, including the
unchanged reader. A local commit does not trigger this deployment; push only
when explicitly requested. Browser verification screenshots belong under the
ignored `build/site-qa`, outside the output.
