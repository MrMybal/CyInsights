"""Structural checks on the actual exported manual, not a replacement reader."""
import hashlib
import json
from pathlib import Path
import re
import sys
import unittest
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]
HELP=ROOT/'build/help'


class DocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((HELP/'manifest.json').read_text(encoding='utf-8'))
        cls.pages={(p['id'],p['locale']):p for p in cls.data['pages']}
        cls.catalog=json.loads((ROOT/'Docs/Manual/catalog.json').read_text(encoding='utf-8'))

    def test_guides_and_shared_anchors(self):
        self.assertEqual(self.data['defaultLocale'],'en')
        for topic in self.catalog['topics']:
            en=self.pages[topic['id'],'en'];fr=self.pages[topic['id'],'fr']
            self.assertEqual([h['id'] for h in en['headings']],[h['id'] for h in fr['headings']],topic['id'])
            for media in topic.get('media',[]):
                self.assertIn('id="'+media+'"',en['html'])
                self.assertIn('id="'+media+'"',fr['html'])

    def test_all_application_references_preserved(self):
        sys.path.insert(0,str(ROOT/'Tools'))
        import build_docs
        inventory=json.loads((HELP/'inventory.json').read_text(encoding='utf-8'))
        originals={e['source'] for e in inventory}
        for source in build_docs.documentation_files():
            self.assertIn(source.relative_to(ROOT).as_posix(),originals)
        self.assertEqual(len(inventory),len({i['id'] for i in inventory}))

    def test_contract_matches_operations(self):
        ops=json.loads((ROOT/'AI/app/CyTool.json').read_text(encoding='utf-8'))['operations']
        for op in ops:
            page=self.pages[f'operation-{op["id"].replace("_","-")}-{op["id"].replace("_","-")}','en']
            self.assertIn('op_'+op['id'],page['text'])
            self.assertIn('outputSchema' if 'outputSchema' in page['text'] else 'Output schema',page['text'])
        self.assertEqual(len([p for p in self.data['pages'] if p['id'].startswith('operation-')]),len(ops))

    def test_routes_and_sections(self):
        for page in self.data['pages']:
            for href in page.get('links',[]):
                match=re.match(r'^#/([^/]+)/([^/?]+)(?:/([^?]+))?',href)
                if not match:continue
                locale,target,anchor=map(lambda s:unquote(s) if s else s,match.groups())
                dest=self.pages.get((target,locale)) or self.pages.get((target,'en'))
                if dest is None and target in self.data.get('aliases',{}):continue
                self.assertIsNotNone(dest,(page['id'],href))
                if anchor:self.assertIn(anchor,{h['id'] for h in dest['headings']}|set(dest.get('anchors',[])),href)

    def test_translations_honest(self):
        report=json.loads((HELP/'translation-report.json').read_text(encoding='utf-8'))
        self.assertEqual(report['outdated'],[])
        missing={p['id'] for p in report['missing']}
        self.assertFalse(missing&{t['id'] for t in self.catalog['topics']})
        # Existing English-only technical contracts are reported, not fake translated.
        self.assertIn('operation-get-state-get-state',missing)

    def test_export_is_local_and_complete(self):
        hashes=json.loads((HELP/'export-sha256.json').read_text())
        self.assertEqual(hashes['modules'],['image'])
        self.assertEqual({p.name for p in HELP.glob('media-*.js')},{'media-image.js'})
        for name,expected in hashes['files'].items():
            path=(HELP/name).resolve()
            self.assertTrue(path.is_relative_to(HELP.resolve()))
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),expected,name)
        index=(HELP/'index.html').read_text()
        self.assertIn("connect-src 'none'",index)
        self.assertNotRegex(index,r'(?:src|href)="https?://')
        self.assertIn('host.js',index)
        for name in ('cydoc','esbuild','marked','prismjs'):
            self.assertTrue((HELP/f'licenses/{name}.txt').is_file())
        lock=json.loads((ROOT/'Tools/Documentation/package-lock.json').read_text())
        self.assertFalse(any('/three' in p for p in lock['packages']))

    def test_export_publication_content(self):
        sys.path.insert(0,str(ROOT/'Tools'))
        from check_publication import content_issues
        for name in ('data.js','manifest.json','inventory.json'):
            self.assertEqual(content_issues(name,(HELP/name).read_bytes()),[],name)

    def test_only_allowlisted_sources(self):
        names={p['source'].rsplit(':',1)[0] for p in self.data['pages'] if p.get('kind')=='source'}
        self.assertEqual(len(names),6)
        self.assertFalse(any('/Engine/' in n or '.local/' in n or 'ThirdParty/' in n for n in names))


if __name__=='__main__':unittest.main(verbosity=2)
