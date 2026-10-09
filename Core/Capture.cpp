// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Capture.hpp"
#include <CyGPUInspectorCore/Sha256.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace cyinsights {
namespace {
constexpr uint64_t MaxArchiveBytes = 128ull * 1024 * 1024 * 1024;
void Require(bool ok, const char* error) { if (!ok) throw std::runtime_error(error); }
bool SafeName(const std::string& name) {
    if (name.empty() || name.size() > 4096 || name.front() == '/' || name.back() == '/') return false;
    if (name.find_first_of("\\:\0", 0, 3) != std::string::npos) return false;
    if (name.find_first_of("<>\"|?*") != std::string::npos || std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;})) return false;
    for (const auto& part : fs::u8path(name)) {
        const auto s = part.u8string();
        if (s.empty() || s == "." || s == ".." || s.back() == '.' || s.back() == ' ') return false;
        std::string base = s.substr(0, s.find('.'));
        std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c){return static_cast<char>(std::toupper(c));});
        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" ||
            (base.size() == 4 && (base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") && base[3] >= '0' && base[3] <= '9')) return false;
    }
    return true;
}
void Put(std::ostream& s, uint64_t value, size_t bytes) {
    for (size_t i = 0; i < bytes; ++i) s.put(static_cast<char>(value >> (8*i)));
}
uint64_t Get(std::istream& s, size_t bytes) {
    uint64_t v = 0;
    for (size_t i=0;i<bytes;++i) { int c=s.get(); Require(c != EOF, "Truncated header"); v |= uint64_t(static_cast<unsigned char>(c)) << (8*i); }
    return v;
}
cygi::Sha256Digest Stream(std::istream& in, uint64_t length, std::ostream* out = nullptr) {
    cygi::Sha256 hash; std::array<char, 65536> buffer{};
    while (length) {
        const auto n = static_cast<std::streamsize>(std::min<uint64_t>(length, buffer.size()));
        in.read(buffer.data(), n); Require(in.gcount() == n, "Truncated payload");
        hash.Update(buffer.data(), static_cast<size_t>(n));
        if (out) { out->write(buffer.data(), n); Require(bool(*out), "Cannot write payload"); }
        length -= n;
    }
    return hash.Finish();
}
bool ReadManifest(std::istream& in, uint64_t length, cygi::Json& m, std::string& error) {
    Require(length <= 16*1024*1024, "Manifest too large");
    std::string text(static_cast<size_t>(length), '\0'); in.read(text.data(), static_cast<std::streamsize>(length));
    return cygi::Json::Parse(text, m, &error) && ValidateManifest(m, error);
}
}
bool ValidateManifest(const cygi::Json& m, std::string& error) {
    if (!m.IsObject() || m["format"].AsString() != "cycapture" || m["version"].AsNumber() != 1 ||
        m["session_id"].AsString().empty() || !m["sources"].IsArray()) {
        error = "Unsupported or invalid cycapture manifest"; return false;
    }
    std::set<std::string> source_ids;
    for (const auto& s : m["sources"].Items()) {
        auto status=s["status"].AsString();
        if (s["id"].AsString().empty() || (status!="measured" && status!="derived" && status!="unavailable")) {
            error = "Every source needs an id and measured/derived/unavailable status"; return false;
        }
        if (s.Has("path") && !SafeName(s["path"].AsString())) { error="Unsafe source path"; return false; }
        if (!source_ids.insert(s["id"].AsString()).second) {error="Duplicate source id";return false;}
    }
    return true;
}
bool Inspect(const fs::path& file, Capture& out, std::string& error) {
    try {
        Capture result; result.file=file;
        const auto total=fs::file_size(file); Require(total <= MaxArchiveBytes, "Archive too large");
        std::ifstream in(file, std::ios::binary); char magic[8]{}; in.read(magic,8);
        Require(std::string(magic,8)=="CYCAP001", "Unknown capture container version");
        const auto count=Get(in,4); Require(count > 0 && count <= 100000, "Invalid entry count");
        std::set<std::string> names;
        for (uint64_t i=0;i<count;++i) {
            const auto n=Get(in,4), size=Get(in,8); cygi::Sha256Digest hash;
            in.read(reinterpret_cast<char*>(hash.bytes.data()),32); Require(bool(in), "Truncated digest");
            Require(n > 0 && n <= 4096, "Invalid path length");
            std::string name(static_cast<size_t>(n),'\0'); in.read(name.data(),static_cast<std::streamsize>(n));
            Require(bool(in) && SafeName(name), "Unsafe archive path");
            auto key=name; std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            for(size_t slash=key.find('/');slash!=std::string::npos;slash=key.find('/',slash+1))
                Require(names.count(key.substr(0,slash))==0,"File/directory path collision");
            auto descendant=names.lower_bound(key+"/");
            Require(descendant==names.end() || descendant->rfind(key+"/",0)!=0,"File/directory path collision");
            Require(names.insert(key).second, "Duplicate archive path");
            const auto offset=static_cast<uint64_t>(in.tellg());
            Require(offset <= total && size <= total-offset, "Payload exceeds archive");
            Require(Stream(in,size)==hash, "Capture checksum mismatch");
            if (i==0) {
                Require(name=="manifest.json", "First entry must be manifest.json");
                in.seekg(static_cast<std::streamoff>(offset));
                Require(ReadManifest(in,size,result.manifest,error), error.c_str());
            }
            result.entries.push_back({name,size,offset,hash.ToHex()});
        }
        Require(static_cast<uint64_t>(in.tellg())==total, "Unexpected trailing capture bytes");
        for (const auto& s:result.manifest["sources"].Items()) if(s.Has("path")) {
            const auto name=s["path"].AsString();
            Require(std::any_of(result.entries.begin(),result.entries.end(),[&](const Entry& e){return e.name==name;}), "Manifest references missing payload");
        }
        out=std::move(result); return true;
    } catch(const std::exception& e) {error=e.what();return false;}
}
bool Pack(const fs::path& directory, const fs::path& output, std::string& error) {
    fs::path temporary;
    try {
        Require(!fs::exists(output), "Output already exists");
        std::vector<fs::path> files{directory/"manifest.json"};
        for(const auto& f:fs::recursive_directory_iterator(directory)) {
            Require(!fs::is_symlink(f.symlink_status()), "Capture inputs cannot be symlinks");
            if(f.is_regular_file() && f.path()!=files.front()) files.push_back(f.path());
        }
        Require(files.size()<=100000, "Too many payloads");
        Require(!fs::exists(output.parent_path()/ (output.filename().wstring()+L".partial")), "Temporary output already exists");
        temporary=output;temporary+=L".partial";
        // Output must be outside the staging directory.
        const auto rel=fs::absolute(output).lexically_normal().lexically_relative(fs::absolute(directory).lexically_normal());
        Require(rel.empty() || *rel.begin()=="..", "Output must be outside capture inputs");
        std::ofstream out(temporary,std::ios::binary); Require(bool(out),"Cannot create archive");
        out.write("CYCAP001",8);Put(out,files.size(),4);
        for(const auto& file:files) {
            const auto name=fs::relative(file,directory).generic_u8string(); Require(SafeName(name),"Unsafe input path");
            const auto size=fs::file_size(file); std::ifstream in(file,std::ios::binary);Require(bool(in),"Cannot read capture input");
            const auto hash=Stream(in,size);in.clear();in.seekg(0);
            Put(out,name.size(),4);Put(out,size,8);out.write(reinterpret_cast<const char*>(hash.bytes.data()),32);out.write(name.data(),name.size());
            Require(Stream(in,size,&out)==hash && fs::file_size(file)==size,"Capture changed while packaging");
        }
        out.close();Require(bool(out),"Cannot finalize archive");
        Capture checked;Require(Inspect(temporary,checked,error),error.c_str());
        fs::rename(temporary,output);return true;
    }catch(const std::exception& e){error=e.what(); if(!temporary.empty()){std::error_code ec;fs::remove(temporary,ec);}return false;}
}
bool Extract(const Capture& capture,const fs::path& fresh,std::string& error) {
    bool created=false;
    try {
        Capture checked;Require(Inspect(capture.file,checked,error),error.c_str());
        Require(!fs::exists(fresh),"Extraction requires a new directory");
        Require(fs::create_directories(fresh),"Cannot create extraction directory");created=true;
        std::ifstream in(checked.file,std::ios::binary);
        for(const auto& e:checked.entries) {
            const auto path=fresh/fs::u8path(e.name);fs::create_directories(path.parent_path());
            Require(!fs::exists(path),"Extraction path collision on this filesystem");
            in.clear();in.seekg(static_cast<std::streamoff>(e.offset)); std::ofstream out(path,std::ios::binary);
            Require(bool(out),"Cannot create extracted file");
            Require(Stream(in,e.size,&out).ToHex()==e.sha256,"Capture changed while extracting");
        }
        return true;
    }catch(const std::exception& e){error=e.what();if(created){std::error_code ec;fs::remove_all(fresh,ec);}return false;}
}
bool MapClock(uint64_t ticks,const ClockAnchor& a,double& seconds) {
    if(!std::isfinite(a.frequency)||a.frequency<=0||!std::isfinite(a.session_seconds)||!std::isfinite(a.uncertainty_seconds)||a.uncertainty_seconds<0)return false;
    const double delta=ticks>=a.ticks ? static_cast<double>(ticks-a.ticks) : -static_cast<double>(a.ticks-ticks);
    seconds=a.session_seconds+delta/a.frequency;return std::isfinite(seconds);
}
}
