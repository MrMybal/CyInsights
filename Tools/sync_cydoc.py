"""Create the pinned official CyDoc package from an isolated Git checkout.

This maintainer command needs Git authentication, Node 22+, npm and build-time
network access. Normal application builds consume the vendored package instead.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import gzip
import io

ROOT = Path(__file__).resolve().parents[1]
REVISION = '774f2a7eabf7dd181a98ffc5add115b580b67f42'
REPOSITORY = 'https://github.com/MrMybal/CyDoc.git'
OMITTED = {'package/tools/create-example-docs.py'}


def package_subset(source, destination):
    """Omit the unused upstream demo authoring script, which embeds a local path.

    Every retained file is byte-for-byte upstream. No runtime/compiler patches.
    Normalize archive metadata for repeatable application dependency snapshots.
    """
    with tarfile.open(source) as original, destination.open('wb') as stream:
        with gzip.GzipFile(filename='',fileobj=stream,mode='wb',mtime=0) as compressed:
            with tarfile.open(fileobj=compressed,mode='w') as output:
                for member in sorted(original.getmembers(),key=lambda m:m.name):
                    if member.name in OMITTED:continue
                    if not member.isfile() or not member.name.startswith('package/') or '..' in Path(member.name).parts:
                        raise ValueError('Unexpected upstream package entry')
                    data=original.extractfile(member).read()
                    info=tarfile.TarInfo(member.name);info.size=len(data);info.mode=member.mode;info.mtime=0
                    output.addfile(info,io.BytesIO(data))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout', type=Path, help='An existing isolated, clean checkout at the pinned revision')
    args = parser.parse_args()
    checkout = args.checkout.resolve() if args.checkout else ROOT/'build/cydoc-upstream'
    def git(*argv):
        return subprocess.check_output(['git', '-c', 'safe.directory='+checkout.as_posix(), '-C', str(checkout), *argv])
    if not checkout.exists():
        subprocess.run(['git', 'clone', '--no-checkout', REPOSITORY, str(checkout)], check=True)
        git('checkout', '--detach', REVISION)
    if git('rev-parse', 'HEAD').decode().strip() != REVISION or git('status', '--porcelain', '--untracked-files=no').strip():
        raise SystemExit('Use a clean checkout of the pinned revision; upstream local changes are never overwritten')
    npm = shutil.which('npm.cmd') or shutil.which('npm')
    if not npm:
        raise SystemExit('Node.js 22+ and npm are required')
    cache = ROOT/'build/npm-cache'
    for command in (['ci', '--no-audit', '--no-fund'], ['pack', '--pack-destination', '.']):
        subprocess.run([npm, '--cache', str(cache), *command], cwd=checkout, check=True)
    destination = ROOT/'ThirdParty/CyDoc'
    destination.mkdir(parents=True, exist_ok=True)
    original=checkout/'cydoc-0.1.0.tgz'
    package_subset(original,destination/original.name)
    shutil.copyfile(checkout/'LICENSE',destination/'LICENSE')
    shutil.copyfile(checkout/'LICENSE',ROOT/'LICENCES/CyDoc-MIT.txt')
    shutil.copyfile(checkout/'examples/integrations/cydoc_link.hpp', destination/'cydoc_link.hpp')
    files = {name: hashlib.sha256((destination/name).read_bytes()).hexdigest()
             for name in ('cydoc-0.1.0.tgz', 'LICENSE', 'cydoc_link.hpp')}
    (destination/'snapshot.json').write_text(json.dumps({'repository': REPOSITORY, 'revision': REVISION,
        'version': '0.1.0', 'omittedFiles':sorted(OMITTED),
        'officialPackageSha256':hashlib.sha256(original.read_bytes()).hexdigest(),
        'sha256': files}, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    main()
