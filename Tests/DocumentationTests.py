"""Checks against the real CyDoc export and application-owned authoring inputs."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
import unittest
from urllib.parse import unquote

ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/'.CyDocBuild'
HELP=ROOT/'Documentation'

class DocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data=json.loads((HELP/'DocData/manifest.json').read_text(encoding='utf-8'))
        cls.pages={(p['id'],p['locale']):p for p in cls.data['pages']}

    def test_complete_languages_and_shared_anchors(self):
        self.assertEqual(self.data['defaultLocale'],'en')
        self.assertEqual({x['code'] for x in self.data['locales']},{'en','fr'})
        ids={p['id'] for p in self.data['pages']}
        for ident in ids:
            en=self.pages[ident,'en'];fr=self.pages[ident,'fr']
            self.assertTrue(en['text'].strip());self.assertTrue(fr['text'].strip())
            self.assertNotEqual(en['text'],fr['text'],ident)
            self.assertEqual([h['id'] for h in en['headings']],[h['id'] for h in fr['headings']],ident)
            self.assertTrue(en['description']);self.assertTrue(fr['description'])
            self.assertEqual(en.get('parent'),fr.get('parent'),ident)
        report=json.loads((HELP/'DocData/translation-report.json').read_text(encoding='utf-8'))
        self.assertEqual(report['missing'],[]);self.assertEqual(report['outdated'],[]);self.assertEqual(report['untracked'],[])
        self.assertEqual(report['current'],len(ids))

    def test_english_revision_is_reviewed(self):
        for p in (DOC/'manual/en').glob('*.md'):
            text=p.read_text(encoding='utf-8');body=text.split('---',2)[2].strip()+'\n'
            sha=hashlib.sha256(body.encode()).hexdigest()[:12]
            self.assertRegex(text,r'(?m)^revision: '+sha+'$',p.name)

    def test_tree_and_no_internal_material(self):
        forbidden=('reference-','source-','translation-report','help-contract')
        for page in self.data['pages']:
            self.assertFalse(page['id'].startswith(forbidden),page['id'])
            self.assertNotEqual(page.get('kind'),'source')
            self.assertNotIn('source',page);self.assertNotIn('sourcePageId',page)
            if page.get('parent'):self.assertIn((page['parent'],page['locale']),self.pages)
        self.assertEqual(self.pages['frames','en']['parent'],'timeline')
        self.assertEqual(self.pages['timeline','en']['parent'],'desktop')
        self.assertEqual(self.pages['operation-query-gpu-query-gpu','fr']['parent'],'api-gpu')
        text='\n'.join(p['text'] for p in self.data['pages'])
        for private in ['CyDoc migration validation','cyinsights-help-build','class HelpReaderProbe','context.cancellation.check()','gitleaks-review.json','Documentation integration notes for coding assistants']:
            self.assertNotIn(private,text)
        self.assertTrue((HELP/'DocAI.md').is_file())
        self.assertIn('get_analysis_guide',(HELP/'DocAI.md').read_text(encoding='utf-8'))

    def test_operation_contract_and_french(self):
        ops=json.loads((ROOT/'AI/app/CyTool.json').read_text(encoding='utf-8'))['operations']
        translations=json.loads((DOC/'api-fr.json').read_text(encoding='utf-8'))['operations']
        self.assertEqual(set(translations),{op['id'] for op in ops})
        for op in ops:
            ident='operation-'+op['id'].replace('_','-')+'-'+op['id'].replace('_','-')
            for locale in ['en','fr']:
                page=self.pages[ident,locale]
                self.assertIn('runtime.submit',page['text'])
                self.assertIn('outputs.reportFile',page['text'])
                self.assertIn('operation-input',{h['id'] for h in page['headings']})
            self.assertIn(translations[op['id']],self.pages[ident,'fr']['text'])
        self.assertEqual(len([p for p in self.data['pages'] if p['id'].startswith('operation-')]),2*len(ops))

    def test_links_aliases_and_sections(self):
        for p in self.data['pages']:
            for href in p.get('links',[]):
                match=re.match(r'^#/([^/]+)/([^/?]+)(?:/([^?]+))?',href)
                if not match:continue
                locale,target,anchor=[unquote(s) if s else s for s in match.groups()]
                alias=self.data.get('aliases',{}).get(target)
                if alias:target,_,anchor=alias.partition('#')
                dest=self.pages.get((target,locale))
                self.assertIsNotNone(dest,(p['id'],href))
                if anchor:self.assertIn(anchor,{h['id'] for h in dest['headings']}|set(dest.get('anchors',[])),href)
        self.assertEqual(self.data['aliases']['overview'],'welcome')
        self.assertIn('reference-docs-nativeactivities',self.data['aliases'])

    def test_distribution_is_only_reader_resources(self):
        self.assertEqual({p.name for p in HELP.iterdir()},{'Documentation.html','DocAI.md','DocData'})
        record=json.loads((HELP/'DocData/export-sha256.json').read_text(encoding='utf-8'))
        self.assertEqual(record['modules'],['image'])
        self.assertEqual({p.name for p in (HELP/'DocData').glob('media-*.js')},{'media-image.js'})
        for name,expected in record['files'].items():
            p=(HELP/name).resolve();self.assertTrue(p.is_relative_to(HELP.resolve()))
            self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),expected,name)
            self.assertFalse(any(part in p.relative_to(HELP).parts for part in ['.CyDocBuild','internal','sources','node_modules']))
        self.assertFalse(any(p.suffix in {'.cpp','.hpp','.py','.java','.ts','.tgz'} for p in HELP.rglob('*') if p.is_file()))
        index=(HELP/'Documentation.html').read_text(encoding='utf-8')
        self.assertIn('DocData/viewer.js',index);self.assertIn("connect-src 'none'",index)
        self.assertNotIn('<iframe',index);self.assertNotRegex(index,r'(?:src|href)="https?://')
        for license in ['cydoc','esbuild','marked','prismjs']:self.assertTrue((HELP/f'DocData/licenses/{license}.txt').is_file())
        lock=json.loads((DOC/'package-lock.json').read_text(encoding='utf-8'))
        self.assertFalse(any('/three' in name for name in lock['packages']))

    def test_media_are_local_and_translated(self):
        self.assertEqual(self.data['logo'],'assets/cyinsights-logo.png')
        for p in self.data['pages']:
            for src in re.findall(r'(?:src|data-src)="([^"]+)"',p['html']):
                if src.startswith('assets/'):self.assertTrue((HELP/'DocData'/src).is_file(),src)
        for locale in ['en','fr']:
            p=self.pages['timeline',locale]
            self.assertIn('data-cydoc-module="image"',p['html'])
            self.assertIn('<svg',p['html'])
        self.assertNotIn('NOT LIVE DATA',(HELP/'DocData/assets/tile-grid-fr.svg').read_text(encoding='utf-8'))

    def test_publication_and_native_entry_paths(self):
        sys.path.insert(0,str(ROOT/'Tools'))
        from check_publication import content_issues
        for name in ['Documentation.html','DocData/data.js','DocData/manifest.json']:
            self.assertEqual(content_issues(name,(HELP/name).read_bytes()),[],name)
        self.assertIn('Documentation/Documentation.html',(ROOT/'App/ManualView.cpp').read_text(encoding='utf-8'))
        android=(ROOT/'Quest/CyInsightOVR/src/org/cyinsights/ovr/HelpActivity.java').read_text(encoding='utf-8')
        self.assertIn('file:///android_asset/Documentation/Documentation.html',android)
        self.assertIn('setBlockNetworkLoads(true)',android)

if __name__=='__main__':unittest.main(verbosity=2)
