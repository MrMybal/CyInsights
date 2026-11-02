"""Refresh the minimal, pinned CyUI source snapshot from an upstream checkout.

No network access or changes to the source checkout. Review a revision change
before running this tool; application CMake files select the compiled modules.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

REVISION = 'ff023e646c6a3038b3dfbfd53f6d2b4a2f5955e3'
MODULES = ('Canvas', 'Plots', 'Timeline', 'Nodes', 'Inspector', 'Images', 'Diagnostics')
ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    args = parser.parse_args()
    source = args.source.resolve()

    def git(*command):
        return subprocess.check_output(['git', '-c', 'safe.directory=' + source.as_posix(),
                                        '-C', str(source), *command])

    selected = {'CMakeLists.txt', 'LICENSE', 'src/Theme.cpp', 'src/Widgets.cpp',
                'src/Layout.cpp', 'src/Filter.cpp', 'src/modules/ModuleInternal.h'}
    selected.update('src/modules/' + module + '.cpp' for module in MODULES)
    selected.update('docs/' + name + '.md' for name in
                    ('AI_GUIDE', 'MODULES', 'LOCALIZATION', 'FILTERING'))
    selected.update('include/cyui/' + name + '.h' for name in
                    ('Core', 'CoreWidgets', 'Export', 'Filter', 'Icons',
                     'Interaction', 'Layout', 'Locale', 'Theme'))
    selected.update('include/cyui/modules/' + module + '.h' for module in MODULES)
    selected.add('include/cyui/detail/LocaleFrench.inc')
    destination = ROOT / 'ThirdParty/CyUI'
    manifest = destination / 'source-sha256.json'
    previous = json.loads(manifest.read_text()) if manifest.exists() else {}
    contents = {path: git('show', REVISION + ':' + path) for path in sorted(selected)}
    # Delete only files owned by the preceding snapshot manifest.
    for path in previous.keys() - contents.keys():
        target = (destination / path).resolve()
        if not target.is_relative_to(destination.resolve()):
            raise ValueError('Invalid previous snapshot path')
        target.unlink(missing_ok=True)
    for path, data in contents.items():
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    manifest.write_bytes((json.dumps({path: hashlib.sha256(data).hexdigest()
                                     for path, data in contents.items()}, indent=2) + '\n').encode('utf-8'))
    print(f'CyUI {REVISION}: {len(contents)} upstream files; {", ".join(MODULES)}')


if __name__ == '__main__':
    main()
