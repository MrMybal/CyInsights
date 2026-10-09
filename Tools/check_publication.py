"""Audit staged/tracked publication files without emitting secret values.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_ROOTS = {'.Dependencies', 'build', 'bin', 'AI/runtime', 'Binaries',
                   'Intermediate', 'Saved', 'DerivedDataCache', 'Source', 'Content', 'Config', '.codex', '.agents'}
FORBIDDEN_SUFFIXES = {'.utrace', '.ucache', '.cycapture', '.cyindex', '.log', '.exe',
                      '.dll', '.lib', '.pdb', '.obj', '.addon64', '.uba', '.uproject', '.sln'}
SECRETS = re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|'
                     rb'AKIA[0-9A-Z]{16}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true')
    args = parser.parse_args()
    command = ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'] if args.staged else ['git', 'ls-files', '-z']
    files = subprocess.check_output(command, cwd=ROOT).decode('utf-8').split('\0')
    files = [name for name in files if name]
    if not files:
        raise SystemExit('No publication files to audit')
    errors = []
    total = 0
    for name in files:
        path = Path(name)
        if any(name == item or name.startswith(item + '/') for item in FORBIDDEN_ROOTS):
            errors.append('excluded directory: ' + name)
        if path.suffix.lower() in FORBIDDEN_SUFFIXES or 'Binaries' in path.parts or 'Intermediate' in path.parts or '__pycache__' in path.parts:
            errors.append('excluded artifact: ' + name)
        if path.name.startswith('.env') and path.name != '.env.example':
            errors.append('environment file: ' + name)
        data = (ROOT / name).read_bytes()
        total += len(data)
        if len(data) > 95 * 1024 * 1024:
            errors.append('oversized Git file: ' + name)
        if path.suffix != '.whl' and SECRETS.search(data):
            errors.append('possible secret: ' + name)
    lock = json.loads((ROOT / 'DEPENDENCIES.lock.json').read_text(encoding='utf-8'))
    for name, expected in lock['sha256'].items():
        path = ROOT / lock['source'] / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            errors.append('dependency hash mismatch: ' + name)
        if (Path(lock['source']) / name).as_posix() not in files:
            errors.append('dependency source omitted: ' + name)
    ai_lock = json.loads((ROOT / 'AI/dependency-lock.json').read_text(encoding='utf-8'))
    for wheel in ai_lock['wheels']:
        name = 'AI/wheels/' + wheel['file']
        if name not in files or hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != wheel['sha256']:
            errors.append('AI wheel missing or changed: ' + name)
    for scope in ('Plugins/CyInsights/Source', 'Plugins/CyGPUInspector/Source',
                  'Tools/NativeTasks', 'ThirdParty/CyGPUInspector/CyGPUInspectorCore',
                  'ThirdParty/CyGPUInspector/CyGPUInspectorRS'):
        for path in (ROOT / scope).rglob('*'):
            if path.is_file() and path.suffix in {'.cpp', '.h', '.hpp', '.cs', '.rc', '.inc'}:
                if b'AGPL' in path.read_bytes():
                    errors.append('copyleft in MIT integration: ' + path.relative_to(ROOT).as_posix())
    if errors:
        raise SystemExit('\n'.join(errors))
    print(json.dumps({'files': len(files), 'bytes': total, 'source_dependencies': len(lock['sha256']),
                      'offline_wheels': len(ai_lock['wheels']), 'publication_audit': 'passed'}))

if __name__ == '__main__':
    main()
