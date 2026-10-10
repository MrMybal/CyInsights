"""Create distributable application and plugin archives from tracked sources.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
from check_publication import content_issues

ROOT = Path(__file__).resolve().parents[1]


def git(*args):
    return subprocess.check_output(['git', '-c', 'safe.directory='+ROOT.as_posix(), *args], cwd=ROOT)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binaries', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    if not args.version or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-' for c in args.version):
        raise SystemExit('Invalid version label')
    binaries = args.binaries.resolve()
    output = args.output.resolve()
    if output != ROOT/'build' and ROOT/'build' not in output.parents:
        raise SystemExit('Release output must stay within the workspace build directory')
    output.mkdir(parents=True, exist_ok=True)
    tracked = [p for p in git('ls-files', '-z').decode().split('\0') if p]
    app = {}
    for name in ('CyInsightsApp.exe', 'CyCaptureTool.exe'):
        data = (binaries/name).read_bytes()
        if content_issues(name, data):
            raise SystemExit('Executable failed publication inspection: '+name)
        app[name] = data
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
        'Installed Unreal analyzers are detected on launch; select yours in Session if needed.\n'
        'On first launch, native readers are built automatically in the background, with\n'
        'progress, log and cancellation. No script needs to be run manually. Results are\n'
        'cached in local application data for subsequent launches.\n'
        'This preparation requires licensed UE 5.5-5.8 and VS C++/CMake tools.\n'
        'UE 5.8 needs MSVC 14.44. Unreal is not rebuilt. The CSV bridge remains available\n'
        'if preparation is cancelled or dependencies are missing. Open a .utrace or .cycapture.\n'
        'Optional AI companion: Python 3.11 x64, then powershell -File AI/SetupRuntime.ps1.\n'
        'Native Unreal adapters are supplied as sources, not prebuilt Engine Tools.\n'
        'See Docs/Licensing.md, Docs/NativeTiming.md and the matching release sources.\n'
    ).encode()
    app['release-manifest.json'] = json.dumps({
        'version': args.version, 'source_commit': git('rev-parse', 'HEAD').decode().strip(),
        'signed': False, 'native_readers': 'automatic local preparation at launch with licensed Unreal and VS C++ tools',
        'files': {name: hashlib.sha256(data).hexdigest() for name, data in sorted(app.items())},
    }, indent=2).encode()
    plugins = {name: (ROOT/name).read_bytes() for name in tracked if name.startswith('Plugins/')}
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
