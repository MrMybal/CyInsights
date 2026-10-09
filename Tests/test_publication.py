"""Check publication gates without printing candidate credentials.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

MODULE = Path(__file__).resolve().parents[1] / 'Tools/check_publication.py'
spec = importlib.util.spec_from_file_location('publication', MODULE)
publication = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publication)


class PublicationChecks(unittest.TestCase):
    def test_paths(self):
        paths = [b'C:/' + b'Users/Example/file.txt', b'/home/' + b'example/file.txt',
                 b'G:/' + b'UnrealEngine/Engine', b'D:/' + b'_Plugins/Product/file.cpp']
        for path in paths:
            self.assertTrue(publication.content_issues('App/example.cpp', path))
        for path in (b'C:/Example/Engine', b'${USERPROFILE}/file.txt', b'../build/source'):
            self.assertFalse(publication.content_issues('Docs/example.md', path))

    def test_secret_candidates_are_not_emitted(self):
        candidates = [b'ghp_' + b'a' * 36, b'sk-proj-' + b'b' * 48,
                      b'xoxb-' + b'1' * 30, b'AKIA' + b'A' * 16]
        for candidate in candidates:
            result = publication.content_issues('App/example.cpp', candidate)
            self.assertTrue(result)
            self.assertNotIn(candidate.decode(), str(result))

    def test_history_and_commit_messages(self):
        original = publication.ROOT
        with tempfile.TemporaryDirectory(prefix='cy-publication-test-') as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.run(['git', *args], cwd=root, check=True, capture_output=True)
            git('init', '-q')
            git('config', 'user.name', 'Example')
            git('config', 'user.email', 'example@example.invalid')
            (root / 'example.txt').write_text('C:/' + 'Users/Example/file.txt')
            git('add', 'example.txt')
            git('commit', '-q', '-m', 'Initial example')
            git('config', 'user.email', '123+example@users.noreply.github.com')
            (root / 'example.txt').write_text('C:/Example/file.txt')
            git('add', 'example.txt')
            git('commit', '-q', '-m', 'Update example ' + 'ghp_' + 'a' * 36)
            publication.ROOT = root
            try:
                issues, commits, blobs = publication.history_issues()
            finally:
                publication.ROOT = original
            self.assertEqual(commits, 2)
            self.assertEqual(blobs, 2)
            self.assertTrue(any('non-noreply' in issue for issue in issues))
            self.assertTrue(any('machine-specific' in issue for issue in issues))
            self.assertTrue(any('commit message' in issue for issue in issues))
            self.assertNotIn('example@example.invalid', str(issues))


if __name__ == '__main__':
    unittest.main()
