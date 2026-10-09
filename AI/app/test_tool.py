import json
import tempfile
from pathlib import Path
from unittest.mock import patch
import pytest
from cytools_core import CyToolError, load_manifest
from tool import create_runtime

def test_contract():
    manifest=load_manifest(Path(__file__).with_name('CyTool.json'))
    assert len(manifest['operations'])==17
    assert manifest['concurrency']['policy']=='Concurrent'
    annotation=next(op for op in manifest['operations'] if op['id']=='publish_finding')
    assert set(annotation['permissions'])=={'annotations','appControl'}

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
