"""Capture basic/custom/basic/off profiles from our test-owned Unreal host.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import argparse
import json
import os
from pathlib import Path
import socket
import struct
import subprocess
import threading
import time

def verify_decoded(folder, index):
    data=(folder/'frame-stats.cystats').read_bytes()
    assert data[:8]==b'CYFS0001'
    pos=8;metrics={};active=set();samples=0
    while True:
        tag=data[pos];pos+=1
        if tag==1:
            metric, _=struct.unpack_from('<IB',data,pos);pos+=5;fields=[]
            for _ in range(3):
                n=struct.unpack_from('<I',data,pos)[0];pos+=4
                fields.append(data[pos:pos+n].decode('utf8'));pos+=n
            metrics[metric]=fields
        elif tag==2:
            active.add(struct.unpack_from('<I',data,pos)[0]);pos+=64;samples+=1
        elif tag==255:
            expected=struct.unpack_from('<Q',data,pos)[0];pos+=8
            assert expected==samples and pos==len(data)
            break
        else: raise AssertionError(f'Invalid cache tag: {tag}')
    manifest=json.loads((folder/'manifest.json').read_text(encoding='utf8'))
    source=next(s for s in manifest['sources'] if s['id']=='unreal.frame_stats')
    assert int(source['sample_count'])==samples
    # Important spec declarations can be replayed from an earlier capture.
    # Only IDs with actual values describe what this profile collected.
    groups={metrics[key][0] for key in active}
    if index in (0,2): assert len(active)==7 and groups=={'Unit','GPU','UObjects'}
    elif index==1: assert len(active)>9 and {'Game','Memory'}.issubset(groups)
    else: assert not active and samples==0
    return dict(profile=index,active_metrics=len(active),samples=samples,groups=sorted(groups),trace_bytes=(folder/'unreal.utrace').stat().st_size)

def verify_gpu(folder, index):
    # Read the independent decoded timing cache, not the requested profile.
    with (folder/'timing.cynt').open('rb') as stream:
        def value(fmt): return struct.unpack(fmt,stream.read(struct.calcsize(fmt)))
        def text():
            n,=value('<I');return stream.read(n).decode('utf8')
        assert stream.read(8)==b'CYNTIM01'
        timers={}
        count,=value('<I')
        for _ in range(count):
            key,=value('<Q');timers[key]=text()
        tracks,=value('<I');gpu=[]
        for _ in range(tracks):
            key,=value('<Q');name=text();count,=value('<Q')
            if key>>63:
                names=set();valid=0;max_depth=0
                for _ in range(count):
                    a,b,timer,depth,flags,reserved=value('<ddIIII')
                    if b>a and not flags: valid+=1;names.add(timers[timer]);max_depth=max(max_depth,depth)
                gpu.append(dict(track=name,events=count,valid_intervals=valid,max_depth=max_depth,scopes=sorted(names)))
            else:stream.seek(count*32,1)
        assert sum(t['valid_intervals'] for t in gpu)>0, 'No measured GPU intervals in rendered capture'
        scopes={name for t in gpu for name in t['scopes']}
        assert {'BasePass','PostProcessing'}.issubset(scopes), 'Default rendering groups missing'
        if index==1:
            assert any('Clear' in name for name in scopes), 'RDG metadata group labels were not decoded'
        return dict(profile=index,tracks=gpu)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, required=True)
    parser.add_argument('--host', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reader', type=Path, help='Optional already-built native reader; decode and verify all four profiles.')
    parser.add_argument('--rendered',action='store_true',help='Use the real RHI and verify measured native GPU intervals with --reader; no NullRHI.')
    args = parser.parse_args()
    if args.rendered and not args.reader:parser.error('--rendered requires --reader')
    args.engine=args.engine.resolve();args.host=args.host.resolve();args.output=args.output.resolve()
    args.output.mkdir(parents=True, exist_ok=True)
    # UBT's -Module checks link DLLs without writing runtime discovery manifests.
    # Create manifests only in our generated host, using the installed build ID.
    prefix = 'UnrealEditor' if (args.engine/'Binaries/Win64/UnrealEditor.modules').exists() else 'UE4Editor'
    build_id = json.loads((args.engine/f'Binaries/Win64/{prefix}.modules').read_text())['BuildId']
    for folder in [args.host.parent/'Binaries/Win64', *args.host.parent.glob('Plugins/*/Binaries/Win64')]:
        modules = {dll.stem[len(prefix)+1:]: dll.name for dll in folder.glob(f'{prefix}-*.dll')}
        (folder/f'{prefix}.modules').write_text(json.dumps(dict(BuildId=build_id, Modules=modules)), encoding='utf-8')
    with socket.socket() as probe:
        probe.bind(('127.0.0.1', 0))
        port = probe.getsockname()[1]
    editor = args.engine / 'Binaries/Win64/UnrealEditor-Cmd.exe'
    if not editor.exists(): editor = args.engine / 'Binaries/Win64/UE4Editor-Cmd.exe'
    with (args.output / 'host.log').open('w', encoding='utf-8') as log:
        process = subprocess.Popen([str(editor), str(args.host), '-game', *([] if args.rendered else ['-nullrhi']), '-nosound', '-unattended', '-NoSplash',
            '-stdout', '-FullStdOutLogOutput', f'-CyInsightsRemotePort={port}'], stdout=log, stderr=subprocess.STDOUT,
            creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            deadline = time.monotonic() + 90
            while True:
                if process.poll() is not None: raise RuntimeError('Owned Unreal host exited before capture')
                try:
                    client = socket.create_connection(('127.0.0.1', port), timeout=1)
                    break
                except OSError:
                    if time.monotonic() > deadline: raise RuntimeError('Owned Unreal host startup timeout')
                    time.sleep(.2)
            with client, client.makefile('rb') as control:
                client.settimeout(15)
                def rpc(command, expect_ok=True, **kwargs):
                    client.sendall((json.dumps(dict(version=1, command=command, **kwargs))+'\n').encode())
                    response = json.loads(control.readline())
                    if bool(response.get('ok')) != expect_ok: raise RuntimeError(response)
                    return response
                hello=rpc('hello');defaults=hello['capture_defaults']
                assert defaults['quest_hz']==0 and defaults['quest_gpu_completion'] and not defaults['gpu_passes']
                assert not defaults['stat_groups_enabled'] and not defaults['memory_stats'] and not defaults['gpu_readbacks'] and not defaults['gpu_details']
                for bad in ({'version':1,'quest_gpu_completion':1},{'version':1,'gpu_readbacks':1},{'version':1,'gpu_frames':9},{'version':1,'unexpected':True}):
                    refused=rpc('start',expect_ok=False,receiver_port=40000,capture_options=bad)
                    assert refused['state']=='idle' and not refused['native_trace_active']
                profiles=(None,dict(version=1,memory_stats=True,stat_groups_enabled=True,stat_groups='Game,UObjects,GPU',gpu_passes=args.rendered),None,dict(version=1,frame_stats=False))
                for index, profile in enumerate(profiles):
                    directory = args.output / str(index)
                    directory.mkdir(exist_ok=True)
                    with socket.socket() as receiver:
                        receiver.bind(('127.0.0.1', 0));receiver.listen(1);receiver.settimeout(15)
                        errors = []
                        def drain():
                            try:
                                data, _ = receiver.accept()
                                with data, (directory/'unreal.utrace').open('wb') as out:
                                    data.settimeout(15)
                                    while block := data.recv(1024*1024): out.write(block)
                            except Exception as exc: errors.append(str(exc))
                        worker = threading.Thread(target=drain);worker.start()
                        started = rpc('start', receiver_port=receiver.getsockname()[1], seconds=0,**({'capture_options':profile} if profile else {}))
                        time.sleep(1)
                        if index==0:
                            rpc('gpu',expect_ok=False,session_id='wrong-session',capture_options=dict(version=1,gpu_frames=1))
                            refused=rpc('gpu',expect_ok=False,session_id=started['session_id'],capture_options=dict(version=1,gpu_frames=1))
                            assert refused['state']=='recording' and not refused['capture_capabilities']['gpu_payload_transfer']
                        rpc('stop', session_id=started['session_id'])
                        worker.join(20)
                        if worker.is_alive() or errors: raise RuntimeError(f'Native drain failed: {errors}')
                        status = rpc('status')
                        assert status['capture_defaults']==defaults, 'Remote override changed project defaults'
                        (directory/'manifest.json').write_text(json.dumps(status['manifest'], indent=2), encoding='utf-8')
                        source = next(s for s in status['manifest']['sources'] if s['id']=='unreal.frame_stats')
                        if index<3:
                            if source['status']!='measured' or int(source['sample_count'])<=0: raise RuntimeError(source)
                        else: assert int(source['sample_count'])==0
                        policy=status['manifest']['capture_options']
                        native_gpu=next(s for s in status['manifest']['sources'] if s['id']=='unreal.gpu.scopes')
                        if args.rendered:
                            assert native_gpu['timestamp_queries_available'] and native_gpu['group_recording_enabled']
                            assert native_gpu['requested_rdg_event_level']==(3 if index==1 else 2)
                            assert native_gpu['applied_rdg_event_level']>=native_gpu['requested_rdg_event_level']
                        assert policy['gpu_passes']==(args.rendered and index==1)
                        assert policy['memory_stats']==(index==1) and policy['stat_groups_enabled']==(index==1)
                        print(f'Capture {index}: {source["sample_count"]} real frame-stat observations', flush=True)
                        if args.reader:
                            env=dict(os.environ);env['PATH']=str(args.engine/'Binaries/Win64')+os.pathsep+env['PATH']
                            subprocess.run([str(args.reader.resolve()),str(directory/'unreal.utrace'),str(directory/'timing.cynt'),str(args.engine/'Binaries/Win64')],env=env,check=True)
                            verified=verify_decoded(directory,index)
                            print(json.dumps(verified),flush=True)
                            if args.rendered:
                                gpu=verify_gpu(directory,index)
                                (directory/"gpu-summary.json").write_text(json.dumps(gpu,indent=2),encoding="utf8")
                                print(json.dumps({"profile":index,"gpu_intervals":sum(t["valid_intervals"] for t in gpu["tracks"])}),flush=True)
        finally:
            if process.poll() is None: process.terminate()
            process.wait(timeout=15)

if __name__ == '__main__': main()
