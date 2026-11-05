// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <d3d11.h>
#include <string>
#include <CyGPUInspectorCore/Json.hpp>
namespace cyinsights {
void InitializeManual(ID3D11Device*);
void ShutdownManual();
void ShowManual(const std::string& topic={},const std::string& language={});
void SelectManualMedia(const std::string& id);
void SelectManualAnchor(const std::string& id);
void DrawManual();
cygi::Json ManualReport();
}
