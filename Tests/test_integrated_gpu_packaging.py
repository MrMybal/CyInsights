"""Integrated Windows collector is part of the single source plugin package.
Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later.
"""
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Tools'))
from package_release import plugin_sources
from check_publication import content_issues


class IntegratedPluginPackaging(unittest.TestCase):
    def test_quest_pass_collector_and_reader_are_self_contained(self):
        entries=plugin_sources()
        self.assertIn('Plugins/CyInsights/Source/CyInsights/Private/CyInsightsVulkanPasses.cpp',entries)
        private=ROOT/'Plugins/CyInsights/Source/CyInsights/Private/QuestPassClock.h'
        reader=ROOT/'Tools/NativeTiming/QuestPassClock.h'
        self.assertEqual(private.read_bytes(),reader.read_bytes())
        self.assertNotIn('../../Plugins/',(ROOT/'Tools/NativeTiming/VulkanPassAnalyzer.hpp').read_text())

    def test_single_installable_plugin_with_internal_modules(self):
        entries=plugin_sources()
        descriptors=[name for name in entries if name.endswith('.uplugin')]
        self.assertEqual(descriptors,['Plugins/CyInsights/CyInsights.uplugin'])
        descriptor=json.loads(entries[descriptors[0]])
        self.assertNotIn('CyGPUInspector',[p['Name'] for p in descriptor.get('Plugins',[])])
        for name in ('CyInsightsGpuLoader','CyInsightsGpuWindows'):
            self.assertIn(name,[m['Name'] for m in descriptor['Modules']])
            self.assertIn(f'Plugins/CyInsights/Source/{name}/{name}.Build.cs',entries)
        self.assertIn('Plugins/CyInsights/Source/ThirdParty/CyInsightsGpu/CyInsightsGpuApi.h',entries)
        self.assertIn('Plugins/CyInsights/LICENSE',entries)

    def test_no_test_binary_or_machine_data_in_plugin_sources(self):
        for name,data in plugin_sources().items():
            with self.subTest(name=name):
                self.assertFalse({'Binaries','Intermediate','Saved'}.intersection(Path(name).parts))
                self.assertFalse(content_issues(name,data))


if __name__=='__main__':unittest.main()
