"""Stage the CyTools companion into a selected versioned desktop release."""
import hashlib, json, shutil, sys, zipfile
from pathlib import Path

root=Path(__file__).resolve().parents[1]
release=Path(sys.argv[1]).resolve()
if not release.is_relative_to(root/'bin'):raise ValueError('Choose a versioned directory inside this project/bin')
if not (release/'CyInsightsApp.exe').is_file():raise ValueError('Build the native application first')
for sub in ('app','wheels'):
    folder=release/'AI'/sub;folder.mkdir(parents=True,exist_ok=True)
    for source in (root/'AI'/sub).iterdir():
        if source.is_file():shutil.copy2(source,folder/source.name)
for name in ('SetupRuntime.ps1','requirements.lock.txt','dependency-lock.json','README.md'):
    shutil.copy2(root/'AI'/name,release/'AI'/name)
shutil.copytree(root/'LICENCES',release/'LICENCES',dirs_exist_ok=True)
for name in ('LICENSE','THIRD-PARTY-CyGPUInspector.md','DEPENDENCIES.lock.json'):
    shutil.copy2(root/name,release/name)
(release/'Docs').mkdir(exist_ok=True)
for name in ('AiIntegration.md','CapabilityCoverage.md','Licensing.md'):
    shutil.copy2(root/'Docs'/name,release/'Docs'/name)
lock=json.loads((root/'AI/dependency-lock.json').read_text(encoding='utf-8'))
for wheel in lock['wheels']:
    assert hashlib.sha256((release/'AI/wheels'/wheel['file']).read_bytes()).hexdigest()==wheel['sha256']
print(json.dumps({'staged':str(release),'cytools_core':lock['cytools_core'],'mcp':lock['mcp'],'wheels':len(lock['wheels'])}))
