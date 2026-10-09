import json
import os
import sys
from pathlib import Path
from cytools_core import Runtime, load_manifest
from cytools_core.transports.cli import run_cli
from bridge import native_call


def create_runtime(data_dir, **options):
    manifest=load_manifest(Path(__file__).with_name('CyTool.json'))
    runtime = Runtime(manifest, data_dir, **options)
    # Application document references are persistent, read-only source paths.
    # They are not task artifacts and must not be copied/purged by FinishTask.
    runtime.configure_inputs(Path(data_dir)/'inputs', fields={})
    def handler(operation):
        def invoke(context, parameters):
            context.cancellation.check()
            context.progress(5,currentStep='Requesting native CyInsights operation')
            value=native_call(operation,parameters,context)
            context.cancellation.check()
            report=context.path('outputs/result.json')
            report.write_text(json.dumps(value,ensure_ascii=False,allow_nan=False,indent=2),encoding='utf-8')
            context.progress(100,currentStep='Native result ready')
            return {'data':value,'reportFile':str(report)}
        return invoke
    for operation in manifest['operations']:runtime.register(operation['id'],handler(operation['id']))
    return runtime


if __name__ == '__main__':
    sys.stdout.reconfigure(encoding='utf-8');sys.stderr.reconfigure(encoding='utf-8')
    argv=sys.argv[1:]
    if '--data-dir' not in argv:
        data=Path(os.environ.get('LOCALAPPDATA',str(Path.home())))/'CyInsights'/'AI'/('runtime-'+str(os.getpid()))
        argv=['--data-dir',str(data),*argv]
    # Standalone MCP is launched by the current Windows user. Native UI gates
    # remain authoritative. Shared HTTP clients use explicitly issued tokens.
    def local_runtime(data_dir,**options):
        runtime=create_runtime(data_dir,**options)
        if not os.environ.get('CYTOOLS_TOKEN'):
            os.environ['CYTOOLS_TOKEN']=runtime.create_client('local Windows CyInsights client',['appControl','remoteCapture','annotations'])['token']
        return runtime
    raise SystemExit(run_cli(local_runtime,argv))
