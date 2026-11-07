"""Build the static CyInsights showcase beside the unchanged offline CyDoc reader."""
import argparse
import hashlib
import html
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / 'Site'


def checksum(data):
    return hashlib.sha256(data).hexdigest()


def build(output, skip_docs=False):
    output = output.resolve()
    if not output.is_relative_to(ROOT / 'build') or output == ROOT / 'build':
        raise ValueError('Choose a dedicated output inside build/')
    if output.exists():
        marker = output / 'site-manifest.json'
        if not marker.is_file():
            raise ValueError('Refusing to replace an unrecognized directory')
        previous = json.loads(marker.read_text(encoding='utf-8'))
        actual = {p.relative_to(output).as_posix() for p in output.rglob('*') if p.is_file()}
        if actual != set(previous['files']) | {'site-manifest.json'}:
            raise ValueError('Output contains additional files; preserve them before rebuilding')
        if any(checksum((output / name).read_bytes()) != digest for name, digest in previous['files'].items()):
            raise ValueError('Output has edited files; do not discard them')
        shutil.rmtree(output)
    output.mkdir(parents=True)
    if not skip_docs:
        subprocess.run([sys.executable, str(ROOT / 'Tools/build_docs.py')], cwd=ROOT, check=True)
    spec = importlib.util.spec_from_file_location('cydoc_gate', ROOT / '.CyDocBuild/check_export.py')
    gate = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gate)
    gate.verify(ROOT / 'Documentation')
    shutil.copytree(ROOT / 'Documentation', output, dirs_exist_ok=True)

    translations = json.loads((SITE / 'content.json').read_text(encoding='utf-8'))
    if set(translations) != {'en', 'fr'} or set(translations['en']) != set(translations['fr']):
        raise ValueError('English and French translation keys must match')
    if any(not value.strip() for locale in translations.values() for value in locale.values()):
        raise ValueError('Empty translation')
    version = json.loads((ROOT / 'VERSION.json').read_text(encoding='utf-8'))['version']
    tag = 'v' + version
    release = 'https://github.com/MrMybal/CyInsights/releases/tag/' + tag
    downloads = 'https://github.com/MrMybal/CyInsights/releases/download/' + tag + '/CyInsights-' + tag + '-'
    assets = output / 'site-assets'
    assets.mkdir()
    images = json.loads((SITE / 'assets/images.json').read_text(encoding='utf-8'))
    image_paths = {}
    for key, record in images.items():
        source = SITE / 'assets/images' / record['file']
        data = source.read_bytes()
        name = source.stem + '.' + checksum(data)[:12] + source.suffix
        (assets / name).write_bytes(data)
        image_paths[key] = 'site-assets/' + name
    css = (SITE / 'site.css').read_text(encoding='utf-8').encode('utf-8')
    js = (SITE / 'site.js').read_text(encoding='utf-8').replace(
        '/*__TRANSLATIONS__*/ {}', json.dumps(translations, ensure_ascii=False, separators=(',', ':'))).encode('utf-8')
    css_name = 'site.' + checksum(css)[:12] + '.css'
    js_name = 'site.' + checksum(js)[:12] + '.js'
    (assets / css_name).write_bytes(css)
    (assets / js_name).write_bytes(js)

    def token(match):
        key = match.group(1)
        if key.startswith('TEXT:'):
            return html.escape(translations['en'][key[5:]], quote=True)
        if key.startswith('IMAGE:'):
            return image_paths[key[6:]]
        if key.startswith('DOWNLOAD:'):
            return downloads + key[9:]
        return {'VERSION': version, 'STYLE': 'site-assets/' + css_name,
                'SCRIPT': 'site-assets/' + js_name, 'RELEASE': release}[key]

    template = (SITE / 'index.html').read_text(encoding='utf-8')
    rendered = re.sub(r'\{\{([^{}]+)\}\}', token, template)
    (output / 'index.html').write_text(rendered, encoding='utf-8', newline='\n')
    (output / '.nojekyll').write_text('', encoding='utf-8')
    files = {p.relative_to(output).as_posix(): checksum(p.read_bytes())
             for p in sorted(output.rglob('*')) if p.is_file()}
    manifest = {'schema': 1, 'version': version, 'locales': ['en', 'fr'],
                'documentation': 'Documentation.html', 'files': files}
    (output / 'site-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'site': str(output), 'files': len(files) + 1, 'images': len(images), 'version': version}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/site')
    parser.add_argument('--skip-docs', action='store_true', help='Reuse an already generated, checked Documentation export')
    args = parser.parse_args()
    build(args.output, args.skip_docs)
