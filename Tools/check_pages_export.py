"""Verify the exact CyDoc directory passed to upload-pages-artifact.

Only documentation output is deployable. Application runtimes, records, private
configuration, caches and raw repository trees do not belong in this directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
from check_publication import content_issues

TOP_LEVEL = {'index.html','host.js','data.js','viewer.js','viewer.css',
             'media-image.js','manifest.json','translation-report.json',
             'inventory.json','legacy-links.json','export-sha256.json'}
LICENSES = {'cydoc.txt','marked.txt','prismjs.txt','esbuild.txt'}
IMAGE_TYPES = {'.png','.jpg','.jpeg','.svg','.gif'}


def verify(root):
    root=root.resolve()
    record=json.loads((root/'export-sha256.json').read_text(encoding='utf-8'))
    if record['modules']!=['image']:
        raise ValueError('Review the deployment allowlist before adding a reader module')
    actual={p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
    if actual!=set(record['files'])|{'export-sha256.json'}:
        raise ValueError('Unexpected or missing export file')
    for name in sorted(actual):
        path=root/name;relative=Path(name)
        if not path.resolve().is_relative_to(root) or path.is_symlink():
            raise ValueError('Export paths must stay inside the reader directory')
        permitted=(len(relative.parts)==1 and name in TOP_LEVEL or
                   len(relative.parts)==2 and relative.parts[0]=='assets' and relative.suffix in IMAGE_TYPES or
                   len(relative.parts)==2 and relative.parts[0]=='licenses' and relative.name in LICENSES)
        if not permitted:
            raise ValueError('Not a documentation resource: '+name)
        data=path.read_bytes()
        if name!='export-sha256.json' and hashlib.sha256(data).hexdigest()!=record['files'][name]:
            raise ValueError('Export checksum differs: '+name)
        if content_issues(name,data):
            raise ValueError('Export content requires review: '+name)
    if not TOP_LEVEL.issubset(actual):
        raise ValueError('Incomplete reader')
    print(json.dumps({'documentation_files':len(actual),'modules':record['modules'],'ready':True}))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('export',type=Path)
    verify(parser.parse_args().export)
