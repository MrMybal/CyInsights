// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiResources.hpp"
#include "ResourceIds.h"
#include <Windows.h>
#include <stdexcept>
namespace cyinsights {
namespace {
cygi::Json ReadResource(int id){
    auto module=GetModuleHandleW(nullptr);
    auto resource=FindResourceW(module,MAKEINTRESOURCEW(id),MAKEINTRESOURCEW(10));
    auto count=resource?SizeofResource(module,resource):0;
    auto loaded=resource?LoadResource(module,resource):nullptr;
    auto bytes=loaded?static_cast<const char*>(LockResource(loaded)):nullptr;
    cygi::Json result;
    if(!bytes||!count||!cygi::Json::Parse(std::string(bytes,count),result))throw std::runtime_error("Embedded AI contract unavailable");
    return result;
}
}
const cygi::Json& EmbeddedAiCatalog(){static const auto value=ReadResource(IDR_CYINSIGHTS_AI_CATALOG);return value;}
const cygi::Json& EmbeddedAiMethod(){static const auto value=ReadResource(IDR_CYINSIGHTS_AI_METHOD);return value;}
std::string AiUiText(const std::string& source,bool french){
    if(!french)return source;
    static const auto locale=ReadResource(IDR_CYINSIGHTS_AI_LOCALES);
    auto value=locale[source]["fr"].AsString();return value.empty()?source:value;
}
}
