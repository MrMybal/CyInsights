// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <d3d11.h>
#include <string>
namespace cyinsights {
// Caller releases the returned view after every ImGui viewport has shut down.
ID3D11ShaderResourceView* LoadBrandLogo(HINSTANCE instance,ID3D11Device* device,std::string& error);
}
