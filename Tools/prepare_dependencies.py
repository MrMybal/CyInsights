"""Verify the versioned source snapshot; never overwrite licenses or plugins.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, help='Optional snapshot directory to verify')
    args = parser.parse_args()
    lock = json.loads((ROOT / 'DEPENDENCIES.lock.json').read_text(encoding='utf-8'))
    source = args.source.resolve() if args.source else ROOT / lock['source']
    errors = []
    for name, expected in lock['sha256'].items():
        file = source / name
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            errors.append(name)
    cyui_root = ROOT / 'ThirdParty/CyUI'
    cyui = json.loads((cyui_root / 'source-sha256.json').read_text(encoding='utf-8'))
    for name, expected in cyui.items():
        file = cyui_root / name
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest() != expected:
            errors.append('CyUI/' + name)
    cydoc_root=ROOT/'ThirdParty/CyDoc'
    cydoc=json.loads((cydoc_root/'snapshot.json').read_text(encoding='utf-8'))
    for name,expected in cydoc['sha256'].items():
        file=cydoc_root/name
        if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest()!=expected:
            errors.append('CyDoc/'+name)
    if errors:
        raise SystemExit('Dependency snapshot differs from its hash manifest:\n' + '\n'.join(errors))
    print(f"Verified {len(lock['sha256'])} core dependency files and {len(cyui)} pinned CyUI files. No external checkout or binary copy required.")

if __name__ == '__main__':
    main()
