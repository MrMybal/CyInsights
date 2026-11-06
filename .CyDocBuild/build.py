"""Build the application's local, bilingual CyDoc manual from reviewed inputs."""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import time

DOC=Path(__file__).resolve().parent
ROOT=DOC.parent
WORK=DOC/'work'
STAGE=WORK/'Documentation'
MANIFEST='DocData/export-sha256.json'


@contextmanager
def lock():
    WORK.mkdir(parents=True,exist_ok=True)
    with (WORK/'build.lock').open('a+b') as stream:
        if os.name=='nt':
            import msvcrt
            stream.seek(0);stream.write(b'0');stream.flush()
            while True:
                try:stream.seek(0);msvcrt.locking(stream.fileno(),msvcrt.LK_NBLCK,1);break
                except OSError:time.sleep(.1)
        else:
            import fcntl
            fcntl.flock(stream,fcntl.LOCK_EX)
        try:yield
        finally:
            if os.name=='nt':stream.seek(0);msvcrt.locking(stream.fileno(),msvcrt.LK_UNLCK,1)
            else:fcntl.flock(stream,fcntl.LOCK_UN)


def verify(folder):
    folder=folder.resolve()
    record=json.loads((folder/MANIFEST).read_text(encoding='utf-8'))
    names={p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
    if names!=set(record['files'])|{MANIFEST}:raise ValueError('Unexpected or missing documentation resource')
    if record['modules']!=['image']:raise ValueError('Review media dependencies before changing selected modules')
    for name,expected in record['files'].items():
        path=folder/name
        if not path.resolve().is_relative_to(folder) or path.is_symlink():raise ValueError('Documentation path escapes export')
        if hashlib.sha256(path.read_bytes()).hexdigest()!=expected:raise ValueError('Documentation hash differs: '+name)
    return record


def destination(path):
    path=path.resolve()
    if path.name!='Documentation' or not path.is_relative_to(ROOT) or path==ROOT:
        raise ValueError('Output must be a Documentation directory inside the application workspace')
    return path


def clean_generated(path):
    path=destination(path)
    if path.exists():
        if not any(path.iterdir()):path.rmdir()
        else:
            verify(path) # Never remove unrelated or unrecognized user files.
            shutil.rmtree(path)


def copy(source,target):
    source=source.resolve();target=destination(target)
    verify(source)
    if source==target:return
    clean_generated(target)
    shutil.copytree(source,target)
    verify(target)


def tooling():
    snapshot=json.loads((ROOT/'ThirdParty/CyDoc/snapshot.json').read_text(encoding='utf-8'))
    for name,sha in snapshot['sha256'].items():
        if hashlib.sha256((ROOT/'ThirdParty/CyDoc'/name).read_bytes()).hexdigest()!=sha:
            raise ValueError('Pinned CyDoc dependency changed: '+name)
    node=shutil.which('node')
    if not node or int(subprocess.check_output([node,'--version']).decode().strip().lstrip('v').split('.')[0])<22:
        raise ValueError('Documentation generation requires Node.js 22+; the reader needs no Node runtime')
    cli=DOC/'node_modules/cydoc/tools/cli.mjs'
    if not cli.is_file():
        npm=shutil.which('npm.cmd') or shutil.which('npm')
        subprocess.run([npm,'ci','--omit=peer','--ignore-scripts','--no-audit','--no-fund','--cache',str(WORK/'npm-cache')],cwd=DOC,check=True)
    # The version number alone is insufficient: upstream snapshots share 0.1.0.
    with tarfile.open(ROOT/'ThirdParty/CyDoc'/('cydoc-'+snapshot['version']+'.tgz')) as archive:
        for name in ['tools/build.mjs','dist/viewer.js','src/viewer.css','tools/api-import.mjs']:
            expected=archive.extractfile('package/'+name).read()
            if (DOC/'node_modules/cydoc'/name).read_bytes()!=expected:
                raise ValueError('Installed CyDoc differs from the pinned archive; run npm ci in .CyDocBuild')
    return node,cli


def build(output):
    node,cli=tooling()
    subprocess.run([node,str(DOC/'prepare_api.mjs')],cwd=DOC,check=True)
    config=json.loads((DOC/'cydoc.config.json').read_text(encoding='utf-8'))
    config['version']=json.loads((ROOT/'VERSION.json').read_text(encoding='utf-8'))['version']
    config['out']='work/Documentation'
    generated=DOC/'generated.config.json'
    generated.write_text(json.dumps(config,ensure_ascii=False,indent=2),encoding='utf-8')
    clean_generated(STAGE)
    # The official check command also creates the export.
    subprocess.run([node,str(cli),'check',str(generated)],cwd=ROOT,check=True)
    data=STAGE/'DocData'
    shutil.copyfile(DOC/'host.js',data/'host.js')
    index=STAGE/'Documentation.html'
    text=index.read_text(encoding='utf-8')
    csp="default-src 'self' file: data: blob:; script-src 'self' file:; style-src 'self' 'unsafe-inline'; img-src 'self' file: data: blob:; connect-src 'none'; object-src 'none'; base-uri 'none'"
    text=text.replace('<meta charset="utf-8">','<meta charset="utf-8">\n  <meta http-equiv="Content-Security-Policy" content="'+csp+'">')
    text=text.replace('<script src="DocData/data.js"','<script src="DocData/host.js" defer></script>\n  <script src="DocData/data.js"')
    index.write_text(text,encoding='utf-8',newline='\n')
    # Source-driven assistant sidecar, deliberately outside human content/search.
    ai=STAGE/'DocAI.md'
    ai.write_text(ai.read_text(encoding='utf-8')+'\n\n'+(DOC/'DocAI.md').read_text(encoding='utf-8'),encoding='utf-8',newline='\n')
    for package in ['cydoc','marked','prismjs','esbuild']:
        base=DOC/'node_modules'/package
        license=next(p for p in [base/'LICENSE',base/'LICENSE.md',base/'LICENSE.txt'] if p.is_file())
        (data/'licenses').mkdir(exist_ok=True)
        shutil.copyfile(license,data/'licenses'/(package+'.txt'))
    files={p.relative_to(STAGE).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
           for p in sorted(STAGE.rglob('*')) if p.is_file()}
    (STAGE/MANIFEST).write_text(json.dumps({'modules':config['modules'],'files':files},indent=2)+'\n',encoding='utf-8')
    verify(STAGE)
    copy(STAGE,output)


def pages(source,output):
    verify(source)
    output=output.resolve()
    if output!=WORK/'pages':raise ValueError('Pages staging is restricted to .CyDocBuild/work/pages')
    if output.exists():
        marker=WORK/'pages.managed'
        if not marker.is_file() or marker.read_text()!='generated':raise ValueError('Unrecognized Pages output')
        shutil.rmtree(output)
    shutil.copytree(source,output)
    # Keep former repository-root README links, including their locale and anchor.
    (output/'index.html').write_text('<!doctype html><html lang="en"><head><meta charset="utf-8"><title>CyInsights documentation</title><script src="DocData/entry-redirect.js" defer></script></head><body><a href="Documentation.html">Documentation</a></body></html>\n',encoding='utf-8')
    (output/'DocData/entry-redirect.js').write_text("location.replace('Documentation.html'+location.search+location.hash);\n",encoding='utf-8')
    (WORK/'pages.managed').write_text('generated')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'Documentation')
    parser.add_argument('--copy-from',type=Path)
    parser.add_argument('--pages',action='store_true')
    args=parser.parse_args()
    with lock():
        output=destination(args.output)
        if args.copy_from:copy(args.copy_from,output)
        else:build(output)
        if args.pages:pages(output,WORK/'pages')
    print(json.dumps({'documentation':str(output),'modules':['image'],'pages_staged':args.pages}))

if __name__=='__main__':main()
