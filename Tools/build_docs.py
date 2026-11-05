"""Compile all application help with the pinned CyDoc offline exporter.

Node 22+ is needed only to build. Readers use file:// without a server or network.
"""
import argparse
import hashlib
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import urllib.parse
import unicodedata
from contextlib import contextmanager
import time

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT/'build/cydoc-project'
TOOLING = ROOT/'Tools/Documentation'
# Deliberately narrow: no engine, device data, generated binaries or dependency
# source trees are scanned. These files are already distributed as source.
SOURCES = ['App/AiRpc.hpp', 'AI/app/bridge.py', 'AI/app/tool.py',
           'Quest/CyInsightOVR/src/org/cyinsights/ovr/ShellClient.java',
           'Quest/CyInsightOVR/src/org/cyinsights/ovr/DrawCounterSample.java']
MEDIA_ROOTS = [ROOT/'Docs/Images', ROOT/'Assets/Branding']


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding='utf-8', newline='\n')


def slug(value):
    return re.sub(r'[^a-z0-9]+', '-', value.lower()).strip('-') or 'section'


def metadata(page_id, title, section, order, body, locale='en', revision=None):
    revision = revision or hashlib.sha256(body.encode()).hexdigest()[:12]
    return (f'---\nid: {page_id}\ntitle: {title.replace(chr(10), " ")}\nsection: {section}\n'
            f'order: {order}\n'+('revision' if locale == 'en' else 'sourceRevision')+f': {revision}\n---\n\n'+body)


def documentation_files():
    # Preserve reference material, SDK examples, release notes and dated evidence.
    # Exclude dependency notices (shipped separately) and AI instructions.
    paths = [ROOT/'README.md']
    for directory in ('Docs', 'AI', 'Quest/CyInsightOVR', 'Plugins/CyInsights',
                      'Tools/NativeTiming', 'Tools/NativeTasks', 'Tools/NativeActivities', 'Tools/WindowsCollector'):
        paths += list((ROOT/directory).rglob('*.md'))
    return sorted(set(p for p in paths if p.name != 'AGENTS.md' and
        not any(x in p.parts for x in ('node_modules', 'runtime', 'LICENCES', 'ThirdParty')) and
        not (p.is_relative_to(ROOT/'Docs/Manual') and p.parent.name in ('en','fr'))))


