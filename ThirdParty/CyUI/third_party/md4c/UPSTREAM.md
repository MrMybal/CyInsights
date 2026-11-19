# MD4C

Upstream: https://github.com/mity/md4c
Commit: f9e5eb232b95b737373ae706648cb75bd259ed1a
License: MIT (see LICENSE.md).

md4c.cpp is upstream src/md4c.c with explicit allocation/opaque-pointer casts for C++ compilation, named for C++ builds.
CyUI extends `MD_ATTRIBUTE` with the raw input span before escape processing. The Markdown exporter uses those spans to relocate link and image destinations, including reference definitions, while preserving the surrounding source.
Only the parser is bundled. No HTML renderer, browser or network dependency.

Trailing whitespace and final blank lines are normalized for the repository.
