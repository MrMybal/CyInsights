"""Verify the native AI workflow in a fresh directory without AI/Python/MCP files.

Uses only Python stdlib as a developer launcher. The tested application performs
its own local tool calls in C++; no SDK client or bridge is started by this test.
Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess


def digest(path):
    value=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):value.update(block)
    return value.hexdigest()


def main():
    parser=argparse.ArgumentParser()
    for name in ('app','trace','engine-profile','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--french',action='store_true')
    args=parser.parse_args();root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    appdir=root/'app';appdir.mkdir();profile=root/'profile';(profile/'CyInsights').mkdir(parents=True)
    source=args.app.resolve();exe=appdir/'CyInsightsApp.exe';shutil.copy2(source,exe)
    for folder in ('NativeTiming','NativeTasks','NativeActivities'):
        shutil.copytree(source.parent/folder,appdir/folder)
    shutil.copy2(args.engine_profile.resolve()/'CyInsights/engines.json',profile/'CyInsights/engines.json')
    assert not (appdir/'AI').exists() and not (root.parent/'AI/app/CyTool.json').exists()
    if args.french:(appdir/'CyInsights-AI-options.json').write_text(json.dumps({'french':True,'autoAnalysis':False,'targetFps':60}),encoding='utf-8')
    before=digest(args.trace);snapshot=root/'standalone.png'
    result=subprocess.run([str(exe),'--standalone-ai-smoke','--snapshot',str(snapshot),str(args.trace.resolve())],cwd=root,env=dict(os.environ,LOCALAPPDATA=str(profile)),timeout=180)
    assert result.returncode==0,('Native smoke failed',result.returncode)
    proof=json.loads(Path(str(snapshot)+'.standalone-ai.json').read_text(encoding='utf-8'))
    assert proof['passed'] and proof['embeddedOperations']==39 and not proof['bridgeRunning']
    assert not proof['externalCompanionPresent'] and len(proof['localOperations'])==7
    assert digest(args.trace)==before
    proof['sourceTraceUnchanged']=True;proof['pythonCompanionBundled']=False;proof['language']='fr' if args.french else 'en'
    (root/'proof.json').write_text(json.dumps(proof,indent=2),encoding='utf-8')
    print('Standalone native tools: 39 embedded operations, 7 real local calls, no Python companion or MCP bridge',flush=True)


if __name__=='__main__':main()
