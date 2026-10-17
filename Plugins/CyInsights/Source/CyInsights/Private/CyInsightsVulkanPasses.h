// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
namespace CyInsightsVulkanPasses {
bool Enable();
void Disable();
void Shutdown();
FString Status();
uint64 Resolved();
uint64 Dropped();
uint64 Pending();
}
