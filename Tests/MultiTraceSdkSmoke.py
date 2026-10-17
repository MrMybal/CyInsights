"""Owned native UI integration test; requires prepared readers and test traces.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
Run explicitly; never connects to a previously running application or device.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--trace-a', type=Path, required=True)
    parser.add_argument('--trace-b', type=Path, required=True)
    parser.add_argument('--profile', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'AI/app'))
    from bridge import native_call
    from cytools_core import CyToolError

    env = dict(os.environ, LOCALAPPDATA=str(args.profile.resolve()))
    process = subprocess.Popen([str(args.app.resolve()), '--ai-control-all',
                                str(args.trace_a.resolve()), str(args.trace_b.resolve())],
                               cwd=args.app.resolve().parent, env=env)
    previous = {key: os.environ.get(key) for key in
                ('CYINSIGHTS_APP_PID', 'CYINSIGHTS_DOCUMENT_ID')}
    try:
        os.environ['CYINSIGHTS_APP_PID'] = str(process.pid)
        states = []
        for document in ('0', '1'):
            os.environ['CYINSIGHTS_DOCUMENT_ID'] = document
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    raise RuntimeError(f'Owned UI exited: {process.returncode}')
                try:
                    state = native_call('get_state', {})
                    if state['ready'] and not state['busy']:
                        assert state['documentId'] == document
                        states.append(state)
                        break
                except CyToolError as error:
                    if error.code != 'MissingConnection':
                        raise
                time.sleep(.1)
            else:
                raise RuntimeError('Document did not finish loading')
        for document, name in (('0', 'GPU-pass-A'), ('1', 'CPU-scope-B')):
            os.environ['CYINSIGHTS_DOCUMENT_ID'] = document
            assert native_call('filter_function', {'function': name})['accepted']
        results = []
        for document, name in (('0', 'GPU-pass-A'), ('1', 'CPU-scope-B')):
            os.environ['CYINSIGHTS_DOCUMENT_ID'] = document
            result = native_call('get_state', {})
            assert result['documentId'] == document and result['exactFunction'] == name
            results.append({'documentId': document, 'filter': result['exactFunction'],
                            'events': result['events']})
        args.report.write_text(json.dumps({'passed': True, 'documents': results}, indent=2), encoding='utf-8')
        print('Two native document endpoints: independent filters and trace state passed')
    finally:
        # This process was started above by this test; other app instances stay open.
        if process.poll() is None:
            process.terminate()
            process.wait(timeout=15)
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


if __name__ == '__main__':
    main()
