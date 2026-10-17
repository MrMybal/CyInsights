"""Generate an isolated rendered Unreal/Quest test project with the current plugin.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
No engine source modification, game installation or device launch is performed.
"""
import argparse
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists():
        raise SystemExit('Choose a new output directory; existing projects are never overwritten')
    shutil.copytree(ROOT / 'Tests/QuestGpuProject', output)
    shutil.copytree(ROOT / 'Plugins/CyInsights', output / 'Plugins/CyInsights',
                    ignore=shutil.ignore_patterns('Binaries', 'Intermediate', '__pycache__'))
    project = dict(FileVersion=3, Modules=[dict(Name='CyQuestGpuTest', Type='Runtime', LoadingPhase='Default')],
                   Plugins=[dict(Name='CyInsights', Enabled=True), dict(Name='OpenXR', Enabled=True)])
    (output / 'CyQuestGpuTest.uproject').write_text(json.dumps(project, indent=2), encoding='utf-8')
    print(output / 'CyQuestGpuTest.uproject')


if __name__ == '__main__':
    main()
