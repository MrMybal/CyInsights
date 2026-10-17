"""Rendered owned host: optional CyInsights plugin collector + local CyInsights recorder.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
import json
import os
import socket
import subprocess
import threading
import time
from pathlib import Path


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('engine','host','recorder','reshade','addon','output'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=False)
    prefix='UnrealEditor'
    build_id=json.loads((args.engine/'Binaries/Win64/UnrealEditor.modules').read_text())['BuildId']
    for folder in [args.host.parent/'Binaries/Win64',args.host.parent/'Plugins/CyInsights/Binaries/Win64']:
        modules={dll.stem[len(prefix)+1:]:dll.name for dll in folder.glob(f'{prefix}-*.dll')}
        (folder/f'{prefix}.modules').write_text(json.dumps(dict(BuildId=build_id,Modules=modules)))
    with socket.socket() as probe:probe.bind(('127.0.0.1',0));port=probe.getsockname()[1]
    recorder=None
    with (args.output/'host.log').open('w',encoding='utf8') as log:
        env=dict(os.environ);env['UE-LocalDataCachePath']=str(args.host.parent/'DerivedDataCache');env['UE-SharedDataCachePath']='None'
        game=subprocess.Popen([str(args.engine/'Binaries/Win64/UnrealEditor-Cmd.exe'),str(args.host),'-game','-nosound','-unattended','-NoSplash','-DDC=InstalledNoZenLocalFallback',
            '-stdout','-FullStdOutLogOutput','-CyInsightsGpu',f'-CyInsightsReShade={args.reshade}',f'-CyInsightsGpuAddon={args.addon}',f'-CyInsightsRemotePort={port}'],
            stdout=log,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW,env=env)
        try:
            deadline=time.monotonic()+90
            while True:
                if game.poll() is not None:raise RuntimeError('Owned rendered host exited')
                try:client=socket.create_connection(('127.0.0.1',port),timeout=1);break
                except OSError:
                    if time.monotonic()>deadline:raise RuntimeError('Host startup timed out')
                    time.sleep(.2)
            with client,client.makefile('rb') as replies,socket.socket() as receiver:
                client.settimeout(15)
                def rpc(command,**kwargs):
                    client.sendall((json.dumps(dict(version=1,command=command,**kwargs))+'\n').encode())
                    result=json.loads(replies.readline())
                    if not result.get('ok'):raise RuntimeError(result)
                    return result
                hello=rpc('hello')
                assert not hello['capture_defaults']['gpu_details'] and not hello['capture_defaults']['gpu_readbacks']
                receiver.bind(('127.0.0.1',0));receiver.listen(1);receiver.settimeout(30)
                errors=[]
                def drain():
                    try:
                        data,_=receiver.accept()
                        with data,(args.output/'unreal.utrace').open('wb') as out:
                            data.settimeout(30)
                            while block:=data.recv(1024*1024):out.write(block)
                    except Exception as error:errors.append(str(error))
                thread=threading.Thread(target=drain);thread.start()
                started=rpc('start',receiver_port=receiver.getsockname()[1])
                target=args.output/'gpu'/'frame';target.parent.mkdir()
                with (args.output/'recorder.log').open('w') as recorder_log:
                    recorder=subprocess.Popen([str(args.recorder),str(game.pid),str(target),'--plugin-request'],stdout=recorder_log,stderr=subprocess.STDOUT,
                        creationflags=subprocess.CREATE_NO_WINDOW)
                    ready=target.parent/(target.name+'.ready')
                    deadline=time.monotonic()+60
                    while not ready.exists():
                        if recorder.poll() is not None:raise RuntimeError('CyInsights recorder failed before connection')
                        if time.monotonic()>deadline:raise RuntimeError('Recorder connection timed out')
                        time.sleep(.1)
                    status=rpc('status');assert status['capture_capabilities']['gpu_request']
                    requested=rpc('gpu',session_id=started['session_id'],capture_options=dict(version=1,gpu_frames=1,gpu_bindings=False,
                        gpu_barriers=False,gpu_per_command=False,gpu_readbacks=False))
                    assert requested['state']=='recording'
                    while recorder.poll() is None:
                        if time.monotonic()>deadline:raise RuntimeError('Recorder capture timed out')
                        rpc('status');time.sleep(.2)
                    if recorder.returncode:raise RuntimeError('GPU recorder validation failed')
                rpc('stop',session_id=started['session_id']);thread.join(20)
                if thread.is_alive() or errors:raise RuntimeError('Native trace drain failed: '+str(errors))
                manifest=rpc('status')['manifest']
                gpu=json.loads((target/'validation.json').read_text())
                assert gpu['execution_frame']==gpu['gpu_timing_frame'] and gpu['draws']>0 and not gpu['readbacks']
                manifest['sources'].append(dict(id='gpu.commands',status='measured',path='gpu/frame/capture.json',
                    reason='Optional internal CyInsights collector; frame execution timestamps, CPU association approximate'))
                # Native manifest reports this source as unavailable until detailed files are assembled.
                manifest['sources']=[s for s in manifest['sources'] if s['id']!='gpu.commands' or s.get('path')=='gpu/frame/capture.json']
                (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
                print(json.dumps(gpu),flush=True)
        finally:
            if recorder and recorder.poll() is None:recorder.terminate();recorder.wait(timeout=15)
            if game.poll() is None:game.terminate()
            game.wait(timeout=15)


if __name__=='__main__':main()
