import json
import tempfile
from pathlib import Path
from unittest.mock import patch
import pytest
from cytools_core import CyToolError, load_manifest
from tool import create_runtime

def test_contract():
    manifest=load_manifest(Path(__file__).with_name('CyTool.json'))
    assert len(manifest['operations'])==39
    assert manifest['concurrency']['policy']=='Concurrent'
    annotation=next(op for op in manifest['operations'] if op['id']=='publish_finding')
    assert set(annotation['permissions'])=={'annotations','appControl'}
    capture=next(op for op in manifest['operations'] if op['id']=='capture_start')
    assert capture['inputSchema']['properties']['meta_gpu']['properties']['restart_game']['default'] is False

def test_jobs_output_isolation_and_persistent_source():
    with tempfile.TemporaryDirectory() as root:
        original=Path(root)/'source.utrace';original.write_bytes(b'Native source stays untouched')
        with create_runtime(Path(root)/'jobs') as runtime:
            alice=runtime.authenticate(runtime.create_client('Alice',['appControl'])['token'])
            bob=runtime.authenticate(runtime.create_client('Bob')['token'])
            session=runtime.create_session(alice,temporary=True)
            with patch('tool.native_call',return_value={'accepted':True}) as call:
                job=runtime.submit(alice,session['sessionId'],'open_trace',{'path':str(original)})
                result=runtime.wait(alice,job['jobId'])
            assert result['state']=='Completed', result
            assert call.call_args[0][1]['path']==str(original)
            report=Path(result['outputs']['reportFile']);assert json.loads(report.read_text())=={'accepted':True}
            assert runtime.list_jobs(bob)==[]
            with pytest.raises(CyToolError):runtime.get_job(bob,job['jobId'])
            runtime.finish_task(alice,session['sessionId'])
            assert not report.exists() and original.exists()

def test_permission_validation_and_errors():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        actor=runtime.authenticate(runtime.create_client('readonly')['token']);session=runtime.create_session(actor)
        with pytest.raises(CyToolError):runtime.submit(actor,session['sessionId'],'open_trace',{'path':'D:/no.utrace'})
        with pytest.raises(CyToolError):runtime.submit(actor,session['sessionId'],'histogram',{'limit':129})
        with pytest.raises(CyToolError):runtime.submit(actor,session['sessionId'],'histogram',{'unexpected':1})
        annotator=runtime.authenticate(runtime.create_client('annotation-only',['annotations'])['token'])
        annotator_session=runtime.create_session(annotator)
        with pytest.raises(CyToolError):runtime.submit(annotator,annotator_session['sessionId'],'publish_finding',{'generation':'0','start':0,'end':1,'title':'denied','detail':''})
        with patch('tool.native_call',side_effect=CyToolError('NoTrace','Open a trace')):
            job=runtime.submit(actor,session['sessionId'],'get_state',{})
            assert runtime.wait(actor,job['jobId'])['state']=='Failed'

def test_defaults_and_compact_native_request():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        actor=runtime.authenticate(runtime.create_client('reader')['token']);session=runtime.create_session(actor)
        with patch('tool.native_call',return_value={'summary':{'count':'14192325'}}) as call:
            job=runtime.submit(actor,session['sessionId'],'histogram',{'function':'FEngineLoop::Tick'})
            result=runtime.wait(actor,job['jobId'])
        assert result['state']=='Completed'
        p=call.call_args[0][1];assert p['limit']==20 and not p['includeBins'] and not p['rootsOnly']
        assert result['outputs']['data']['summary']['count']=='14192325'

def test_missing_connection():
    from bridge import native_call
    with patch.dict('os.environ',{'CYINSIGHTS_APP_PID':''}):
        with pytest.raises(CyToolError) as error:native_call('get_state',{})
        assert error.value.code=='MissingConnection'

def test_capture_options_are_validated_and_forwarded():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        actor=runtime.authenticate(runtime.create_client('capture',['remoteCapture'])['token']);session=runtime.create_session(actor)
        options={'version':1,'memory_stats':True,'stat_groups_enabled':True,'stat_groups':'Game,GPU','quest_hz':0,'gpu_passes':True,'quest_gpu_completion':False,'gpu_frames':2,'gpu_readbacks':False}
        with patch('tool.native_call',return_value={'accepted':True}) as call:
            job=runtime.submit(actor,session['sessionId'],'capture_start',{'ipv4':'127.0.0.1','capture_options':options})
            assert runtime.wait(actor,job['jobId'])['state']=='Completed'
            assert call.call_args[0][1]['capture_options']==options
            gpu=runtime.submit(actor,session['sessionId'],'capture_gpu',{'capture_options':{'version':1,'gpu_frames':2,'gpu_readbacks':False}})
            assert runtime.wait(actor,gpu['jobId'])['state']=='Completed'
        for bad in ({'version':1,'gpu_readbacks':1},{'version':1,'gpu_frames':9},{'version':1,'unknown':True}):
            with pytest.raises(CyToolError):runtime.submit(actor,session['sessionId'],'capture_start',{'ipv4':'127.0.0.1','capture_options':bad})

