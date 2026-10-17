"""Official MCP client -> SDK jobs -> owned native CyInsights instance.

Run explicitly with local synthetic .utrace fixtures and prepared readers. Never
connects to a user's existing app/game, starts a device capture or changes AV.
Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""
import argparse
import asyncio
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client


async def verify(args, process):
    root=Path(__file__).resolve().parents[1]
    env=dict(os.environ,CYINSIGHTS_APP_PID=str(process.pid),CYINSIGHTS_DOCUMENT_ID='0')
    env.pop('CYTOOLS_TOKEN',None)
    companion=args.app.resolve().parent/'AI'
    if not (companion/'app/tool.py').is_file():companion=root/'AI'
    python=companion/'runtime/python/Scripts/python.exe'
    config=StdioServerParameters(command=str(python) if python.is_file() else sys.executable,args=[str(companion/'app/tool.py'),'--data-dir',str(args.report.parent/'sdk-jobs'),'mcp'],env=env)
    checked=[]
    def check(value,name):
        assert value,name
        checked.append(name)
    async with stdio_client(config) as (reader,writer),ClientSession(reader,writer) as client:
        await client.initialize()
        names={t.name for t in (await client.list_tools()).tools}
        manifest=json.loads((root/'AI/app/CyTool.json').read_text(encoding='utf-8'))
        check(all('op_'+op['id'] in names for op in manifest['operations']),'All manifest operations discoverable over official MCP')
        async def raw(name,p):
            value=await client.call_tool(name,p)
            assert not value.isError,value
            return value.structuredContent['result']
        async def invoke(op,p=None,expected='Completed'):
            value=await raw('op_'+op,{'parameters':p or {}})
            job=value['jobId'];deadline=time.monotonic()+120
            while time.monotonic()<deadline:
                result=await raw('cy_get_job',{'jobId':job})
                if result['state'] in ('Completed','Failed','Cancelled'):
                    assert result['state']==expected,(op,result)
                    return result['outputs']['data'] if expected=='Completed' else result
                await asyncio.sleep(.05)
            raise TimeoutError(op)
        async def ready(provider=None):
            deadline=time.monotonic()+120
            while time.monotonic()<deadline:
                assert process.poll() is None,process.returncode
                state=await invoke('get_state')
                if state['ready'] and (not provider or not state[provider]['loading']) and not state['busy']:return state
                await asyncio.sleep(.1)
            raise TimeoutError(('native ready',provider,state))
        state=await ready()
        capabilities=await invoke('get_capabilities')
        check(len(capabilities['operations'])==len(manifest['operations']),'Native catalogue matches MCP manifest')
        check(capabilities['limitations']['incomingTraceDecoding']=='unavailable_analyze_after_stop','Unsupported live decoding explicit')
        method=(await invoke('get_analysis_guide'))['method']
        check(method['id']=='cyinsights.trace-analysis' and method['version']==1,'Versioned analysis method available through MCP')
        await invoke('focus_view',{'view':'ai_method'})
        await invoke('get_metadata')
        check((await invoke('query_scopes',{'limit':2}))['rows'],'Native scopes returned through SDK job')
        await invoke('set_range',{'start':state['domain']['start'],'end':state['domain']['end'],'focus':False})
        check((await invoke('query_frames',{'limit':2}))['rows'],'Frame costs exposed through MCP')
        relations=await invoke('query_relations',{'function':'FEngineLoop::Tick','direction':'callees','limit':2})
        check('matched' in relations,'Native relations available without display aggregates')
        check((await invoke('query_gpu'))['availability']=='unavailable','Missing optional Windows GPU commands remain unavailable')
        check((await invoke('open_comparison',{'path':str(args.trace.resolve())}))['accepted'],'Comparison B load accepted')
        await ready('comparison')
        comparison=await invoke('query_comparison',{'metric':'inclusive','mode':'per_occurrence','limit':5})
        check(comparison['rows'] and all(abs(r['delta'])<1e-9 for r in comparison['rows'] if r['delta'] is not None),'Same fixture A/B has zero normalized delta')
        await invoke('load_counters',{'frameOnly':True});await ready('counters')
        await invoke('list_counters',{'limit':2})
        await invoke('load_activities');state=await ready('activities')
        check(state['activities']['loaded'],'Native activity reader completed through MCP')
        lanes=await invoke('list_activity_lanes',{'kind':'network','limit':2})
        check(lanes['rows'],'Network lane recorded in fixture')
        activities=await invoke('query_activities',{'kind':'network','includeEvents':True,'bins':4,'limit':1})
        check(activities['rows'] and len(activities['rows'][0]['bins'])==4,'Network events and bins reach real MCP client')
        denied=await invoke('capture_start',{'ipv4':'127.0.0.1'},expected='Failed')
        check(denied['error']['code']=='AccessDenied','Separate remote capture permission remains enforced')
        await invoke('open_trace',{'path':str(args.stats_trace.resolve())});state=await ready()
        await invoke('load_counters',{'frameOnly':True});await ready('counters')
        counters=await invoke('list_counters',{'group':'Unit','limit':128})
        check(counters['rows'],'Per-frame counter catalogue loaded')
        observed=[row for row in counters['rows'] if int(row['samples'])>0]
        check(observed,'Recorded per-frame sample provider loaded')
        # NullRHI intentionally has unavailable thread/GPU gauges. The observed
        # frame span is measured in this fixture; those gauges must stay null.
        counter=next(row['index'] for row in observed if 'Observed frame span' in row['name'])
        revision=counters['counterRevision']
        values=await invoke('query_counters',{'counter':counter,'counterRevision':revision,'start':state['domain']['start'],'end':state['domain']['end'],'includeSamples':True,'bins':4,'limit':2})
        if values['rows'][0]['count']=='0':
            args.report.with_suffix('.diagnostic.json').write_text(json.dumps({'state':state,'counters':counters,'values':values},indent=2),encoding='utf-8')
        check(values['rows'][0]['count']!='0','Recorded per-frame values reach MCP')
        frames=await invoke('query_frame_stats',{'counter':counter,'limit':3})
        check(frames['rows'],'Per-frame stat values and null policy reach MCP')
        await invoke('pin_counter',{'counter':counter,'counterRevision':revision,'pinned':True})
        stale=await invoke('query_counters',{'counter':counter,'counterRevision':'999999999'},expected='Failed')
        check(stale['error']['code']=='StaleCounters','Stale counter index rejected before native worker starts')
        await invoke('focus_view',{'view':'frame_stats'})
        await invoke('load_tasks');await ready('tasks')
        tasks=await invoke('query_tasks',{'limit':2,'candidates':True})
        check(tasks['provenance']=='heuristic_cpu_scope_candidates','Task fallback remains explicitly heuristic')
        args.report.write_text(json.dumps({'passed':True,'operations':len(manifest['operations']),'checks':checked,'methodVersion':method['version'],'activityRows':len(activities['rows']),'counterRows':len(values['rows'])},indent=2),encoding='utf-8')
        print(f'{len(checked)} official MCP/native integration checks passed',flush=True)


def main():
    parser=argparse.ArgumentParser()
    for name in ('app','trace','stats-trace','profile','report'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();args.report.parent.mkdir(parents=True,exist_ok=True)
    before={p:hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.trace,args.stats_trace)}
    env=dict(os.environ,LOCALAPPDATA=str(args.profile.resolve()))
    process=subprocess.Popen([str(args.app.resolve()),'--ai-control','--ai-view',str(args.trace.resolve())],cwd=args.app.resolve().parent,env=env)
    try:
        time.sleep(1)
        asyncio.run(verify(args,process))
        assert all(hashlib.sha256(p.read_bytes()).hexdigest()==digest for p,digest in before.items()),'Source fixtures modified'
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=15)


if __name__=='__main__':main()
