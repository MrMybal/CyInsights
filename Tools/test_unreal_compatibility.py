"""Compile only our plugin modules in a generated isolated Unreal test host.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
Engine installations are read-only inputs; no engine module is rebuilt.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True, help='Licensed Engine directory')
    parser.add_argument('--platform', choices=['Win64', 'Android'], default='Win64')
    parser.add_argument('--configuration', choices=['Development', 'Debug', 'DebugGame'], default='Development')
    parser.add_argument('--target', choices=['Editor', 'Game'], default='Editor')
    parser.add_argument('--without-gpu', action='store_true')
    parser.add_argument('--compiler', help='Optional installed MSVC version, e.g. 14.29.30133')
    parser.add_argument('--no-pch', action='store_true', help='Compile without shared precompiled headers')
    args = parser.parse_args()
    engine = args.engine.resolve()
    version = json.loads((engine / 'Build/Build.version').read_text())
    major, minor = version['MajorVersion'], version['MinorVersion']
    if not ((major == 4 and minor == 27) or (major == 5 and minor <= 8)):
        raise SystemExit('Supported source range: UE 4.27 and 5.0–5.8')
    if args.platform == 'Android' and args.target == 'Editor':
        raise SystemExit('Use --target Game for Android')
    name = f'{major}.{minor}-{args.platform}-{args.target}-{args.configuration}' + ('-native-only' if args.without_gpu else '')
    host = ROOT / 'build/UnrealCompatibility' / name
    host.mkdir(parents=True, exist_ok=True)
    plugins = ['CyInsights'] + ([] if args.without_gpu else ['CyGPUInspector'])
    for plugin in plugins:
        shutil.copytree(ROOT / 'Plugins' / plugin, host / 'Plugins' / plugin,
                        dirs_exist_ok=True, ignore=shutil.ignore_patterns('Binaries', 'Intermediate', '__pycache__'))
    project = {'FileVersion': 3, 'Modules': [{'Name': 'CyCompatibilityHost', 'Type': 'Runtime', 'LoadingPhase': 'Default'}],
               'Plugins': [{'Name': plugin, 'Enabled': True} for plugin in plugins]}
    (host / 'CyCompatibilityHost.uproject').write_text(json.dumps(project, indent=2), encoding='utf-8')
    source = host / 'Source/CyCompatibilityHost'
    source.mkdir(parents=True, exist_ok=True)
    (source / 'CyCompatibilityHost.Build.cs').write_text('''// Copyright (c) 2026 Cyberalien. MIT
using UnrealBuildTool;
public class CyCompatibilityHost : ModuleRules {
    public CyCompatibilityHost(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
    }
}
''', encoding='utf-8')
    (source / 'CyCompatibilityHost.cpp').write_text('''// Copyright (c) 2026 Cyberalien. MIT
#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, CyCompatibilityHost, "CyCompatibilityHost");
''', encoding='utf-8')
    for suffix, target in [('', 'Game'), ('Editor', 'Editor')]:
        target_name = 'CyCompatibilityHost' + suffix
        (host / 'Source' / (target_name + '.Target.cs')).write_text(f'''// Copyright (c) 2026 Cyberalien. MIT
using UnrealBuildTool;
public class {target_name}Target : TargetRules {{
    public {target_name}Target(TargetInfo Target) : base(Target) {{
        Type = TargetType.{target};
        DefaultBuildSettings = BuildSettingsVersion.{('V2' if major == 4 else 'Latest')};
        ExtraModuleNames.Add("CyCompatibilityHost");
    }}
}}
''', encoding='utf-8')
    config = host / 'Config'
    config.mkdir(exist_ok=True)
    (config / 'DefaultEngine.ini').write_text('''[/Script/CyGPUInspector.CyGPUInspectorSettings]
bLoadInEditor=False
bLoadInGame=False
bLaunchStandalone=False
''', encoding='utf-8')
    log = host / 'build.log'
    modules = ['CyInsightsTrace', 'CyInsights']
    if args.platform == 'Win64' and not args.without_gpu:
        modules += ['CyGPUInspectorLoader', 'CyGPUInspector']
    if args.target == 'Editor':
        modules += ['CyInsightsEditor'] + ([] if args.without_gpu else ['CyGPUInspectorEditor'])
    if args.platform == 'Android':
        modules += ['CyInsightsQuest']
    arguments = ['CyCompatibilityHost' + ('Editor' if args.target == 'Editor' else ''), args.platform,
                 args.configuration, '-Project=' + str(host / 'CyCompatibilityHost.uproject')]
    arguments += ['-Module=' + module for module in modules]
    arguments += ['-NoUnity', '-NoXGE', '-NoUBA', '-NoUBALocal', '-MaxParallelActions=4', '-Log=' + str(log)]
    if args.compiler: arguments += ['-CompilerVersion=' + args.compiler]
    if args.no_pch: arguments += ['-NoPCH']
    if major == 4:
        command = [str(engine / 'Binaries/DotNET/UnrealBuildTool.exe')] + arguments
    else:
        ubt = engine / 'Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'
        dotnet = list((engine / 'Binaries/ThirdParty/DotNet').glob('*/win-x64/dotnet.exe'))
        if not dotnet: dotnet = list((engine / 'Binaries/ThirdParty/DotNet').glob('*/windows/dotnet.exe'))
        if not dotnet: dotnet = list((engine / 'Binaries/ThirdParty/DotNet').glob('Windows/dotnet.exe'))
        if not dotnet: raise SystemExit('Cannot locate this engine\'s bundled dotnet')
        command = [str(dotnet[0]), str(ubt)] + arguments
    metadata = {'engine': f'{major}.{minor}.{version["PatchVersion"]}', 'platform': args.platform,
                'target': args.target, 'configuration': args.configuration, 'modules': modules,
                'engine_rebuild_requested': False, 'host': str(host)}
    (host / 'validation.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    print(json.dumps(metadata), flush=True)
    with (host / 'stdout.log').open('w', encoding='utf-8') as output:
        result = subprocess.run(command, cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
    metadata['build_exit_code'] = result.returncode
    (host / 'validation.json').write_text(json.dumps(metadata, indent=2), encoding='utf-8')
    print(f'Build exit {result.returncode}; log: {host / "stdout.log"}')
    raise SystemExit(result.returncode)

if __name__ == '__main__':
    main()
