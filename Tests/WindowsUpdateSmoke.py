"""Exercise the real native updater on an isolated test host, never the user's app.
Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import zipfile

ROOT=Path(__file__).resolve().parents[1]
def digest(data):return hashlib.sha256(data).hexdigest()
def wait(predicate,seconds=15):
    until=time.monotonic()+seconds
    while time.monotonic()<until:
        if predicate():return
        time.sleep(.05)
    raise AssertionError('Native updater fixture timed out')

def main():
    p=argparse.ArgumentParser();p.add_argument('--binaries',type=Path,required=True);p.add_argument('--host',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    output=a.output.resolve();assert output.is_relative_to(ROOT/'build') and not output.exists();output.mkdir(parents=True)
    target=output/"application's test copy";target.mkdir();work=output/'transaction';(work/'helper').mkdir(parents=True);prepared=work/'prepared';prepared.mkdir()
    original=a.host.read_bytes()+b'\nold fixture build\n';(target/'CyInsightsApp.exe').write_bytes(original);(target/'CyCaptureTool.exe').write_bytes(b'old fixture CLI');(target/'settings.json').write_text('{"preserve":"settings"}',encoding='utf-8')
    helper=a.binaries/'CyInsightsUpdater.exe';shutil.copyfile(helper,target/helper.name);shutil.copyfile(helper,work/'helper'/helper.name)
    files={'CyInsightsApp.exe':a.host.read_bytes(),'CyInsightsUpdater.exe':helper.read_bytes(),'CyCaptureTool.exe':(a.binaries/'CyCaptureTool.exe').read_bytes(),'Docs/Updated.txt':b'updated documentation'}
    manifest={'version':'v9.0.0','files':{n:digest(b) for n,b in files.items()}};files['release-manifest.json']=json.dumps(manifest).encode()
    archive=work/'update.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for n,b in files.items():z.writestr(n,b)
    for n,b in files.items():dest=prepared/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(b)
    parent=process=None
    try:
        parent=subprocess.Popen([str(target/'CyInsightsApp.exe'),'--wait'],creationflags=subprocess.CREATE_NO_WINDOW)
        wait(lambda:(target/'parent-ready.txt').exists())
        created=wintypes.FILETIME();ended=wintypes.FILETIME();kernel=wintypes.FILETIME();user=wintypes.FILETIME()
        get_times=ctypes.windll.kernel32.GetProcessTimes;get_times.argtypes=[wintypes.HANDLE]+[ctypes.POINTER(wintypes.FILETIME)]*4;get_times.restype=wintypes.BOOL
        assert get_times(wintypes.HANDLE(int(parent._handle)),ctypes.byref(created),ctypes.byref(ended),ctypes.byref(kernel),ctypes.byref(user))
        job={'version':'v9.0.0','target':str(target),'sha256':digest(archive.read_bytes()),'size':archive.stat().st_size,'pid':parent.pid,'process_created':str((created.dwHighDateTime<<32)|created.dwLowDateTime),'reopen':[]}
        jobfile=work/'install.json';jobfile.write_text(json.dumps(job),encoding='utf-8')
        process=subprocess.Popen([str(work/'helper'/helper.name),'--apply',str(jobfile)],cwd=work,creationflags=subprocess.CREATE_NO_WINDOW)
        time.sleep(.3);assert parent.poll() is None and (target/'CyInsightsApp.exe').read_bytes()==original,'Helper changed files while the application was running'
        (target/'finish-request.txt').write_text('exit');assert parent.wait(timeout=10)==0
        assert process.wait(timeout=30)==0
        wait(lambda:(target/'restart-proof.txt').exists())
        assert json.loads((work/'result.json').read_text())['status']=='installed'
        assert (target/'CyInsightsApp.exe').read_bytes()==files['CyInsightsApp.exe']
        assert (work/'backup/CyInsightsApp.exe').read_bytes()==original
        assert json.loads((target/'settings.json').read_text())=={'preserve':'settings'}
        assert (target/'Docs/Updated.txt').read_bytes()==files['Docs/Updated.txt']
        print(json.dumps({'real_helper':'passed','waited_for_application_exit':True,'restarted_test_host':True,'backup_verified':True,'settings_preserved':True,'actual_application_modified':False}),flush=True)
    finally:
        for child in (parent,process):
            if child is not None and child.poll() is None:child.terminate();child.wait(timeout=10)

if __name__=='__main__':main()
