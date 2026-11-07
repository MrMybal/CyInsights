"""Validate the allowlisted showcase and its unchanged offline manual."""
import hashlib
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import sys
import unittest
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/site'


class Document(HTMLParser):
    def __init__(self, text):
        super().__init__()
        self.nodes = []
        self.feed(text)

    def handle_starttag(self, tag, attrs):
        self.nodes.append((tag, dict(attrs)))


class SiteTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.html = (OUT / 'index.html').read_text(encoding='utf-8')
        cls.doc = Document(cls.html)
        cls.text = json.loads((ROOT / 'Site/content.json').read_text(encoding='utf-8'))

    def test_full_translations(self):
        self.assertEqual(set(self.text), {'en', 'fr'})
        self.assertEqual(set(self.text['en']), set(self.text['fr']))
        self.assertIn('<html lang="en">', self.html)
        for _, attrs in self.doc.nodes:
            for key in ('data-i18n', 'data-i18n-alt', 'data-i18n-aria', 'data-caption'):
                if key in attrs:
                    for language in self.text.values():
                        self.assertTrue(language[attrs[key]].strip())

    def test_links_and_images(self):
        ids = [attrs['id'] for _, attrs in self.doc.nodes if 'id' in attrs]
        self.assertEqual(len(ids), len(set(ids)))
        for tag, attrs in self.doc.nodes:
            for attribute in ('src', 'href'):
                target = attrs.get(attribute)
                if not target:
                    continue
                url = urlsplit(target)
                if url.scheme:
                    self.assertEqual(url.scheme, 'https')
                    self.assertIn(url.hostname, {'github.com'})
                elif url.path:
                    path = OUT / unquote(url.path)
                    self.assertTrue(path.is_file(), target)
                elif url.fragment:
                    self.assertIn(url.fragment, ids)
            if tag == 'img' and attrs.get('id') != 'lightbox-image':
                self.assertIn('alt', attrs)
                self.assertIn('width', attrs)
                self.assertIn('height', attrs)
        self.assertNotIn('{{', self.html)
        self.assertGreaterEqual(sum('data-lightbox' in attrs for _, attrs in self.doc.nodes), 10)
        self.assertEqual(sum('data-download' in attrs for _, attrs in self.doc.nodes), 7)

    def test_hashed_assets_and_payload(self):
        record = json.loads((OUT / 'site-manifest.json').read_text())
        actual = {p.relative_to(OUT).as_posix() for p in OUT.rglob('*') if p.is_file()}
        self.assertEqual(actual, set(record['files']) | {'site-manifest.json'})
        for name, digest in record['files'].items():
            self.assertEqual(hashlib.sha256((OUT / name).read_bytes()).hexdigest(), digest)
        for path in (OUT / 'site-assets').iterdir():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()[:12]
            self.assertIn('.' + digest + '.', path.name)
        expected = {'.nojekyll', 'index.html', 'site-manifest.json'}
        expected.update(p.relative_to(ROOT / 'Documentation').as_posix()
                        for p in (ROOT / 'Documentation').rglob('*') if p.is_file())
        expected.update('site-assets/' + p.name for p in (OUT / 'site-assets').iterdir())
        self.assertEqual(actual, expected)
        self.assertFalse(any(name.endswith(('.log', '.utrace', '.cycapture', '.pfx', '.keystore')) for name in actual))

    def test_cydoc_byte_identity_and_routes(self):
        manifest = json.loads((ROOT / 'Documentation/DocData/manifest.json').read_text())
        page_ids = {page['id'] for page in manifest['pages']}
        for _, attrs in self.doc.nodes:
            if 'data-doc' in attrs:
                self.assertIn(attrs['data-doc'], page_ids)
                self.assertEqual(attrs['href'], 'Documentation.html#/en/' + attrs['data-doc'])
        for source in (ROOT / 'Documentation').rglob('*'):
            if source.is_file():
                self.assertEqual(source.read_bytes(), (OUT / source.relative_to(ROOT / 'Documentation')).read_bytes())
        self.assertIn('^#\\/(en|fr)\\/', (ROOT / 'Site/site.js').read_text())


if __name__ == '__main__':
    unittest.main()
