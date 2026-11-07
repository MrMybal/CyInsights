// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <Windows.h>
#include <filesystem>
#include <stdexcept>

namespace cyinsights {
inline std::filesystem::path ExecutableDirectory() {
    wchar_t path[32768]{};
    auto length=GetModuleFileNameW(nullptr,path,32768);
    if(!length||length>=32768)throw std::runtime_error("Application directory unavailable");
    return std::filesystem::path(path).parent_path();
}
inline std::filesystem::path ResolveUserDataDirectory(const std::filesystem::path& app,
                                                     const std::filesystem::path& local) {
    std::error_code error;
    if(std::filesystem::is_regular_file(app/L"cyinsights-portable.txt",error))return app/L"Data";
    if(!local.is_absolute())throw std::runtime_error("Windows local application data directory unavailable");
    return local/L"CyInsights";
}
inline std::filesystem::path UserDataDirectory() {
    wchar_t local[32768]{};auto length=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    return ResolveUserDataDirectory(ExecutableDirectory(),length&&length<32768?std::filesystem::path(local):std::filesystem::path());
}
inline std::filesystem::path LayoutFile() {
    auto file=UserDataDirectory()/L"CyInsights-layout-v2.ini";
    std::filesystem::create_directories(file.parent_path());
    // Preserve layouts from older ZIP releases, without replacing current settings.
    auto legacy=ExecutableDirectory()/file.filename();std::error_code error;
    if(!std::filesystem::exists(file,error)&&std::filesystem::is_regular_file(legacy,error))
        std::filesystem::copy_file(legacy,file,std::filesystem::copy_options::skip_existing,error);
    return file;
}
}