def prepare():
    # Only remove the fixed generated project tree, never authoring inputs.
    if PROJECT.exists():
        if not PROJECT.resolve().is_relative_to(ROOT/'build'):
            raise SystemExit('Generated project path escapes the workspace build directory')
        shutil.rmtree(PROJECT)
    for folder in ('content/en', 'content/fr', 'assets', 'sources'):
        (PROJECT/folder).mkdir(parents=True, exist_ok=True)
    config = json.loads((ROOT/'Docs/CyDoc/cydoc.config.json').read_text(encoding='utf-8'))
    config['version'] = json.loads((ROOT/'VERSION.json').read_text())['version']
    catalog = json.loads((ROOT/'Docs/Manual/catalog.json').read_text(encoding='utf-8'))
    approvals = json.loads((ROOT/'Docs/CyDoc/translations.json').read_text(encoding='utf-8'))['fr']
    inventory = []
    for order, topic in enumerate(catalog['topics']):
        # Canonical newlines keep translation revisions stable across Git's
        # Windows/Linux checkout policies.
        original=(ROOT/f'Docs/Manual/en/{topic["id"]}.md').read_text(encoding='utf-8')
        revision = hashlib.sha256(original.encode('utf-8')).hexdigest()[:12]
        for locale in ('en', 'fr'):
            body = (ROOT/f'Docs/Manual/{locale}/{topic["id"]}.md').read_text(encoding='utf-8').replace('manual:', 'doc:')
            # CyDoc supplies the page title itself. Retain the authoring H1 but
            # avoid displaying it twice in the exported article.
            body = re.sub(r'^# [^\n]+\n?', '', body, count=1).lstrip()
            # Explicit author-owned anchors are shared by the two translations.
            for media_id in topic.get('media', []):
                media = catalog['media'][media_id]
                title, caption = media['title'][locale], media['caption'][locale]
                body += f'\n\n## {title} {{#{media_id}}}\n\n{caption}\n\n'
                if media['kind'] == 'image':
                    source = ROOT/media['source']
                    shutil.copyfile(source, PROJECT/'assets'/source.name)
                    body += '```image\n'+json.dumps({'id': media_id, 'src': 'assets/'+source.name,
                        'alt': title, 'controls': 'minimal'}, ensure_ascii=False)+'\n```\n'
                elif media['kind'] == 'flow':
                    nodes = [{'id': f'n{i}', 'label': n[locale], 'x': (i % 2)*310, 'y': (i//2)*100}
                             for i, n in enumerate(media['nodes'])]
                    edges = [{'from': nodes[i]['id'], 'to': nodes[i+1]['id']} for i in range(len(nodes)-1)]
                    body += '```diagram\n'+json.dumps({'type': 'flow', 'title': title, 'nodes': nodes, 'edges': edges}, ensure_ascii=False)+'\n```\n'
                else:
                    # Accessible static SVG. This is labelled illustrative data.
                    name = f'tile-grid-{locale}.svg'
                    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 600 280" role="img"><title>{html.escape(title)}</title><desc>{html.escape(caption)}</desc>']
                    for i, value in enumerate([60,80,100,110,70,75,140,280,360,100,65,170,480,410,90,55,80,130,150,60]):
                        color = '#74c69d' if value < 150 else '#f1c453' if value < 300 else '#ee7466'
                        x,y=(i%5)*118,(i//5)*60
                        svg.append(f'<rect x="{x}" y="{y}" width="114" height="56" rx="4" fill="{color}"/><text x="{x+57}" y="{y+34}" text-anchor="middle" font-family="sans-serif" font-size="20" fill="#10212e">{value} µs</text>')
                    svg.append('<text x="4" y="273" font-family="sans-serif" font-size="17" fill="#7389a0">EXAMPLE / EXEMPLE — NOT LIVE DATA</text></svg>')
                    write(PROJECT/'assets'/name, ''.join(svg))
                    body += f'![{caption}](assets/{name})\n'
            section = 'CyInsights' if topic['product']=='desktop' else 'CyInsightOVR' if topic['product']=='quest' else ('Shared guides' if locale=='en' else 'Guides communs')
            text = metadata(topic['id'], topic['title'][locale], section, order, body, locale,
                            revision if locale=='en' else approvals.get(topic['id'],'pending'))
            write(PROJECT/f'content/{locale}/{topic["id"]}.md', text)
        inventory.append({'source': f'Docs/Manual/*/{topic["id"]}.md', 'id': topic['id'], 'locales': ['en', 'fr'], 'status': 'guide'})
    refs = documentation_files()
    ids = {p.resolve(): 'reference-'+slug(p.relative_to(ROOT).as_posix().removesuffix('.md')) for p in refs}
    reference_anchors={}
    for path in refs:
        used=set();mapping={};fence=False
        for line in path.read_text(encoding='utf-8-sig').splitlines():
            if line.startswith(('```','~~~')):fence=not fence
            if fence:continue
            heading=re.match(r'^#{1,6} (.+)',line)
            if not heading:continue
            text=heading[1];explicit=re.search(r'\s*\{#([a-zA-Z0-9_-]+)\}\s*$',text)
            plain=re.sub(r'\s*\{#[a-zA-Z0-9_-]+\}\s*$','',text)
            normalized=''.join(c for c in unicodedata.normalize('NFD',plain) if not unicodedata.combining(c))
            anchor=explicit[1] if explicit else slug(normalized)
            stem=anchor;count=2
            while anchor in used:anchor=f'{stem}-{count}';count+=1
            used.add(anchor)
            mapping[anchor]=anchor
            # GitHub's historical fragments keep accented letters.
            github=re.sub(r'[^\w\- ]','',plain.lower()).replace(' ','-')
            mapping.setdefault(github,anchor)
        reference_anchors[path.resolve()]=mapping
    assets = {}
    legacy=[]
    for index, path in enumerate(refs):
        source = path.read_text(encoding='utf-8-sig')
        title = next((s.lstrip('# ').strip() for s in source.splitlines() if s.startswith('# ')), path.stem)
        # Original technical references retain their source language. English is
        # the catalog fallback; this banner prevents pretending they are translated.
        historical = any(x in path.as_posix() for x in ('/History/', '/Release-', '/VALIDATION.md')) or bool(re.search(r'Release-20[0-9]{6}|— 9 octobre 2026',title)) or path.name in ('SecurityReview.md','InterfaceRevision.md','InterfacePerformance.md','TestingRelease.md','Status.md')
        banner = ('Historical record. Dates, test results and feature status describe that revision; consult the current guides for current behavior.' if historical else
                  'Technical reference preserved in its original language. Some sections describe implementation history. Current user workflows are in the English/French guides.')
        sdk_background=path.is_relative_to(ROOT/'AI/app/Docs') or path.name=='CyToolsGuide.md'
        if sdk_background:banner+=' This is upstream CyToolsCore SDK background, not a list of CyInsights features. The operation contract and current CyInsights guides define the enabled capabilities.'
        source = '```callout\n'+json.dumps({'type': 'info', 'title': 'Archive' if historical else 'Reference', 'body': banner})+'\n```\n\n'+source
        def local_link(match):
            label, href = match[1], match[2]
            if re.match(r'^(?:[a-z]+:|#|//)', href, re.I):
                return match[0]
            decoded = urllib.parse.unquote(href.split('#')[0].split('?')[0])
            target = (path.parent/decoded).resolve()
            if target in ids:
                fragment=urllib.parse.unquote(href.split('#',1)[1]) if '#' in href else ''
                section=reference_anchors[target].get(fragment,'')
                if fragment and not section:legacy.append({'source':path.relative_to(ROOT).as_posix(),'href':href,'fallback':ids[target]})
                return f'[{label}](doc:{ids[target]}'+('#'+section if section else '')+')'
            if target.is_file() and any(target.is_relative_to(folder) for folder in MEDIA_ROOTS) and target.suffix.lower() in ('.png','.jpg','.jpeg','.svg','.gif'):
                name = hashlib.sha256(target.relative_to(ROOT).as_posix().encode()).hexdigest()[:12]+target.suffix
                assets[name] = target
                return f'[{label}](assets/{name})'
            # Source/maintenance paths remain visible, never broken file links.
            return f'{label} (`{href}`; repository source)'
        source = re.sub(r'\[([^\]\n]+)\]\(([^)\s]+)\)', local_link, source)
        page_id = ids[path.resolve()]
        write(PROJECT/f'content/en/{page_id}.md', metadata(page_id,title,'History' if historical else 'SDK background' if sdk_background else 'Technical references',100+index,source))
        inventory.append({'source': path.relative_to(ROOT).as_posix(), 'id': page_id, 'locales': ['en'], 'status': 'historical' if historical else 'original-language reference'})
    for name, path in assets.items():
        shutil.copyfile(path, PROJECT/'assets'/name)
    for relative in SOURCES:
        write(PROJECT/'sources'/relative, (ROOT/relative).read_text(encoding='utf-8'))
    # Neutral import is derived from the actual shipped contract, not a guessed
    # C++ parser or generic SDK scaffold. Preserve complete schemas separately.
    operations = json.loads((ROOT/'AI/app/CyTool.json').read_text(encoding='utf-8'))['operations']
    symbols = []
    for op in operations:
        body = op['description']+'\n\nInput schema:\n```json\n'+json.dumps(op['inputSchema'],indent=2)+'\n```\n\nOutput schema:\n```json\n'+json.dumps(op['outputSchema'],indent=2)+'\n```\n\nPermissions and platforms:\n```json\n'+json.dumps({k:v for k,v in op.items() if k not in ('inputSchema','outputSchema','description')},indent=2)+'\n```'
        symbols.append({'key': op['id'], 'name': op['id'], 'language':'python', 'kind':'operation',
            'signature': 'op_'+op['id']+'(parameters={...})', 'description': body})
    write(PROJECT/'operations.json', json.dumps({'symbols':symbols},indent=2))
    # A real annotation is extracted from the staged, application-owned source.
    write(PROJECT/'sources/HelpContract.hpp', (ROOT/'Common/Documentation/HelpContract.hpp').read_text(encoding='utf-8'))
    write(PROJECT/'assets/favicon.svg', '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><rect width="64" height="64" rx="16" fill="#087d9b"/><text x="8" y="44" font-size="38" font-family="sans-serif" fill="white">Cy</text></svg>')
    write(PROJECT/'cydoc.config.json', json.dumps(config,ensure_ascii=False,indent=2))
    write(PROJECT/'inventory.json', json.dumps(inventory,ensure_ascii=False,indent=2))
    write(PROJECT/'legacy-links.json',json.dumps(legacy,ensure_ascii=False,indent=2))


def build(output):
    snapshot = json.loads((ROOT/'ThirdParty/CyDoc/snapshot.json').read_text())
    for name, expected in snapshot['sha256'].items():
        if hashlib.sha256((ROOT/'ThirdParty/CyDoc'/name).read_bytes()).hexdigest()!=expected:
            raise SystemExit('CyDoc package verification failed: '+name)
    node = shutil.which('node')
    if not node or int(subprocess.check_output([node,'--version']).decode().strip().lstrip('v').split('.')[0])<22:
        raise SystemExit('Documentation generation requires Node.js 22+; the installed reader does not require Node')
    cli = TOOLING/'node_modules/cydoc/tools/cli.mjs'
    if not cli.exists():
        npm = shutil.which('npm.cmd') or shutil.which('npm')
        subprocess.run([npm,'ci','--omit=peer','--ignore-scripts','--no-audit','--no-fund',
                        '--cache',str(ROOT/'build/npm-cache')], cwd=TOOLING, check=True)
    prepare()
    subprocess.run([node,str(cli),'check',str(PROJECT/'cydoc.config.json')],cwd=ROOT,check=True)
    export = PROJECT/'export'
    # App-owned host shell. CyDoc compiler/data/module scripts are unmodified.
    # This tiny classic script sets a dark default only on first use and adapts
    # pre-CyDoc manual links before the official bootstrap mounts the reader.
    write(export/'host.js', """(()=>{
try{if(!localStorage.getItem('cydoc:theme'))localStorage.setItem('cydoc:theme','nord')}catch{}
if(location.hash.startsWith('#manual:'))location.replace('#/en/'+encodeURIComponent(location.hash.slice(8)));
const params=new URLSearchParams(location.hash.split('?')[1]||'');
if(params.get('preferred')==='auto'){params.delete('preferred');let locale;try{locale=localStorage.getItem('cyinsights-help-locale')}catch{};const parts=location.hash.split('?')[0].split('/');if(locale==='fr'||locale==='en')parts[1]=locale;history.replaceState(null,'',parts.join('/')+(params.size?'?'+params:''))}
function remember(){const locale=location.hash.split('/')[1];if(locale==='en'||locale==='fr')try{localStorage.setItem('cyinsights-help-locale',locale)}catch{}}
addEventListener('hashchange',remember);remember();
// Upstream's generic MIT footer describes its reader. Make the application's
// separate license explicit, without changing any CyDoc compiler/viewer code.
const label=()=>{const footer=document.querySelector('.footer>div>span:last-child');const text='CyDoc: MIT · CyInsights: AGPL-3.0-or-later';if(footer&&footer.textContent!==text)footer.textContent=text;const tag=document.querySelector('.mit-tag');if(tag&&tag.textContent!=='CyDoc MIT')tag.textContent='CyDoc MIT'};
new MutationObserver(label).observe(document.documentElement,{childList:true,subtree:true});label();
})();""")
    write(export/'index.html', '''<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="color-scheme" content="dark light"><meta http-equiv="Content-Security-Policy" content="default-src 'self' file: data: blob:; script-src 'self' file:; style-src 'self' 'unsafe-inline'; img-src 'self' file: data: blob:; connect-src 'none'; object-src 'none'; base-uri 'none'">
<link rel="icon" href="assets/favicon.svg"><link rel="stylesheet" href="viewer.css"><title>CyInsights documentation</title>
<script src="host.js" defer></script><script src="data.js" defer></script><script src="viewer.js" defer></script></head>
<body><div id="app"></div><noscript>Enable JavaScript to read this offline manual. / Activez JavaScript pour lire ce manuel local.</noscript></body></html>''')
    notices = export/'licenses'
    notices.mkdir(exist_ok=True)
    for package in ('cydoc','marked','prismjs','esbuild'):
        root = TOOLING/'node_modules'/package
        license = next((p for p in root.iterdir() if p.name.lower() in ('license','license.md','license.txt')), None)
        if not license:
            raise SystemExit('Missing documentation dependency notice: '+package)
        shutil.copyfile(license,notices/(package+'.txt'))
    shutil.copyfile(PROJECT/'inventory.json',export/'inventory.json')
    shutil.copyfile(PROJECT/'legacy-links.json',export/'legacy-links.json')
    files = {p.relative_to(export).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in export.rglob('*') if p.is_file()}
    write(export/'export-sha256.json',json.dumps({'cydoc_revision':snapshot['revision'],'modules':['image'],'files':files},indent=2))
    copy_export(export, output)
    print(json.dumps({'help':output.resolve().relative_to(ROOT).as_posix(),'indexed_sources':len(SOURCES)+1,'modules':['image']}))


def copy_export(export, output):
    output = output.resolve()
    if output != export and (output.name != 'help' or not any(output.is_relative_to(ROOT/d) for d in ('build','bin'))):
        raise SystemExit('Documentation output must be named help under workspace build/ or bin/')
    if output != export:
        manifest=json.loads((export/'export-sha256.json').read_text())
        actual={p.relative_to(export).as_posix() for p in export.rglob('*') if p.is_file()}
        if actual != set(manifest['files'])|{'export-sha256.json'}:
            raise SystemExit('Unexpected or missing file in documentation export')
        for name,expected in manifest['files'].items():
            path=(export/name).resolve()
            if not path.is_relative_to(export.resolve()) or hashlib.sha256(path.read_bytes()).hexdigest()!=expected:
                raise SystemExit('Documentation export failed verification: '+name)
        if output.exists():
            shutil.rmtree(output)
        shutil.copytree(export,output)


@contextmanager
def build_lock():
    # Windows and Quest builds may run together. Their shared authoring stage
    # must not be removed by another exporter while it is compiling.
    lock=ROOT/'build/cydoc-build.lock';lock.parent.mkdir(exist_ok=True)
    with lock.open('a+b') as handle:
        handle.seek(0);handle.write(b'0');handle.flush();deadline=time.monotonic()+120
        while True:
            try:
                handle.seek(0)
                if os.name=='nt':
                    import msvcrt
                    msvcrt.locking(handle.fileno(),msvcrt.LK_NBLCK,1)
                else:
                    import fcntl
                    fcntl.flock(handle.fileno(),fcntl.LOCK_EX|fcntl.LOCK_NB)
                break
            except OSError:
                if time.monotonic()>deadline:raise SystemExit('Another documentation build is still running')
                time.sleep(.1)
        try:yield
        finally:
            handle.seek(0)
            if os.name=='nt':msvcrt.locking(handle.fileno(),msvcrt.LK_UNLCK,1)
            else:fcntl.flock(handle.fileno(),fcntl.LOCK_UN)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/help')
    parser.add_argument('--copy-from',type=Path,help='Copy an already checked export; no Node invocation')
    args=parser.parse_args()
    with build_lock():
        if args.copy_from:
            export=args.copy_from.resolve()
            if export!=ROOT/'build/help':raise SystemExit('Copy source must be the checked workspace build/help export')
            copy_export(export,args.output)
        else:build(args.output)
