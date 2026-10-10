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
                   'Intermediate', 'Saved', 'DerivedDataCache', 'Source', 'Content', 'Config',
                   '.codex', '.agents', '.aws', '.ssh', '.idea'}
FORBIDDEN_SUFFIXES = {'.utrace', '.ucache', '.cycapture', '.cyindex', '.log', '.exe',
                      '.dll', '.lib', '.pdb', '.obj', '.addon64', '.uba', '.uproject', '.sln',
                      '.pfx', '.p12', '.key', '.dmp', '.db', '.sqlite', '.sqlite3', '.bak'}
SECRETS = re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{40,}|'
                     rb'(?:AKIA|ASIA)[0-9A-Z]{16}|AIza[A-Za-z0-9_-]{35}|'
                     rb'xox[baprs]-[A-Za-z0-9-]{20,}|sk-(?:proj-)?[A-Za-z0-9_-]{32,}|'
                     rb'-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----)')
LOCAL_PATHS = re.compile(rb'(?i)(?:[a-z]:[\\/](?:Users|Documents and Settings)[\\/][^\\/\s"<>]+|'
                         rb'/(?:home|Users)/[^/\s"<>]+/|'
                         rb'[a-z]:[\\/](?:_Plugins|_Projects?|UnrealEngine)(?:[\\/]|["\s]))')


def content_issues(name, data):
    # Locked wheels are supplied by third parties. Their extracted contents
    # need a separate scan; this gate does not pretend to inspect compression.
    if Path(name).suffix.lower() == '.whl':
        return []
    issues = []
    if SECRETS.search(data):
        issues.append('possible secret: ' + name)
    if LOCAL_PATHS.search(data):
        issues.append('machine-specific path: ' + name)
    return issues


def history_issues():
    issues = []
    commits = subprocess.check_output(['git', 'rev-list', '--all'], cwd=ROOT).decode().splitlines()
    checked = set()
    for commit in commits:
        identities = subprocess.check_output(['git', 'show', '-s', '--format=%ae%x00%ce', commit], cwd=ROOT).decode().strip().split('\0')
        if any(not email.endswith('@users.noreply.github.com') for email in identities):
            issues.append('non-noreply commit identity: ' + commit[:12])
        message = subprocess.check_output(['git', 'show', '-s', '--format=%B', commit], cwd=ROOT)
        issues.extend(content_issues('commit message ' + commit[:12], message))
        entries = subprocess.check_output(['git', 'ls-tree', '-r', '-z', commit], cwd=ROOT).split(b'\0')
        for entry in entries:
            if not entry:
                continue
            metadata, path = entry.split(b'\t', 1)
            _, kind, oid = metadata.split()
            if kind != b'blob' or oid in checked:
                continue
            checked.add(oid)
            name = path.decode('utf-8')
            data = subprocess.check_output(['git', 'cat-file', 'blob', oid.decode()], cwd=ROOT)
            issues.extend(content_issues(name + ' @ ' + commit[:12], data) if not name.endswith('.whl') else [])
    return issues, len(commits), len(checked)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true')
    parser.add_argument('--history', action='store_true', help='Check all available refs and commit identities too')
    args = parser.parse_args()
    command = ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'] if args.staged else ['git', 'ls-files', '-z']
    files = subprocess.check_output(command, cwd=ROOT).decode('utf-8').split('\0')
    files = [name for name in files if name]
    tracked = set(subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0'))
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
        data = subprocess.check_output(['git', 'show', ':' + name], cwd=ROOT) if args.staged else (ROOT / name).read_bytes()
        total += len(data)
        if len(data) > 95 * 1024 * 1024:
            errors.append('oversized Git file: ' + name)
        errors.extend(content_issues(name, data))
    lock = json.loads((ROOT / 'DEPENDENCIES.lock.json').read_text(encoding='utf-8'))
    for name, expected in lock['sha256'].items():
        path = ROOT / lock['source'] / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            errors.append('dependency hash mismatch: ' + name)
        if (Path(lock['source']) / name).as_posix() not in tracked:
            errors.append('dependency source omitted: ' + name)
    ai_lock = json.loads((ROOT / 'AI/dependency-lock.json').read_text(encoding='utf-8'))
    for wheel in ai_lock['wheels']:
        name = 'AI/wheels/' + wheel['file']
        if name not in tracked or hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != wheel['sha256']:
            errors.append('AI wheel missing or changed: ' + name)
    for scope in ('Plugins/CyInsights/Source', 'Plugins/CyGPUInspector/Source',
                  'Tools/NativeTasks', 'Tools/NativeActivities', 'ThirdParty/CyGPUInspector/CyGPUInspectorCore',
                  'ThirdParty/CyGPUInspector/CyGPUInspectorRS'):
        for path in (ROOT / scope).rglob('*'):
            if path.is_file() and path.suffix in {'.cpp', '.h', '.hpp', '.cs', '.rc', '.inc'}:
                if b'AGPL' in path.read_bytes():
                    errors.append('copyleft in MIT integration: ' + path.relative_to(ROOT).as_posix())
    commits_checked = 0
    if args.history:
        findings, commits_checked, _ = history_issues()
        errors.extend(findings)
    if errors:
        raise SystemExit('\n'.join(errors))
    print(json.dumps({'files': len(files), 'bytes': total, 'source_dependencies': len(lock['sha256']),
                      'offline_wheels': len(ai_lock['wheels']), 'commits_checked': commits_checked,
                      'publication_audit': 'passed'}))

if __name__ == '__main__':
    main()
