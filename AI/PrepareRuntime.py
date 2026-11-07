"""Install/verify the shipped MCP wheels in an application-owned Python prefix.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
This script never downloads packages or modifies a global Python installation.
"""
import argparse
import hashlib
from importlib import metadata
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


def canonical(name):
    return re.sub(r"[-_.]+", "-", name).lower()


def verified_requirements(source):
    lock = json.loads((source / "dependency-lock.json").read_text(encoding="utf-8"))
    hashes = {}
    for item in lock["wheels"]:
        name = item["file"]
        if Path(name).name != name or not name.endswith(".whl"):
            raise RuntimeError("Invalid wheel path in dependency lock")
        path = source / "wheels" / name
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != item["sha256"]:
            raise RuntimeError("Wheel integrity check failed: " + name)
        package, version = name.split("-", 2)[:2]
        hashes[(canonical(package), version)] = digest
    requirements = []
    for line in (source / "requirements.lock.txt").read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        match = re.fullmatch(r"([A-Za-z0-9_.-]+)==([A-Za-z0-9_.+!-]+)", line.strip())
        if not match:
            raise RuntimeError("Runtime requirements must use exact pinned versions")
        name, version = match.groups()
        digest = hashes[(canonical(name), version)]
        requirements.append((name, version, digest))
    if len(requirements) != len(hashes) or len({canonical(r[0]) for r in requirements}) != len(requirements):
        raise RuntimeError("Runtime requirements and wheel inventory differ")
    return requirements


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("install", "verify"))
    parser.add_argument("--source", type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()
    if sys.version_info[:2] != (3, 11) or struct.calcsize("P") != 8:
        raise RuntimeError("This adapter requires private Python 3.11 x64")
    requirements = verified_requirements(source)
    if args.action == "install":
        verified = Path(sys.prefix) / "cyinsights-requirements.txt"
        verified.write_text("".join(f"{name}=={version} --hash=sha256:{digest}\n"
                                    for name, version, digest in requirements), encoding="utf-8")
        subprocess.run([sys.executable, "-I", "-m", "pip", "--isolated",
                        "--disable-pip-version-check", "install", "--no-input", "--no-index",
                        "--no-cache-dir", "--no-warn-script-location", "--only-binary=:all:", "--require-hashes",
                        "--find-links", str(source / "wheels"), "-r", str(verified)], check=True)
        # A fresh interpreter must process newly installed .pth files (pywin32).
        # The native installer performs that verification as a separate phase.
        return
    for name, version, _ in requirements:
        if metadata.version(name) != version:
            raise RuntimeError("Installed dependency version differs: " + name)
    # Import the real native extensions as well as the official SDK transports.
    import cytools_core
    import mcp
    import pydantic_core
    import psutil
    import win32api
    from cytools_core import load_manifest
    from mcp.server.stdio import stdio_server
    manifest = load_manifest(source / "app" / "CyTool.json")
    with tempfile.TemporaryDirectory(prefix="sdk-check-", dir=Path(sys.prefix).parent) as probe:
        described = subprocess.run([sys.executable, "-E", "-s", str(source / "app" / "tool.py"),
                                    "--data-dir", probe, "describe"],
                                   capture_output=True, text=True, timeout=30)
        if described.returncode:
            print(described.stderr, file=sys.stderr, flush=True)
            raise RuntimeError("The installed SDK cannot describe this application's operations")
    descriptor = json.loads(described.stdout)
    if descriptor.get("id") != manifest["id"] or len(descriptor.get("operations", [])) != len(manifest["operations"]):
        raise RuntimeError("The runtime does not expose the complete tool contract")
    print(json.dumps({"ready": True, "python": sys.version.split()[0],
                      "cytools_core": metadata.version("cytools-core"), "mcp": metadata.version("mcp"),
                      "operations": len(manifest["operations"])}), flush=True)


if __name__ == "__main__":
    main()
