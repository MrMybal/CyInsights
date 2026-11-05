// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <d3d11.h>
#include <string>
#include <cstddef>
namespace cyinsights {
// Caller releases the returned view after every ImGui viewport has shut down.
ID3D11ShaderResourceView* LoadBrandLogo(HINSTANCE instance,ID3D11Device* device,std::string& error);
ID3D11ShaderResourceView* LoadEmbeddedPng(ID3D11Device* device,const unsigned char* data,std::size_t bytes,int& width,int& height,std::string& error);
}
