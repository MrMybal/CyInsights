// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ReleaseUpdate.hpp"
#include <CyGPUInspectorCore/Sha256.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <regex>
#include <stdexcept>
#include <tuple>
namespace cyinsights::updates {
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool Hash(const std::string& value){return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return c>='0'&&c<='9'||c>='a'&&c<='f';});}
}
std::optional<Version> Version::Parse(const std::string& input){
    static const std::regex pattern(R"(^v?(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})\.(0|[1-9][0-9]{0,8})(-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?$)");
    std::smatch m;if(!std::regex_match(input,m,pattern))return {};
    return Version{static_cast<uint32_t>(std::stoul(m[1])),static_cast<uint32_t>(std::stoul(m[2])),static_cast<uint32_t>(std::stoul(m[3])),m[4].matched};
}
bool Version::NewerThan(const Version& o) const {
    auto a=std::tie(major,minor,patch),b=std::tie(o.major,o.minor,o.patch);
    return a>b||(a==b&&!prerelease&&o.prerelease);
}
std::optional<Release> Select(const cygi::Json& j,const std::string& installed,const std::string& product){
    auto current=Version::Parse(installed);Require(current.has_value(),"Invalid installed application version");
    if(!j.IsObject()||j["draft"].GetType()!=cygi::Json::Type::boolean||j["prerelease"].GetType()!=cygi::Json::Type::boolean||j["draft"].AsBool()||j["prerelease"].AsBool())return {};
    auto tag=j["tag_name"].AsString();auto candidate=Version::Parse(tag);
    if(!candidate||candidate->prerelease||!candidate->NewerThan(*current))return {};
    Require(product=="Windows-x64.zip"||product=="Quest-Standard-arm64.apk"||product=="Quest-Vulkan-arm64.apk","Unsupported update product");
    std::string name="CyInsights-"+tag+"-"+product;
    auto expected="https://github.com/MrMybal/CyInsights/releases/download/"+tag+"/"+name;
    std::optional<Release> result;
    for(const auto& a:j["assets"].Items())if(a["name"].AsString()==name){
        Require(!result,"Duplicate update asset");auto digest=a["digest"].AsString();
        Require(digest.rfind("sha256:",0)==0&&Hash(digest.substr(7)),"The release does not provide a valid SHA-256 digest");
        Require(a["browser_download_url"].AsString()==expected,"Unexpected GitHub asset location");
        auto size=a["size"].AsUInt();Require(size>0&&size<=1024ull*1024*1024,"Invalid update download size");
        auto url="https://github.com/MrMybal/CyInsights/releases/tag/"+tag;
        Require(j["html_url"].AsString()==url,"Unexpected GitHub release location");
        result=Release{tag,tag[0]=='v'?tag.substr(1):tag,url,name,expected,digest.substr(7),size};
    }
    return result;
}
bool SafeRelativePath(const std::string& name){
    if(name.empty()||name.size()>240||name.front()=='/'||name.back()=='/'||name.find('\\')!=std::string::npos)return false;
    size_t start=0;
    while(start<name.size()){
        auto end=name.find('/',start);if(end==std::string::npos)end=name.size();auto part=name.substr(start,end-start);
        if(part.empty()||part=="."||part==".."||part.back()=='.'||part.back()==' ')return false;
        for(unsigned char c:part)if(c<32||c>=127||std::string(":*?\"<>|").find(char(c))!=std::string::npos)return false;
        std::transform(part.begin(),part.end(),part.begin(),[](unsigned char c){return char(std::toupper(c));});
        auto stem=part.substr(0,part.find('.'));
        if(stem=="CON"||stem=="NUL"||stem=="PRN"||stem=="AUX"||std::regex_match(stem,std::regex("(COM|LPT)[1-9]")))return false;
        start=end+1;
    }return true;
}
bool AllowedHttps(const std::string& url){
    static const std::regex pattern(R"(^https://(api\.github\.com|github\.com|release-assets\.githubusercontent\.com|objects\.githubusercontent\.com)(/[^\s#]*)?$)");
    return std::regex_match(url,pattern);
}
std::string HashFile(const fs::path& file){
    std::ifstream in(file,std::ios::binary);Require(bool(in),"Cannot read update file");cygi::Sha256 hash;std::array<char,65536> data;
    while(in){in.read(data.data(),data.size());auto count=in.gcount();if(count>0)hash.Update(data.data(),size_t(count));}
    Require(in.eof(),"Cannot finish reading update file");return hash.Finish().ToHex();
}
void VerifyFile(const fs::path& file,const std::string& hash,uint64_t size){Require(Hash(hash)&&fs::is_regular_file(file)&&fs::file_size(file)==size&&HashFile(file)==hash,"Update size or SHA-256 verification failed");}
std::wstring Quote(const std::wstring& argument){
    std::wstring out=L"\"";size_t slashes=0;
    for(auto c:argument){if(c==L'\\'){++slashes;continue;}if(c==L'\"')out.append(slashes*2+1,L'\\');else out.append(slashes,L'\\');slashes=0;out+=c;}
    out.append(slashes*2,L'\\');return out+L'\"';
}
}
