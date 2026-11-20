"""Local native integration test. No model account, network service or Quest required.

The fixture speaks streaming Chat Completions; tool calls go through CyMCPSDK
to a real CyInsights process. It is a protocol fixture, not an inference test.
"""
import argparse
import json
from pathlib import Path
import subprocess
import threading
import time
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

class Fixture(BaseHTTPRequestHandler):
    attachment_seen=False
    def log_message(self, *_):
        pass

    def do_GET(self):
        body=json.dumps({'data':[{'id':'fixture-model'}]}).encode()
        self.send_response(200)
        self.send_header('Content-Type','application/json')
        self.send_header('Content-Length',str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        request=json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        if 'NATIVE_ATTACHMENT_MARKER' in str(request):Fixture.attachment_seen=True
        self.send_response(200)
        self.send_header('Content-Type','text/event-stream')
        self.send_header('Connection','close')
        self.end_headers()
        def chunk(delta,finish=None):
            event={'choices':[{'index':0,'delta':delta,'finish_reason':finish}]}
            self.wfile.write(('data: '+json.dumps(event)+'\n\n').encode())
            self.wfile.flush()
        try:
            users=[m['content'] for m in request['messages'] if m['role']=='user']
            if 'cancel-test' in str(users[-1]):
                chunk({'content':'Waiting'})
                time.sleep(3)
                chunk({},'stop')
            elif request['messages'][-1]['role']=='tool':
                chunk({'content':'Done: the real function filter changed.'},'stop')
            elif 'remote-test' in str(users[-1]):
                calls=[]
                for index,name in enumerate(('remote.echo','remote.failure')):
                    wire='cy_'+hashlib.sha256(name.encode()).hexdigest()[:32]
                    assert any(t['function']['name']==wire for t in request['tools'])
                    calls.append({'index':index,'id':f'remote-{index}','type':'function','function':{'name':wire,'arguments':'{}'}})
                chunk({'tool_calls':calls},'tool_calls')
            else:
                chunk({'content':'Working'})
                time.sleep(.7)
                tool=next(t['function']['name'] for t in request['tools'] if t['function']['name']=='filter_function')
                chunk({'tool_calls':[{'index':0,'id':'native-filter-1','type':'function','function':{'name':tool,'arguments':json.dumps({'function':'NativeStreamingTest'})}}]},'tool_calls')
            self.wfile.write(b'data: [DONE]\n\n')
            self.wfile.flush()
        except (BrokenPipeError,ConnectionResetError):
            pass

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--binaries',type=Path,required=True)
    p.add_argument('--tests',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    a.binaries=a.binaries.resolve();a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True)
    # Keep test data within this dedicated build directory, using the supported
    # portable layout; no user profile or provider configuration is changed.
    marker=a.binaries/'cyinsights-portable.txt'
    if not marker.exists():marker.write_text('Local native integration verification\n')
    cli=a.binaries/'CyInsightsCli.exe'
    def run(*args,code=0):
        r=subprocess.run([str(cli),*map(str,args)],capture_output=True,text=True,encoding='utf-8',timeout=30)
        assert r.returncode==code,(args,r.returncode,r.stderr)
        return json.loads(r.stdout) if r.stdout.strip() else None
    csv=a.output/'timing.csv'
    csv.write_text('ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n10,GameThread,1,FEngineLoop::Tick,0,0.030,0\n10,GameThread,2,Work,0.001,0.005,1\n')
    assert len(run('list')['operations'])==48
    assert run('describe','histogram')['name']=='histogram'
    assert run('invoke','get_state','--trace',csv)['events']=='2'
    assert run('invoke','get_capabilities','--trace',csv)['providers']['timing']['loaded']
    assert len(run('invoke','get_analysis_guide')['method']['workflow'])==9
    result=run('invoke','histogram','--trace',csv,'--json','{"function":"Work"}')
    assert result['summary']['count']=='1'
    run('invoke','filter_function','--json','{"function":12}',code=2)
    options=a.binaries/'Data/AI/Native/0/options.json'
    options.parent.mkdir(parents=True,exist_ok=True)
    original_options=options.read_bytes() if options.exists() else None
    previous_generation=None
    profiles=[(None,False),('local_controlled',False),('token_only',False),('protected_access',False),('protected_access',True)]
    profile_results=[]
    try:
        for profile,control in profiles:
            saved={'compact':True,'notifications':True,'cacheSeconds':30}
            if profile is not None:saved['accessProfile']=profile
            options.write_text(json.dumps(saved),encoding='utf-8')
            actual_profile=profile or 'free'
            label=actual_profile+('-control' if control else '')
            exit_file=a.output/f'exit-{label}.marker';exit_file.unlink(missing_ok=True)
            process=subprocess.Popen([str(a.binaries/'CyInsightsApp.exe'),'--ai-control' if control else '--ai-enable','--ai-view','--test-exit-file',str(exit_file)])
            try:
                deadline=time.monotonic()+30
                while time.monotonic()<deadline:
                    try:probe=subprocess.run([str(cli),'invoke','get_state','--pid',str(process.pid)],capture_output=True,timeout=5)
                    except subprocess.TimeoutExpired:
                        if process.poll() is not None:raise RuntimeError('CyInsights exited during initialization')
                        continue
                    if probe.returncode==0:break
                    if process.poll() is not None:raise RuntimeError('CyInsights exited before bridge initialization')
                    time.sleep(.2)
                else:raise RuntimeError('Bridge failed to start')
                state=json.loads(probe.stdout);generation=state['generation']
                assert generation!=previous_generation,'Restart reused an AI trace generation'
                previous_generation=generation
                assert state['accessProfile']==actual_profile,state
                assert json.loads(options.read_text(encoding='utf-8'))['accessProfile']==actual_profile
                connection=a.binaries/'Data/AI/Native/0'/f'connection-{process.pid}.json'
                endpoint=json.loads(connection.read_text(encoding='utf-8'));url=endpoint['url']
                required=actual_profile in ('token_only','protected_access')
                allowed=actual_profile in ('free','token_only') or control
                assert endpoint['requiresToken']==required and state['permissions']['appControl']==allowed
                assert state['permissions']['remoteCapture']==(actual_profile in ('free','token_only'))
                assert run('invoke','get_state','--connection',connection)['pid']==str(process.pid)
                if required:
                    run('invoke','get_state','--url',url,code=4)
                    wrong=dict(endpoint);wrong['token']='incorrect-local-token-'*4
                    invalid=a.output/f'invalid-{label}.json';invalid.write_text(json.dumps(wrong),encoding='utf-8')
                    run('invoke','get_state','--connection',invalid,code=4)
                else:
                    assert 'token' not in endpoint
                    assert run('invoke','get_state','--url',url)['pid']==str(process.pid)
                    run('invoke','filter_function','--url',url,'--json','{"function":"AddressOnlyCli"}',code=0 if allowed else 4)
                run('invoke','filter_function','--pid',process.pid,'--json','{"function":"CliProfileTest"}',code=0 if allowed else 4)
                check=subprocess.run([str(a.tests.resolve()),'--http-instance',str(process.pid),str(connection)],capture_output=True,text=True,timeout=30,cwd=a.output)
                assert check.returncode==0,check.stdout+check.stderr
                (a.output/f'http-{label}.txt').write_text(check.stdout,encoding='utf-8')
                if not allowed:
                    check=subprocess.run([str(a.tests.resolve()),'--denied',str(process.pid),str(cli)],capture_output=True,text=True,timeout=30,cwd=a.output)
                    assert check.returncode==0,check.stdout+check.stderr
                if actual_profile=='free':
                    server=ThreadingHTTPServer(('127.0.0.1',0),Fixture)
                    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
                    try:
                        r=subprocess.run([str(a.tests.resolve()),'--instance',str(process.pid),f'http://127.0.0.1:{server.server_port}/v1',str(cli)],capture_output=True,text=True,timeout=60,cwd=a.output)
                        (a.output/'real-instance.txt').write_text(r.stdout+r.stderr,encoding='utf-8')
                        assert r.returncode==0,r.stdout+r.stderr
                        assert Fixture.attachment_seen,'Portable attachment did not reach the provider'
                        image=a.output/'assistant-ui.png'
                        ui=subprocess.run([str(a.binaries/'CyInsightsApp.exe'),'--ai-enable','--ai-chat-smoke',f'http://127.0.0.1:{server.server_port}/v1','--ui-size','1440x900','--snapshot',str(image)],capture_output=True,text=True,timeout=45)
                        assert ui.returncode==0,('Native chat render smoke failed',ui.returncode,ui.stderr)
                        report=json.loads(Path(str(image)+'.chat.json').read_text())
                        assert report['passed'] and report['partialRendered'] and report['cancelled'] and report['toolChangedRealState'],report
                        assert image.is_file() and Path(str(image)+'.partial.png').is_file()
                    finally:server.shutdown();server.server_close()
                profile_results.append({'profile':actual_profile,'tokenRequired':required,'mutationAllowed':allowed})
            finally:
                exit_file.write_text('Close the owned test instance\n')
                try:
                    process.wait(timeout=20)
                    assert process.returncode==0,('Application did not close cleanly',process.returncode)
                finally:
                    if process.poll() is None:process.kill()
    finally:
        if original_options is None:options.unlink(missing_ok=True)
        else:options.write_bytes(original_options)
    (a.output/'profiles.json').write_text(json.dumps(profile_results,indent=2),encoding='utf-8')
    (a.output/'result.json').write_text(json.dumps({'status':'passed','sdk':'0.7.1','cli':'headless and real-instance','chat':'streaming HTTP protocol fixture, real app mutation, cancel and restore','mcp':'native SDK stdio client/server on real instance','permission':'Free/default and all explicit profiles, HTTP token enforcement and shared mutation gates'},indent=2))
    print('Native CLI, chat and MCP end-to-end checks passed')

if __name__=='__main__':main()
