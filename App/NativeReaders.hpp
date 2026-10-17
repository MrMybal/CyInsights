// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
namespace cyinsights {
inline std::filesystem::path ApplicationDirectory() {
    wchar_t path[32768]{}; GetModuleFileNameW(nullptr,path,32768);
    return std::filesystem::path(path).parent_path();
}
inline std::filesystem::path NativeDataDirectory() {
    wchar_t path[32768]{};auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
    if(!n||n>=32768)throw std::runtime_error("Windows local application data directory unavailable");
    return std::filesystem::path(path)/"CyInsights";
}
inline std::filesystem::path AnalyzerEngine(const std::filesystem::path& analyzer) {
    return analyzer.parent_path().parent_path().parent_path();
}
inline std::string ReadNativeFile(const std::filesystem::path& path) {
    std::ifstream in(path,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};
}
inline std::string AnalyzerVersion(const std::filesystem::path& analyzer) {
    const auto text=ReadNativeFile(AnalyzerEngine(analyzer)/"Build/Build.version");
    auto number=[&](const char* key){auto p=text.find(key);if(p==std::string::npos)return -1;p=text.find(':',p);if(p==std::string::npos)return -1;try{return std::stoi(text.substr(p+1));}catch(...){return -1;}};
    int major=number("MajorVersion"),minor=number("MinorVersion");
    return major<0||minor<0?std::string():std::to_string(major)+"."+std::to_string(minor);
}
inline std::string NativeCacheKey(const std::filesystem::path& analyzer) {
    std::error_code ec;auto path=std::filesystem::weakly_canonical(analyzer,ec).generic_u8string();
    std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    auto value=path+ReadNativeFile(AnalyzerEngine(analyzer)/"Build/Build.version");
    for(const auto* dll:{L"UnrealInsights-Core.dll",L"UnrealInsights-TraceAnalysis.dll",L"UnrealInsights-TraceServices.dll"}) {
        auto file=analyzer.parent_path()/dll;auto size=std::filesystem::file_size(file,ec);
        if(!ec)value+=std::to_string(size);ec.clear();auto time=std::filesystem::last_write_time(file,ec);
        if(!ec)value+=std::to_string(time.time_since_epoch().count());ec.clear();
    }
    uint64_t hash=14695981039346656037ull;for(unsigned char c:value){hash^=c;hash*=1099511628211ull;}
    std::ostringstream out;out<<std::hex<<hash;return out.str();
}
inline std::filesystem::path NativeCacheDirectory(const std::filesystem::path& analyzer) {
    return NativeDataDirectory()/"NativeReaders"/NativeCacheKey(analyzer);
}
inline std::string NativeReaderSourceStamp(const std::filesystem::path& app) {
    uint64_t hash=14695981039346656037ull;
    auto add=[&](const auto& value){for(auto c:value){hash^=static_cast<unsigned char>(c);hash*=1099511628211ull;}hash^=0xff;hash*=1099511628211ull;};
    for(const auto* part:{"InstallNativeReaders.ps1","NativeTiming","NativeTasks","NativeActivities"}) {
        auto path=app/"Tools"/part;
        std::vector<std::filesystem::path> files;std::error_code ec;
        if(std::filesystem::is_regular_file(path,ec))files.push_back(path);
        else if(std::filesystem::is_directory(path,ec))for(const auto& entry:std::filesystem::recursive_directory_iterator(path))if(entry.is_regular_file())files.push_back(entry.path());
        std::sort(files.begin(),files.end());
        for(const auto& file:files){add(file.lexically_relative(app).generic_u8string());add(ReadNativeFile(file));}
    }
    std::ostringstream result;result<<std::hex<<hash;return result.str();
}
inline std::filesystem::path FindNativeReader(const char* name,const std::filesystem::path& analyzer,
                                             const std::filesystem::path& app=ApplicationDirectory()) {
    auto relative=std::filesystem::path(std::string("Native")+name)/AnalyzerVersion(analyzer)/(std::string("CyNative")+name+".exe");
    std::error_code ec;
    try {auto cache=NativeCacheDirectory(analyzer);if(ReadNativeFile(cache/"ready.txt")==NativeReaderSourceStamp(app)&&std::filesystem::is_regular_file(cache/relative,ec))return cache/relative;}catch(...){}
    return app/relative;
}
}
