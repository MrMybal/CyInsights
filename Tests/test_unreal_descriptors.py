"""Capture configuration contract shared by UE 4.27 and UE 5.x.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UnrealDescriptors(unittest.TestCase):
    def setUp(self):
        self.plugins = {
            name: json.loads((ROOT / 'Plugins' / name / (name + '.uplugin')).read_text())
            for name in ('CyInsights', 'CyGPUInspector')
        }

    def test_capture_configurations(self):
        for plugin in self.plugins.values():
            for entry in plugin['Modules'] + plugin.get('Plugins', []):
                with self.subTest(name=entry['Name']):
                    self.assertEqual(set(entry['WhitelistTargetConfigurations']),
                                     {'Debug', 'DebugGame', 'Development'})

    def test_server_excluded(self):
        for plugin in self.plugins.values():
            for module in plugin['Modules']:
                with self.subTest(module=module['Name']):
                    self.assertTrue(module['Type'] == 'Editor' or
                                    'Server' in module.get('BlacklistTargets', []))

    def test_platform_collectors(self):
        main = self.plugins['CyInsights']
        quest = next(m for m in main['Modules'] if m['Name'] == 'CyInsightsQuest')
        self.assertEqual(quest['WhitelistPlatforms'], ['Android'])
        dependencies = {p['Name']: p for p in main['Plugins']}
        self.assertEqual(dependencies['OpenXR']['WhitelistPlatforms'], ['Android'])
        self.assertEqual(dependencies['CyGPUInspector']['WhitelistPlatforms'], ['Win64'])
        self.assertTrue(dependencies['CyGPUInspector']['Optional'])
        loader = next(m for m in self.plugins['CyGPUInspector']['Modules']
                      if m['Name'] == 'CyGPUInspectorLoader')
        self.assertEqual(loader['WhitelistPlatforms'], ['Win64'])

    def test_common_descriptor_schema(self):
        # UE4 does not read AllowList/DenyList. UE5.8 rejects both spellings
        # together but still accepts the legacy spelling by itself.
        for plugin in self.plugins.values():
            self.assertFalse(plugin.get('EngineVersion'))
            for entry in plugin['Modules'] + plugin.get('Plugins', []):
                self.assertFalse(any('AllowList' in key or 'DenyList' in key for key in entry))


if __name__ == '__main__':
    unittest.main()
