// CyGPUInspector — Unreal plugin. Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT

#include "CyGPUInspectorSettings.h"

UCyGPUInspectorSettings::UCyGPUInspectorSettings()
{
}

#if WITH_EDITOR
FText UCyGPUInspectorSettings::GetSectionDescription() const
{
	return NSLOCTEXT("CyGPUInspector", "SettingsDescription",
		"Frame captures with CyGPUInspector: where ReShade, the add-on and the standalone are, and what a capture records.");
}
#endif
