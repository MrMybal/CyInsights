// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
namespace CyInsightsVulkanTimestamp {
// Does not patch engine code. Only the exported Vulkan function dispatch is
// wrapped, on the serialized RHI execution path of a supported Android build.
bool Enable();
void Disable();
void Shutdown();
}
