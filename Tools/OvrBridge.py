#!/usr/bin/env python3
# Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""Bounded CyInsightOVR developer socket client. ADB shell owns the endpoint."""
import argparse,base64,json,pathlib,subprocess

def main():
    p=argparse.ArgumentParser();p.add_argument('--adb',default=str(pathlib.Path.home()/'AppData/Local/Android/Sdk/platform-tools/adb.exe'));p.add_argument('--device',required=True)
    p.add_argument('operation',choices=['draw_counters_prepare','draw_counters_start','draw_counters_stop','snapshot','status','capture','enable_details','overlay_meta_on','overlay_meta_off','heatmap_prepare','heatmap_start','heatmap_update','heatmap_stop','heatmap_remove','companion_capabilities','share_view_request','tiles_record_start','tiles_record_status','tiles_record_stop']);p.add_argument('--package',default='org.cyinsights.ovr');p.add_argument('--seconds',type=float,default=.3);p.add_argument('--output',type=pathlib.Path);p.add_argument('--mode',choices=['hud','fullscreen'],default='hud');p.add_argument('--position',type=int,choices=range(9),default=8);p.add_argument('--ceiling-us',type=float,default=500);p.add_argument('--opacity',type=float,default=.25)
    p.add_argument('--hz',type=float,default=1);p.add_argument('--images',action='store_true');p.add_argument('--recording-id');a=p.parse_args()
    request={'operation':a.operation,'package':a.package,'seconds':a.seconds}
    if a.operation in ('heatmap_start','heatmap_update'):request.update(mode=a.mode,position=a.position,ceiling_us=a.ceiling_us,opacity=a.opacity)
    if a.operation=='tiles_record_start':
        if not .2<=a.hz<=5:p.error('Tile recording cadence must be 0.2 to 5 Hz')
        request.update(hz=a.hz,images=a.images)
    if a.operation=='draw_counters_start':
        if a.hz not in (1,5,10,20):p.error('Counter frequency must be 1, 5, 10 or 20 Hz')
        request['hz']=int(a.hz)
    if a.operation=='tiles_record_stop':
        if not a.recording_id:p.error('--recording-id is required to stop your recording')
        request['recording_id']=a.recording_id
    encoded=base64.b64encode(json.dumps(request).encode('utf-8')).decode('ascii')
    command=[a.adb,'-s',a.device,'shell','CLASSPATH=/data/local/tmp/cyinsightovr-bridge.jar','app_process','/system/bin','org.cyinsights.ovr.ShellClient','--request',encoded]
    raw=subprocess.run(command,check=True,capture_output=True,timeout=30).stdout
    result=json.loads(raw.decode('utf-8'))
    if set(result)=={'error'}:raise RuntimeError(result['error'] or 'CyInsightOVR returned an unspecified error')
    if a.output:
        value=result.get('capture') if a.operation=='snapshot' else result
        if value is None:raise RuntimeError('No completed capture available')
        a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(value,indent=2),encoding='utf-8')
    if a.output:print(json.dumps({'saved':str(a.output),'status':value.get('status'),'surfaces':len(value.get('surfaces',[]))}))
    elif a.operation=='snapshot':
        capture=result.get('capture') or {}
        print(json.dumps({'status':result.get('status'),'capturing':result.get('capturing'),'metrics':len(result.get('metrics',[])),
            'capture':{k:capture.get(k) for k in ('capture_id','status','reason','tile_count')},'surfaces':len(capture.get('surfaces',[])), 'live_tiles':result.get('live_tiles'), 'draw_counters':result.get('draw_counters')},indent=2))
    else:print(json.dumps(result,indent=2))
if __name__=='__main__':main()
