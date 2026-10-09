// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Branding.hpp"
#include "ResourceIds.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
#include <vector>
namespace cyinsights {
using Microsoft::WRL::ComPtr;
ID3D11ShaderResourceView* LoadBrandLogo(HINSTANCE instance,ID3D11Device* device,std::string& error){
    error.clear();
    auto resource=FindResourceW(instance,MAKEINTRESOURCEW(IDR_CYINSIGHTS_LOGO),MAKEINTRESOURCEW(10));
    auto bytes=resource?SizeofResource(instance,resource):0;
    auto loaded=resource?LoadResource(instance,resource):nullptr;
    auto data=loaded?static_cast<BYTE*>(LockResource(loaded)):nullptr;
    if(!device||!data||!bytes){error="Embedded CyInsights logo missing";return nullptr;}
    auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(initialized)&&initialized!=RPC_E_CHANGED_MODE){error="Cannot initialize logo decoder";return nullptr;}
    struct ComScope{bool owned;~ComScope(){if(owned)CoUninitialize();}} com{SUCCEEDED(initialized)};
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    auto hr=CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory));
    if(SUCCEEDED(hr))hr=factory->CreateStream(&stream);
    if(SUCCEEDED(hr))hr=stream->InitializeFromMemory(data,bytes);
    if(SUCCEEDED(hr))hr=factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder);
    if(SUCCEEDED(hr))hr=decoder->GetFrame(0,&frame);
    if(SUCCEEDED(hr))hr=factory->CreateFormatConverter(&converter);
    if(SUCCEEDED(hr))hr=converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom);
    UINT width=0,height=0;
    if(SUCCEEDED(hr))hr=converter->GetSize(&width,&height);
    if(FAILED(hr)||!width||!height||width>std::numeric_limits<UINT>::max()/4/height){error="Cannot decode embedded logo";return nullptr;}
    std::vector<BYTE> pixels(size_t(width)*height*4);
    hr=converter->CopyPixels(nullptr,width*4,static_cast<UINT>(pixels.size()),pixels.data());
    if(FAILED(hr)){error="Cannot read embedded logo pixels";return nullptr;}
    D3D11_TEXTURE2D_DESC description{};
    description.Width=width;description.Height=height;description.MipLevels=1;description.ArraySize=1;
    description.Format=DXGI_FORMAT_R8G8B8A8_UNORM;description.SampleDesc.Count=1;
    description.Usage=D3D11_USAGE_IMMUTABLE;description.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{pixels.data(),width*4,0};
    ComPtr<ID3D11Texture2D> texture;
    hr=device->CreateTexture2D(&description,&initial,&texture);
    ID3D11ShaderResourceView* view=nullptr;
    if(SUCCEEDED(hr))hr=device->CreateShaderResourceView(texture.Get(),nullptr,&view);
    if(FAILED(hr)){error="Cannot upload CyInsights logo to renderer";return nullptr;}
    return view;
}
}
