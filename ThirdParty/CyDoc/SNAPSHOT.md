# CyDoc package subset

Upstream: https://github.com/MrMybal/CyDoc

Version 0.1.0, revision `774f2a7eabf7dd181a98ffc5add115b580b67f42`.
`snapshot.json` pins the reviewed archive, original official package hash, native
route helper and full MIT license. Every retained package entry is upstream code.
Only `tools/create-example-docs.py`, an unused demo authoring generator containing
a development-machine path, is omitted. No compiler or reader code is patched.

Reproduce with `python Tools/sync_cydoc.py`; it uses a clean isolated Git checkout
and the official npm build/pack workflow, then selects the distributable subset
with normalized archive metadata. Normal builds use the local archive and locked
npm build dependencies. No external CyDoc checkout is required.

Application exports enable only image. They contain no Three.js, Node runtime,
npm cache, examples, generator or private Git metadata. The full MIT notice and
notices for Prism, Marked and esbuild are copied into each exported help tree.