def test_meta_gpu_options_are_typed_and_forwarded():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        actor=runtime.authenticate(runtime.create_client('meta',['remoteCapture'])['token']);session=runtime.create_session(actor)
        options={'enabled':True,'restart_game':False,'adb_path':'C:/Android/platform-tools/adb.exe','device':'192.0.2.5:5555','package':'com.test.Game'}
        with patch('tool.native_call',return_value={'accepted':True}) as call:
            job=runtime.submit(actor,session['sessionId'],'capture_start',{'ipv4':'192.0.2.5','meta_gpu':options})
            assert runtime.wait(actor,job['jobId'])['state']=='Completed'
            assert call.call_args[0][1]['meta_gpu']==options
        for bad in ({'enabled':1},{'restart_game':'no'},{'package':True},{'unknown':1}):
            with pytest.raises(CyToolError):runtime.submit(actor,session['sessionId'],'capture_start',{'ipv4':'192.0.2.5','meta_gpu':bad})

def test_vulkan_pass_query_forwarding_and_bounds():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        actor=runtime.authenticate(runtime.create_client('pass-reader')['token']);session=runtime.create_session(actor)
        with patch('tool.native_call',return_value={'availability':'measured_arguments','matchedSubpasses':'3','rows':[]}) as call:
            job=runtime.submit(actor,session['sessionId'],'query_vulkan_passes',{'start':0,'end':1,'passId':'21','limit':3,'offset':2})
            result=runtime.wait(actor,job['jobId']);assert result['state']=='Completed'
            assert call.call_args[0][1]['passId']=='21' and call.call_args[0][1]['offset']==2
        for bad in ({'start':0,'end':1,'limit':129},{'start':0,'end':1,'passId':'../x'}):
            with pytest.raises(CyToolError):runtime.submit(actor,session['sessionId'],'query_vulkan_passes',bad)


def test_extended_provider_contracts_and_forwarding():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        reader=runtime.authenticate(runtime.create_client('extended-reader')['token'])
        session=runtime.create_session(reader)
        recipes={
            'get_analysis_guide':{},'get_capabilities':{},'get_metadata':{},
            'list_counters':{'metaOnly':True,'group':'GPU','offset':2,'limit':3},
            'query_counters':{'counter':2,'start':1,'end':2,'includeSamples':True,'sampleOffset':3,'bins':8},
            'query_frame_stats':{'counter':2,'start':1,'end':2,'offset':3},
            'list_activity_lanes':{'kind':'network'},
            'query_activities':{'kind':'storage','lane':1,'page':2,'bins':8,'includeEvents':True},
            'query_comparison':{'start':1,'end':2,'startB':3,'endB':5,'mode':'per_occurrence','metric':'inclusive','domain':'gpu'},
            'query_scopes':{'function':'FEngineLoop::Tick','track':0,'rootsOnly':True,'offset':3},
            'query_frames':{'targetFps':72,'limit':3},
            'query_relations':{'function':'Work','direction':'callees'},
            'query_task_links':{'taskId':'9007199254740993'},
            'query_gpu':{'section':'commands','pipeline':8,'includeBindings':True},
        }
        for operation,parameters in recipes.items():
            with patch('tool.native_call',return_value={'tested':operation}) as call:
                job=runtime.submit(reader,session['sessionId'],operation,parameters)
                result=runtime.wait(reader,job['jobId'])
                assert result['state']=='Completed',result
                assert call.call_args[0][0]==operation
                for key,value in parameters.items():assert call.call_args[0][1][key]==value
        for operation,parameters in (
            ('load_counters',{}),('load_activities',{}),('open_comparison',{'path':'D:/capture.utrace'}),
            ('pin_counter',{'counter':0}),('focus_view',{'view':'activities'}),('cancel_loading',{'provider':'tasks'})):
            with pytest.raises(CyToolError):runtime.submit(reader,session['sessionId'],operation,parameters)
        for operation,parameters in (
            ('query_counters',{'bins':129}),('query_activities',{'kind':'fake'}),
            ('query_frame_stats',{}),('query_gpu',{'section':'textures'}),
            ('query_task_links',{'taskId':'../file'}),('query_comparison',{'mode':'fake'})):
            with pytest.raises(CyToolError):runtime.submit(reader,session['sessionId'],operation,parameters)


def test_guide_covers_every_native_operation_and_evidence_rules():
    manifest=load_manifest(Path(__file__).with_name('CyTool.json'))
    method=json.loads(Path(__file__).with_name('analysis-method.json').read_text(encoding='utf-8'))
    assert method['version']==1 and method['boundaries']
    text=json.dumps(method)
    for operation in ('query_comparison','query_activities','query_frame_stats','query_gpu','query_relations'):
        assert operation in text
    assert 'never instructions' in method['trust'] and 'generation' in text
    assert len({op['id'] for op in manifest['operations']})==len(manifest['operations'])


def test_extended_application_controls_forward_exact_options():
    with tempfile.TemporaryDirectory() as root, create_runtime(root) as runtime:
        actor=runtime.authenticate(runtime.create_client('controller',['appControl'])['token'])
        session=runtime.create_session(actor)
        recipes={'load_counters':{'frameOnly':True},'load_activities':{},
                 'open_comparison':{'path':'D:/test capture.utrace'},
                 'focus_view':{'view':'frame_stats'},'pin_counter':{'counter':8,'pinned':False},
                 'cancel_loading':{'provider':'comparison'}}
        for operation,parameters in recipes.items():
            with patch('tool.native_call',return_value={'accepted':True}) as call:
                job=runtime.submit(actor,session['sessionId'],operation,parameters)
                assert runtime.wait(actor,job['jobId'])['state']=='Completed'
                assert call.call_args[0][0]==operation
                for key,value in parameters.items():assert call.call_args[0][1][key]==value
