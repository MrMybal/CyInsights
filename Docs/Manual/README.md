# CyDoc user-manual sources

**[Documentation](https://mrmybal.github.io/CyInsights/#/en/welcome)** · [Documentation en français](https://mrmybal.github.io/CyInsights/#/fr/welcome)

The 24 chapters in `en/` and `fr/`, their shared explicit anchors and
`catalog.json` remain application-owned authoring inputs. Links use
`[caption](doc:page-id#section-id)`. Screenshots stay in `Docs/Images/`;
diagram definitions in the catalog are compiled to static SVG. The tile grid
is explicitly illustrative, never fabricated measured data.

The former native Markdown/search/image reader and C++ content embedding have
been replaced by CyDoc. See [integration, build and validation](../CyDoc/README.md).
The old [validation record](VALIDATION.md) describes the retired reader.

```text
python Tools/build_docs.py --output build/help
python Tests/DocumentationTests.py
```

Open `build/help/index.html` directly. No server is needed. Preserve page IDs,
explicit section anchors and media IDs across translations. Changes to English
text change its tracked revision. Translation approvals are stored in
`Docs/CyDoc/translations.json`; only advance the French revision after reviewing
its meaning against English. Builds report pending or stale translations.
