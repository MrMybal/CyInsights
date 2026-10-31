// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ReleaseUpdate.hpp"
#include <CyGPUInspectorCore/Sha256.hpp>
#include <Windows.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace cyinsights::updates;
namespace {
int checks=0;
void Check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
template<class F>void Reject(F action,const char* name){bool failed=false;try{action();}catch(...){failed=true;}Check(failed,name);}
void Write(const fs::path& p,const std::string& value){fs::create_directories(p.parent_path());std::ofstream(p,std::ios::binary)<<value;}
std::string Read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
std::string Hash(const std::string& s){return cygi::Sha256::Hash(s.data(),s.size()).ToHex();}
cygi::Json FixtureRelease(const std::string& tag="v9.0.0",const std::string& product="Windows-x64.zip"){
    auto j=cygi::Json::Object();j["draft"]=false;j["prerelease"]=false;j["tag_name"]=tag;j["html_url"]="https://github.com/MrMybal/CyInsights/releases/tag/"+tag;
    auto asset=cygi::Json::Object();auto name="CyInsights-"+tag+"-"+product;asset["name"]=name;asset["browser_download_url"]="https://github.com/MrMybal/CyInsights/releases/download/"+tag+"/"+name;asset["digest"]="sha256:"+std::string(64,'a');asset["size"]=50.;j["assets"]=cygi::Json::Array();j["assets"].Push(asset);return j;
}
using Files=std::map<std::string,std::string>;
Files Package(){Files f{{"CyInsightsApp.exe","new app"},{"CyCaptureTool.exe","new CLI"},{"CyInsightsUpdater.exe","new helper"},{"Docs/Updates.txt","new guide"}};auto j=cygi::Json::Object();j["version"]="v9.0.0";j["files"]=cygi::Json::Object();for(const auto& p:f)j["files"][p.first]=Hash(p.second);f["release-manifest.json"]=j.Write(2);return f;}
uint32_t Crc(const std::string& s){uint32_t c=~0u;for(unsigned char b:s){c^=b;for(int i=0;i<8;++i)c=(c>>1)^(0xedb88320u&uint32_t(-int(c&1)));}return ~c;}
void Zip(const fs::path& path,const Files& files,bool symlink=false,bool mismatch=false){
    std::string data,central;auto u16=[](std::string& s,uint16_t v){s+=char(v);s+=char(v>>8);};auto u32=[&](std::string& s,uint32_t v){u16(s,uint16_t(v));u16(s,uint16_t(v>>16));};
    for(const auto& f:files){auto offset=uint32_t(data.size()),crc=Crc(f.second);u32(data,0x04034b50);u16(data,20);u16(data,0);u16(data,0);u16(data,0);u16(data,0);u32(data,crc);u32(data,uint32_t(f.second.size()));u32(data,uint32_t(f.second.size()));u16(data,uint16_t(f.first.size()));u16(data,0);data+=f.first;if(mismatch)data.back()='x';data+=f.second;
        u32(central,0x02014b50);u16(central,0x314);u16(central,20);u16(central,0);u16(central,0);u16(central,0);u16(central,0);u32(central,crc);u32(central,uint32_t(f.second.size()));u32(central,uint32_t(f.second.size()));u16(central,uint16_t(f.first.size()));u16(central,0);u16(central,0);u16(central,0);u16(central,0);u32(central,(symlink?0120777u:0100644u)<<16);u32(central,offset);central+=f.first;
    }auto offset=uint32_t(data.size());data+=central;u32(data,0x06054b50);u16(data,0);u16(data,0);u16(data,uint16_t(files.size()));u16(data,uint16_t(files.size()));u32(data,uint32_t(central.size()));u32(data,offset);u16(data,0);Write(path,data);
}
}
int main(int argc,char** argv){try{
    auto root=fs::temp_directory_path()/("CyInsights-update-tests-"+std::to_string(GetCurrentProcessId()));fs::create_directories(root);
    auto good=FixtureRelease();Check(Select(good,"0.2.2-alpha.1","Windows-x64.zip").has_value(),"future stable");
    for(auto tag:{"v0.2.1","v0.2.2-alpha.2","v9.0.0-beta.1","v9.0.0-rc.1","v99.0.0+build","v09.0.0","not-a-version"})Check(!Select(FixtureRelease(tag),"0.2.2-alpha.1","Windows-x64.zip"),"old / prerelease / invalid tag excluded");
    Check(Select(FixtureRelease("v0.2.2"),"0.2.2-alpha.1","Windows-x64.zip").has_value(),"stable final replaces installed alpha");Check(!Select(FixtureRelease("v0.2.2"),"0.2.2","Windows-x64.zip"),"equal stable excluded");
    auto flagged=good;flagged["prerelease"]=true;Check(!Select(flagged,"0.1.0","Windows-x64.zip"),"prerelease flag excluded");flagged=good;flagged["draft"]=true;Check(!Select(flagged,"0.1.0","Windows-x64.zip"),"draft excluded");flagged=good;flagged["draft"]=cygi::Json();Check(!Select(flagged,"0.1.0","Windows-x64.zip"),"missing boolean rejected");
    Check(!Select(good,"0.1.0","Quest-Vulkan-arm64.apk"),"no variant substitution");
    for(auto product:{"Quest-Vulkan-arm64.apk","Quest-Standard-arm64.apk"})Check(Select(FixtureRelease("v9.0.0",product),"0.2.2-alpha.1",product).has_value(),"Quest asset selection");
    auto asset=good["assets"].Items().front();auto bad=good;bad["assets"].Push(asset);Reject([&]{Select(bad,"0.1.0","Windows-x64.zip");},"duplicate assets rejected");
    for(auto field:{"digest","browser_download_url"}){bad=good;auto a=asset;a[field]="invalid";bad["assets"]=cygi::Json::Array();bad["assets"].Push(a);Reject([&]{Select(bad,"0.1.0","Windows-x64.zip");},"invalid asset provenance rejected");}
    for(auto u:{"http://github.com/test","https://github.com.evil.test/test","https://github.com@evil.test/a","https://release-assets.githubusercontent.com.evil.test/x","https://127.0.0.1/","https://github.com:80/test"})Check(!AllowedHttps(u),"untrusted transport rejected");
    for(auto u:{Api,"https://github.com/MrMybal/CyInsights/releases/download/v9.0.0/a.zip","https://release-assets.githubusercontent.com/file?token=example"})Check(AllowedHttps(u),"trusted HTTPS accepted");
    for(auto p:{"../outside","/absolute","C:/absolute","Docs/../x","Docs\\x","a//b","a/","a:stream","CON","Docs/NUL.txt","Docs/COM1","Docs/a.","Docs/a ","./file"})Check(!SafeRelativePath(p),"unsafe update path rejected");
    Check(SafeRelativePath("Docs/Version 9.md"),"normal package path accepted");
    Write(root/"hash","abc");VerifyFile(root/"hash",Hash("abc"),3);++checks;Reject([&]{VerifyFile(root/"hash",Hash("wrong"),3);},"hash mismatch rejected");Reject([&]{VerifyFile(root/"hash",Hash("abc"),2);},"size mismatch rejected");
    auto files=Package();Zip(root/"good.zip",files);CheckZip(root/"good.zip");++checks;
    auto evil=files;evil["../escaped.txt"]="bad";Zip(root/"traversal.zip",evil);Reject([&]{CheckZip(root/"traversal.zip");},"ZIP traversal rejected");
    Zip(root/"symlink.zip",files,true);Reject([&]{CheckZip(root/"symlink.zip");},"ZIP symlinks rejected");
    Zip(root/"mismatch.zip",files,false,true);Reject([&]{CheckZip(root/"mismatch.zip");},"ZIP local path mismatch rejected");
    evil=files;evil["docs/updates.txt"]="alias";Zip(root/"alias.zip",evil);Reject([&]{CheckZip(root/"alias.zip");},"ZIP case aliases rejected");
    Write(root/"truncated.zip",Read(root/"good.zip").substr(0,30));Reject([&]{CheckZip(root/"truncated.zip");},"truncated ZIP rejected");
    std::atomic_bool cancel{false};Prepare(root/"good.zip",root/"prepared","v9.0.0",cancel);Check(Read(root/"prepared/CyInsightsApp.exe")=="new app","real Windows ZIP extraction");
    Write(root/"prepared/CyInsightsApp.exe","tampered");Reject([&]{ValidatePrepared(root/"prepared","v9.0.0");},"prepared file tampering rejected");Write(root/"prepared/CyInsightsApp.exe","new app");
    Write(root/"prepared/unlisted.txt","extra");Reject([&]{ValidatePrepared(root/"prepared","v9.0.0");},"unlisted extracted file rejected");fs::remove(root/"prepared/unlisted.txt");
    Reject([&]{ValidatePrepared(root/"prepared","v8.0.0");},"wrong manifest version rejected");
    auto target=root/"application";Write(target/"CyInsightsApp.exe","old app");Write(target/"CyCaptureTool.exe","old CLI");Write(target/"settings.ini","preserve me");
    Apply(root/"prepared",target,root/"backup","v9.0.0");Check(Read(target/"CyInsightsApp.exe")=="new app"&&Read(root/"backup/CyInsightsApp.exe")=="old app","files replaced with backup");Check(Read(target/"settings.ini")=="preserve me","settings preserved");
    auto locked=root/"locked";Write(locked/"CyInsightsApp.exe","original app");Write(locked/"CyCaptureTool.exe","original CLI");
    HANDLE hold=CreateFileW((locked/"CyInsightsApp.exe").c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);Check(hold!=INVALID_HANDLE_VALUE,"test lock created");
    Reject([&]{Apply(root/"prepared",locked,root/"rollback","v9.0.0");},"locked application prevents installation");CloseHandle(hold);
    Check(Read(locked/"CyCaptureTool.exe")=="original CLI"&&Read(locked/"CyInsightsApp.exe")=="original app","partial installation rolled back");
    if(argc>1&&std::string(argv[1])=="--network"){auto j=FetchLatest(cancel);Check(j["tag_name"].AsString().size()>0,"real GitHub metadata received");Check(!Select(j,"999999999.0.0","Windows-x64.zip"),"real metadata cannot downgrade");}
    if(argc>1&&std::string(argv[1])=="--download"){
        auto j=FetchLatest(cancel);auto candidate=Select(j,"0.0.0","Windows-x64.zip");Check(candidate.has_value(),"real stable download fixture selected");
        uint64_t received=0;Download(*candidate,root/"download.zip",cancel,[&](uint64_t n,uint64_t){received=n;});Check(received==candidate->size,"real HTTPS redirect and download");CheckZip(root/"download.zip");++checks;
        cancel=true;Reject([&]{Download(*candidate,root/"cancelled.zip",cancel);},"download cancellation");Check(!fs::exists(root/"cancelled.zip.partial")&&!fs::exists(root/"cancelled.zip"),"cancelled partial removed");
    }
    if(argc==4&&std::string(argv[1])=="--package"){
        Prepare(fs::u8path(argv[2]),root/"actual-package",argv[3],cancel);
        ValidatePrepared(root/"actual-package",argv[3]);++checks;
    }
    fs::remove_all(root);std::cout<<checks<<" updater checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"Updater test failed after "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
