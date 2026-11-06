"""Restrict a documentation/Pages artifact to the checked CyDoc distribution."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from check_publication import content_issues

ENTRY={'Documentation.html','DocAI.md'}
DATA={'data.js','viewer.js','viewer.css','host.js','media-image.js','manifest.json',
      'translation-report.json','export-sha256.json'}
LICENSES={'cydoc.txt','marked.txt','prismjs.txt','esbuild.txt'}

def verify(folder):
    folder=folder.resolve()
    record=json.loads((folder/'DocData/export-sha256.json').read_text(encoding='utf-8'))
    actual={p.relative_to(folder).as_posix() for p in folder.rglob('*') if p.is_file()}
    extra={'index.html','DocData/entry-redirect.js'} if (folder/'index.html').is_file() else set()
    if record['modules']!=['image'] or actual!=set(record['files'])|{'DocData/export-sha256.json'}|extra:
        raise ValueError('Unexpected deployment resource or module')
    for name in actual:
        path=folder/name;p=Path(name)
        allowed=(name in ENTRY|extra or len(p.parts)==2 and p.parts[0]=='DocData' and p.name in DATA or
                 len(p.parts)==3 and p.parts[:2]==('DocData','assets') and p.suffix.lower() in {'.png','.svg','.jpg','.jpeg','.webp'} or
                 len(p.parts)==3 and p.parts[:2]==('DocData','licenses') and p.name in LICENSES)
        if not allowed or not path.resolve().is_relative_to(folder) or path.is_symlink():raise ValueError('Not a reader resource: '+name)
        data=path.read_bytes()
        if name in record['files'] and hashlib.sha256(data).hexdigest()!=record['files'][name]:raise ValueError('Deployment hash differs: '+name)
        if content_issues(name,data):raise ValueError('Documentation content requires review: '+name)
    print(json.dumps({'documentation_files':len(actual),'modules':record['modules'],'ready':True}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('export',type=Path)
    verify(parser.parse_args().export)
