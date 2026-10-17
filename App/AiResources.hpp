// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <CyGPUInspectorCore/Json.hpp>
namespace cyinsights {
// The native application owns its contracts and guide. No companion, network
// connection or developer checkout is needed to use local analysis tools.
const cygi::Json& EmbeddedAiCatalog();
const cygi::Json& EmbeddedAiMethod();
std::string AiUiText(const std::string& source,bool french);
}
