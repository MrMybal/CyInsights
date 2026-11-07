"""Reject corrupted or incomplete offline SDK inputs before invoking pip."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('runtime_bootstrap',ROOT/'AI/PrepareRuntime.py')
bootstrap=importlib.util.module_from_spec(spec);spec.loader.exec_module(bootstrap)


class RuntimeBootstrapTests(unittest.TestCase):
    def fixture(self,root):
        wheel=root/'wheels/example-1.0-py3-none-any.whl';wheel.parent.mkdir()
        wheel.write_bytes(b'checked wheel fixture')
        (root/'dependency-lock.json').write_text(json.dumps({'wheels':[{'file':wheel.name,'sha256':hashlib.sha256(wheel.read_bytes()).hexdigest()}]}))
        (root/'requirements.lock.txt').write_text('example==1.0\n')
        return wheel

    def test_changed_wheel_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);wheel=self.fixture(root)
            self.assertEqual(bootstrap.verified_requirements(root)[0][:2],('example','1.0'))
            wheel.write_bytes(b'tampered')
            with self.assertRaisesRegex(RuntimeError,'integrity'):bootstrap.verified_requirements(root)

    def test_unpinned_or_missing_requirements_are_rejected(self):
        for invalid in ('example>=1.0\n','', 'example==2.0\n'):
            with self.subTest(requirement=invalid),tempfile.TemporaryDirectory() as folder:
                root=Path(folder);self.fixture(root);(root/'requirements.lock.txt').write_text(invalid)
                with self.assertRaises((RuntimeError,KeyError)):bootstrap.verified_requirements(root)

    def test_shipped_inventory_is_complete(self):
        requirements=bootstrap.verified_requirements(ROOT/'AI')
        self.assertEqual(len(requirements),38)
        self.assertIn(('cytools-core','0.10.1'),[(bootstrap.canonical(n),v) for n,v,_ in requirements])
        self.assertIn(('mcp','1.30.0'),[(n,v) for n,v,_ in requirements])


if __name__=='__main__':unittest.main()
