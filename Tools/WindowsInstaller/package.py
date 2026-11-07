"""Build an offline per-user installer from the verified application payload.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
from pathlib import Path
import re
import subprocess

ROOT=Path(__file__).resolve().parents[2]

def nsis_string(value):
    return str(value).replace('$','$$').replace('"','$\\"')

def build_installer(entries,output,tag,product_version,compiler):
    output=output.resolve();compiler=compiler.resolve()
    target=output/f'CyInsights-{tag}-Windows-x64-Setup.exe'
    payload=output/'.installer-build'
    if target.exists() or payload.exists():raise SystemExit('Installer output already exists; use a fresh package directory')
    if 'cyinsights-portable.txt' in entries or any(p.startswith('Data/') for p in entries):
        raise SystemExit('Installer input must not contain portable settings')
    version=subprocess.run([str(compiler),'/VERSION'],capture_output=True,text=True,check=True).stdout.strip()
    if version!='v3.12':raise SystemExit('Use the pinned NSIS 3.12 compiler from Tools/prepare_installer.py')
    payload.mkdir(parents=True)
    files=[];directories=set();previous=None
    for name,data in sorted(entries.items()):
        if not re.fullmatch(r'[A-Za-z0-9_. /-]+',name) or '..' in Path(name).parts or name.startswith('/'):
            raise SystemExit('Invalid installer payload path')
        destination=payload/'app'/name
        destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(data)
        parent=Path(name).parent
        if parent!=previous:
            suffix='' if parent==Path('.') else '\\'+str(parent).replace('/','\\')
            files.append(f'SetOutPath "$INSTDIR{nsis_string(suffix)}"')
            previous=parent
        files.append(f'File "{nsis_string(destination)}"')
        while parent!=Path('.'):
            directories.add(parent);parent=parent.parent
    uninstall=[f'Delete "$INSTDIR\\{nsis_string(name.replace(chr(47),chr(92)))}"' for name in sorted(entries)]
    uninstall.extend(f'RMDir "$INSTDIR\\{nsis_string(str(p).replace(chr(47),chr(92)))}"' for p in sorted(directories,key=lambda p:(len(p.parts),str(p)),reverse=True))
    (payload/'files.nsh').write_text('\n'.join(files)+'\n',encoding='utf-8')
    (payload/'uninstall.nsh').write_text('\n'.join(uninstall)+'\n',encoding='utf-8')
    numeric=product_version.split('-',1)[0]
    definitions={'PAYLOAD':payload,'OUTPUT_FILE':target,'PRODUCT_VERSION':product_version,
                 'NUMERIC_VERSION':numeric,'PRODUCT_ICON':ROOT/'Assets/Branding/cyinsights-logo-v1.ico',
                 'PRODUCT_LICENSE':ROOT/'LICENSE'}
    command=[str(compiler),'/V2','/INPUTCHARSET','UTF8']+[f'/D{k}={nsis_string(v)}' for k,v in definitions.items()]+[str(ROOT/'Tools/WindowsInstaller/CyInsights.nsi')]
    result=subprocess.run(command,capture_output=True,text=True,encoding='utf-8',errors='replace')
    (payload/'compiler.log').write_text(result.stdout+result.stderr,encoding='utf-8')
    if result.returncode or not target.is_file():raise SystemExit('Installer compilation failed:\n'+(result.stdout+result.stderr)[-5000:])
    return target
