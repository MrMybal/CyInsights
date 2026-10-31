"""Create distributable application and plugin archives from tracked sources.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from check_publication import content_issues

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-c', 'safe.directory='+ROOT.as_posix(), *args], cwd=ROOT)


def plugin_sources():
    """Only the integrated plugin, including new modules in a local source build."""
    folder=ROOT/'Plugins/CyInsights'
    excluded={'Binaries','Intermediate','Saved','__pycache__'}
    entries={}
    for path in folder.rglob('*'):
        if not path.is_file() or excluded.intersection(path.relative_to(folder).parts):
            continue
        if not path.resolve().is_relative_to(folder.resolve()):
            raise SystemExit('Plugin source escapes its directory')
        entries[path.relative_to(ROOT).as_posix()]=path.read_bytes()
    return entries


def windows_signatures(binaries, require_signed=False):
    """Read Authenticode afresh; never trust a stale hand-written manifest."""
    # Permit this local, reviewed script in this child process only. Keep
    # downloaded scripts subject to signature checks; no machine policy changes.
    host=shutil.which('pwsh.exe') or shutil.which('powershell.exe')
    if not host:
        raise SystemExit('PowerShell is required to verify Windows release signatures')
    command = [host, '-NoProfile', '-ExecutionPolicy', 'RemoteSigned', '-File', str(ROOT/'Tools/SignWindowsRelease.ps1'),
               '-Directory', str(binaries), '-VerifyOnly']
    if require_signed:
        command.append('-RequireSigned')
    environment=os.environ.copy()
    # Prefer the modern host when present. If only Windows PowerShell is
    # available, let it initialize its own built-in module search path.
    if Path(host).name.lower()=='powershell.exe':environment.pop('PSModulePath',None)
    subprocess.run(command, check=True, capture_output=True, env=environment)
    report = json.loads((binaries/'windows-signatures.json').read_text(encoding='utf-8-sig'))
    for record in report['files']:
        path = (binaries/record['path']).resolve()
        if not path.is_relative_to(binaries) or hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
            raise SystemExit('Binary changed after signature verification')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binaries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    parser.add_argument('--require-signed', action='store_true', help='Reject unsigned, untrusted or untimestamped product binaries')
    args = parser.parse_args()
    if not args.version or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in args.version):
        raise SystemExit('Invalid version label')
    product_version=json.loads((ROOT/'VERSION.json').read_text(encoding='utf-8'))['version']
    if re.fullmatch(r'v?\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?',args.version) and args.version.removeprefix('v') != product_version:
        raise SystemExit('Release tag does not match VERSION.json; rebuild the matching version before packaging')
    binaries = args.binaries.resolve()
    output = args.output.resolve()
    if output != ROOT/'build' and ROOT/'build' not in output.parents:
        raise SystemExit('Release output must stay within the workspace build directory')
    output.mkdir(parents=True, exist_ok=True)
    tracked = [p for p in git('ls-files', '-z').decode().split('\0') if p]
    app = {}
    signatures = windows_signatures(binaries, args.require_signed)
    for name in ('CyInsightsApp.exe', 'CyCaptureTool.exe', 'CyInsightsUpdater.exe'):
        data = (binaries/name).read_bytes()
        matching = [r for r in signatures['files'] if r['path'] == name]
        if len(matching) != 1 or hashlib.sha256(data).hexdigest() != matching[0]['sha256']:
            raise SystemExit('Executable changed after signature verification: '+name)
        if name=='CyInsightsApp.exe' and matching[0].get('product_version')!=product_version:
            raise SystemExit('Windows application version does not match VERSION.json; rebuild before packaging')
        if content_issues(name, data):
            raise SystemExit('Executable failed publication inspection: '+name)
        app[name] = data
    app['windows-signatures.json'] = json.dumps(signatures, indent=2).encode()
    app['Docs/Updates.md'] = (ROOT/'Docs/Updates.md').read_bytes()
    for name in tracked:
        # Preserve licensed dependency notices and the offline AI installer;
        # do not copy an existing virtualenv or its machine-specific config.
        if name.startswith(('LICENCES/', 'Docs/', 'AI/')) or name in ('LICENSE', 'THIRD-PARTY-CyGPUInspector.md'):
            app[name] = (ROOT/name).read_bytes()
        if name.startswith(('Tools/NativeTiming/', 'Tools/NativeTasks/', 'Tools/NativeActivities/')) or name == 'Tools/InstallNativeReaders.ps1':
            app[name] = (ROOT/name).read_bytes()
    app['README.txt'] = (
        'CyInsights '+args.version+' / Windows x64\n'
        'Extract the complete archive and run CyInsightsApp.exe.\n'
        'Keep CyInsightsUpdater.exe alongside the application. Help > Updates checks stable GitHub releases.\n'
        'Register your Unreal engine root folder in Session; no automatic installation scan is performed.\n'
        'On first launch, native readers are built automatically in the background, with\n'
        'progress, log and cancellation. No script needs to be run manually. Results are\n'
        'cached in local application data for subsequent launches.\n'
        'This preparation requires licensed UE 5.5-5.8 and VS C++/CMake tools.\n'
        'UE 5.8 needs MSVC 14.44. Unreal is not rebuilt. The CSV bridge remains available\n'
        'if preparation is cancelled or dependencies are missing. Open a .utrace or .cycapture.\n'
        'The native AI Inspector, tools and analysis method are built into the app.\n'
        'Only the optional external AI/SDK companion needs Python 3.11 x64 and\n'
        'powershell -File AI/SetupRuntime.ps1.\n'
        'Native Unreal adapters are supplied as sources, not prebuilt Engine Tools.\n'
        'See Docs/Licensing.md, Docs/NativeTiming.md and the matching release sources.\n'
    ).encode()
    app['release-manifest.json'] = json.dumps({
        'version': args.version, 'source_commit': git('rev-parse', 'HEAD').decode().strip(),
        'signed': signatures['signed'], 'signature_report': 'windows-signatures.json',
        'antivirus_verdict': 'not_established_by_signature_or_build_provenance',
        'native_readers': 'automatic local preparation at launch with licensed Unreal and VS C++ tools',
        'files': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(app.items())},
    }, indent=2).encode()
    plugins = plugin_sources()
    for name in ('Docs/UnrealCompatibility.md', 'Docs/Licensing.md', 'LICENSE'):
        plugins[name] = (ROOT/name).read_bytes()
    plugins['README.txt'] = b'Unreal plugin sources. Copy Plugins/CyInsights into your project Plugins directory and build with your licensed engine. See Docs/UnrealCompatibility.md and Docs/Licensing.md. No compiled Unreal binaries are included.\n'
    archives = []
    for label, entries in (('Windows-x64', app), ('Unreal-Plugins-Source', plugins)):
        archive = output/('CyInsights-'+args.version+'-'+label+'.zip')
        if archive.exists():
            raise SystemExit('Refusing to replace an existing release archive')
        with ZipFile(archive, 'w', ZIP_DEFLATED, compresslevel=6) as z:
            for name, data in sorted(entries.items()):
                if not name.endswith('.whl') and content_issues(name, data):
                    raise SystemExit('Package entry failed publication inspection: '+name)
                z.writestr(name, data)
        with ZipFile(archive) as z:
            if z.testzip() is not None or set(z.namelist()) != set(entries):
                raise SystemExit('Release archive verification failed')
        archives.append(archive)
    checksums = output/'SHA256SUMS.txt'
    checksums.write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in archives), encoding='utf-8')
    print(json.dumps({'archives': [dict(name=p.name, bytes=p.stat().st_size) for p in archives]}))


if __name__ == '__main__':
    main()
