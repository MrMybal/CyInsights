# Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""Check the raw /showIncludes prefix against Ninja's generated rule."""
from pathlib import Path
import subprocess
import sys

build = Path(sys.argv[1]).resolve()
probe = build / "dependency-probe"
probe.mkdir(exist_ok=True)
header = probe / "dependency.hpp"
header.write_text("// dependency probe\n", encoding="utf-8")
source = probe / "probe.cpp"
source.write_text('#include "dependency.hpp"\n', encoding="utf-8")
result = subprocess.run(["cl.exe", "/nologo", "/showIncludes", "/Zs", str(source)], capture_output=True)
if result.returncode:
    raise SystemExit("MSVC dependency probe failed: " + repr(result.stdout + result.stderr))
line = next((line for line in (result.stdout + result.stderr).splitlines() if b"dependency.hpp" in line), None)
if line is None:
    raise SystemExit("No MSVC dependency record found")
prefix = line[:line.index(str(header).encode("utf-8"))].rstrip() + b" "
rules = (build / "CMakeFiles/rules.ninja").read_bytes()
generated = next(line for line in rules.splitlines() if line.startswith(b"msvc_deps_prefix = ")).split(b" = ", 1)[1]
print("Compiler prefix:", repr(prefix))
print("Ninja prefix:", repr(generated))
if generated.rstrip() != prefix.rstrip():
    raise SystemExit("MSVC/Ninja dependency prefixes differ")
