"""Build staging for .cycapture; preserve native and platform collector payloads.
Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
"""
import argparse
import json
import shutil
import uuid
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--native-staging', type=Path, help='Saved/CyInsights/<session> from the Unreal plugin')
parser.add_argument('--utrace', type=Path, help='Standalone trace, e.g. pulled from a Quest')
parser.add_argument('--gpu', type=Path, help='CyInsightsApp Captures/<session> or one exported frame directory')
parser.add_argument('--meta-csv', type=Path, help='Unmodified OVR Metrics Tool report')
parser.add_argument('--output', required=True, type=Path, help='New staging directory; pack with CyCaptureTool')
parser.add_argument('--platform', default='Win64')
parser.add_argument('--engine', default='unknown')
parser.add_argument('--project', default='unknown')
args = parser.parse_args()
if bool(args.native_staging) == bool(args.utrace):
    parser.error('Choose exactly one of --native-staging or --utrace')
if args.output.exists():
    parser.error('Output must be a new directory')
if args.native_staging:
    manifest = json.loads((args.native_staging / 'manifest.json').read_text(encoding='utf-8-sig'))
    shutil.copytree(args.native_staging, args.output)
else:
    args.output.mkdir(parents=True)
    shutil.copy2(args.utrace, args.output / 'unreal.utrace')
    manifest = {'format': 'cycapture', 'version': 1, 'session_id': str(uuid.uuid4()),
                'metadata': {'platform': args.platform, 'engine_version': args.engine, 'project': args.project},
                'sources': [{'id': 'unreal.trace', 'status': 'measured', 'path': 'unreal.utrace'}]}
if args.gpu:
    frames = [args.gpu] if (args.gpu / 'events.bin').is_file() else [p.parent for p in args.gpu.rglob('events.bin')]
    if not frames:
        raise SystemExit('No exported command frames (events.bin). Buffer-only Images directories are not command captures.')
    for index, frame in enumerate(frames):
        dest = args.output / 'gpu' / str(index)
        shutil.copytree(frame, dest)
        manifest['sources'].append({'id': f'gpu.commands.{index}', 'status': 'measured',
                                    'path': dest.relative_to(args.output).as_posix() + '/capture.json',
                                    'collector': 'cygpuinspector.reshade',
                                    'frame_relation': 'unavailable',
                                    'reason': 'Frame identity is collector-local; native CPU/GPU calibration unavailable'})
    manifest['sources'] = [s for s in manifest['sources'] if s['id'] != 'gpu.commands']
if args.meta_csv:
    dest = args.output / 'meta';dest.mkdir(exist_ok=True)
    shutil.copy2(args.meta_csv, dest / 'metrics.csv')
    manifest['sources'].append({'id': 'meta.metrics', 'status': 'measured', 'path': 'meta/metrics.csv',
                                'clock_relation': 'unavailable',
                                'reason': 'Raw report retained; units/clock require a report-version adapter'})
(args.output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(f'Prepared {args.output}. Close all capture writers before packing.')
