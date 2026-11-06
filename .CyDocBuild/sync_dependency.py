"""Refresh the official CyDoc package from the pinned, isolated Git checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REVISION = '6ddf9a582defb2f11fbb3f992afef6f98dcadc93'
REPOSITORY = 'https://github.com/MrMybal/CyDoc.git'

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--checkout',type=Path)
    args=parser.parse_args()
    checkout=args.checkout.resolve() if args.checkout else ROOT/'.CyDocBuild/work/upstream'
    def git(*argv):
        return subprocess.check_output(['git','-c','safe.directory='+checkout.as_posix(),'-C',str(checkout),*argv])
    if not checkout.exists():
        subprocess.run(['git','clone','--no-checkout',REPOSITORY,str(checkout)],check=True)
        git('checkout','--detach',REVISION)
    if git('rev-parse','HEAD').decode().strip()!=REVISION or git('status','--porcelain','--untracked-files=no').strip():
        raise SystemExit('Use a clean isolated checkout at the pinned CyDoc revision')
    npm=shutil.which('npm.cmd') or shutil.which('npm')
    if not npm:raise SystemExit('Node.js 22+ and npm are required')
    cache=ROOT/'.CyDocBuild/work/npm-cache'
    for command in (['ci','--no-audit','--no-fund'],['pack','--pack-destination','.']):
        subprocess.run([npm,'--cache',str(cache),*command],cwd=checkout,check=True)
    version=json.loads((checkout/'package.json').read_text(encoding='utf-8'))['version']
    package='cydoc-'+version+'.tgz'
    destination=ROOT/'ThirdParty/CyDoc'
    destination.mkdir(parents=True,exist_ok=True)
    for source,name in [(checkout/package,package),(checkout/'LICENSE','LICENSE'),
                        (checkout/'examples/integrations/cydoc_link.hpp','cydoc_link.hpp')]:
        shutil.copyfile(source,destination/name)
    shutil.copyfile(checkout/'LICENSE',ROOT/'LICENCES/CyDoc-MIT.txt')
    files={name:hashlib.sha256((destination/name).read_bytes()).hexdigest()
           for name in (package,'LICENSE','cydoc_link.hpp')}
    (destination/'snapshot.json').write_text(json.dumps({'repository':REPOSITORY,'revision':REVISION,
        'version':version,'distribution':'official npm pack archive, unmodified','sha256':files},indent=2)+'\n',encoding='utf-8',newline='\n')

if __name__=='__main__':main()
