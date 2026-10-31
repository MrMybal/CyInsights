// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <CyGPUInspectorCore/Json.hpp>
#include <filesystem>
#include <optional>
#include <atomic>
#include <functional>
#include <string>
namespace cyinsights::updates {
namespace fs=std::filesystem;
inline constexpr const char* Api="https://api.github.com/repos/MrMybal/CyInsights/releases/latest";
struct Version {
    uint32_t major=0,minor=0,patch=0;bool prerelease=false;
    static std::optional<Version> Parse(const std::string& text);
    bool NewerThan(const Version& other) const;
};
struct Release {std::string tag,version,url,asset,url_download,sha256;uint64_t size=0;};
std::optional<Release> Select(const cygi::Json& json,const std::string& installed,const std::string& product);
bool SafeRelativePath(const std::string& name);
bool AllowedHttps(const std::string& url);
std::string HashFile(const fs::path& file);
void VerifyFile(const fs::path& file,const std::string& hash,uint64_t size);
using Progress=std::function<void(uint64_t,uint64_t)>;
cygi::Json FetchLatest(std::atomic_bool& cancel);
void Download(const Release& release,const fs::path& destination,std::atomic_bool& cancel,Progress progress={});
fs::path NewWorkDirectory();
void CheckZip(const fs::path& zip);
void ValidatePrepared(const fs::path& prepared,const std::string& version);
void Prepare(const fs::path& zip,const fs::path& prepared,const std::string& version,std::atomic_bool& cancel);
// Copies only manifest-owned files. Existing settings and captures are untouched.
// On failure every file already replaced is restored from the transaction backup.
void Apply(const fs::path& prepared,const fs::path& target,const fs::path& backup,const std::string& version);
std::wstring Quote(const std::wstring& argument);
}
