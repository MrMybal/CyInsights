// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "AiRuntime.hpp"
namespace cyinsights {
std::filesystem::path AiSourceDirectory();
ai_runtime::Setup& AiRuntimeSetup();
bool DrawAiRuntimeSetup(bool french);
}
