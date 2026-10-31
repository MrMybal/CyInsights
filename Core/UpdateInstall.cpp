// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ReleaseUpdate.hpp"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
namespace cyinsights::updates {
namespace {
void Need(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
uint16_t U16(const unsigned char* p){return uint16_t(p[0]|p[1]<<8);}
uint32_t U32(const unsigned char* p){return uint32_t(U16(p))|uint32_t(U16(p+2))<<16;}
std::string Lower(std::string name){std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(std::tolower(c));});return name;}
void Extra(const unsigned char* data,size_t count){
    for(size_t at=0;at<count;){Need(at+4<=count,"Invalid ZIP extra data");auto id=U16(data+at),n=U16(data+at+2);at+=4;
        Need(n<=count-at&&id==0x5455,"Unsupported ZIP extra data; only ordinary timestamp metadata is accepted");at+=n;}
}
void NoReparse(const fs::path& path){
    auto absolute=fs::absolute(path).lexically_normal();fs::path current;
    for(const auto& part:absolute){current/=part;auto attrs=GetFileAttributesW(current.c_str());
        if(attrs!=INVALID_FILE_ATTRIBUTES)Need(!(attrs&FILE_ATTRIBUTE_REPARSE_POINT),"Update paths cannot cross a junction or symbolic link");}
}
cygi::Json Manifest(const fs::path& prepared,const std::string& version){
    NoReparse(prepared);auto file=prepared/L"release-manifest.json";
    Need(fs::is_regular_file(file)&&fs::file_size(file)<=4*1024*1024,"Update manifest missing or too large");
    std::ifstream in(file);std::string text((std::istreambuf_iterator<char>(in)),{});cygi::Json j;
    Need(cygi::Json::Parse(text,j)&&j.IsObject()&&j["version"].AsString()==version&&j["files"].IsObject(),"Update manifest does not match the selected release");return j;
}
void AtomicCopy(const fs::path& source,const fs::path& destination){
    NoReparse(destination);fs::create_directories(destination.parent_path());auto temp=destination;temp+=L".cyinsights-update-new";
    Need(!fs::exists(temp),"An unfinished update file needs attention before retrying");
    if(!CopyFileW(source.c_str(),temp.c_str(),TRUE)){DeleteFileW(temp.c_str());throw std::runtime_error("Cannot stage replacement file; check disk space and folder permissions");}
    if(!MoveFileExW(temp.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temp.c_str());throw std::runtime_error("Cannot replace application file; close other CyInsights instances and retry");}
}
}
void CheckZip(const fs::path& zip){
    auto size=fs::file_size(zip);Need(size>=22&&size<=1024ull*1024*1024,"Invalid update archive size");
    std::ifstream in(zip,std::ios::binary);auto read=[&](uint64_t offset,size_t count){Need(offset<=size&&count<=size-offset,"ZIP entry outside archive");std::vector<unsigned char> b(count);in.seekg(offset);in.read(reinterpret_cast<char*>(b.data()),count);Need(bool(in),"Truncated update ZIP");return b;};
    auto tail=read(size-std::min<uint64_t>(size,65557),size_t(std::min<uint64_t>(size,65557)));size_t eocd=SIZE_MAX;
    for(size_t p=tail.size()-22;;--p){if(U32(tail.data()+p)==0x06054b50&&p+22+U16(tail.data()+p+20)==tail.size()){eocd=p;break;}if(p==0)break;}
    Need(eocd!=SIZE_MAX,"ZIP directory missing");auto e=tail.data()+eocd;
    Need(U16(e+4)==0&&U16(e+6)==0&&U16(e+8)==U16(e+10),"Split update ZIPs are unsupported");
    auto count=U16(e+10);auto bytes=U32(e+12);uint64_t offset=U32(e+16);
    Need(count>0&&count<50000&&bytes<=32*1024*1024&&offset+bytes==size-tail.size()+eocd,"Invalid ZIP directory bounds");
    auto directory=read(offset,bytes);size_t at=0;uint64_t expanded=0;std::set<std::string> names;
    for(unsigned i=0;i<count;++i){
        Need(at+46<=directory.size(),"Truncated ZIP directory");auto h=directory.data()+at;
        Need(U32(h)==0x02014b50,"Invalid ZIP directory entry");auto flags=U16(h+8),method=U16(h+10),n=U16(h+28),extra=U16(h+30),comment=U16(h+32);
        Need(!(flags&1)&&(method==0||method==8)&&at+46+n+extra+comment<=directory.size(),"Encrypted or invalid update ZIP entry");
        auto mode=U32(h+38)>>16;Need((mode&0170000)==0||(mode&0170000)==0100000,"Only regular files are allowed in update ZIPs");
        std::string name(reinterpret_cast<const char*>(h+46),n);Need(SafeRelativePath(name)&&names.insert(Lower(name)).second,"Unsafe or duplicate update ZIP path");
        Extra(h+46+n,extra);auto compressed=U32(h+20);expanded+=U32(h+24);Need(expanded<=2ull*1024*1024*1024,"Expanded update ZIP exceeds the package budget");
        auto local=read(U32(h+42),30);Need(U32(local.data())==0x04034b50&&U16(local.data()+6)==flags&&U16(local.data()+8)==method,"ZIP local header mismatch");
        auto ln=U16(local.data()+26),le=U16(local.data()+28);auto localname=read(uint64_t(U32(h+42))+30,size_t(ln)+le);
        Need(std::string(reinterpret_cast<const char*>(localname.data()),ln)==name,"ZIP local path mismatch");Extra(localname.data()+ln,le);
        Need(uint64_t(U32(h+42))+30+ln+le+compressed<=offset,"ZIP payload overlaps its directory");at+=46+n+extra+comment;
    }Need(at==directory.size()&&names.count("release-manifest.json")!=0,"ZIP manifest missing or trailing directory data");
}
void ValidatePrepared(const fs::path& prepared,const std::string& version){
    auto j=Manifest(prepared,version);std::set<std::string> expected,folded;
    for(const auto& pair:j["files"].Members()){
        Need(SafeRelativePath(pair.first)&&pair.first!="release-manifest.json"&&folded.insert(Lower(pair.first)).second,"Unsafe update manifest path");
        auto path=prepared/fs::u8path(pair.first);NoReparse(path);Need(fs::is_regular_file(path)&&HashFile(path)==pair.second.AsString(),"Extracted update file failed SHA-256 verification");expected.insert(pair.first);
    }
    for(auto name:{"CyInsightsApp.exe","CyCaptureTool.exe","CyInsightsUpdater.exe"})Need(expected.count(name)!=0,"Update package is missing a product executable");
    expected.insert("release-manifest.json");std::set<std::string> actual;
    for(const auto& e:fs::recursive_directory_iterator(prepared)){NoReparse(e.path());if(e.is_directory())continue;Need(e.is_regular_file(),"Unexpected update filesystem entry");actual.insert(e.path().lexically_relative(prepared).generic_u8string());}
    Need(actual==expected,"Update package contains unlisted files");
}
void Prepare(const fs::path& zip,const fs::path& prepared,const std::string& version,std::atomic_bool& cancel){
    CheckZip(zip);Need(!fs::exists(prepared),"Update extraction directory already exists");NoReparse(prepared);fs::create_directories(prepared);
    wchar_t system[32768]{};Need(GetSystemDirectoryW(system,DWORD(std::size(system)))!=0,"Windows system directory unavailable");auto tar=fs::path(system)/L"tar.exe";
    Need(fs::is_regular_file(tar),"This Windows installation lacks tar.exe; download and extract the release manually");
    auto line=Quote(tar.wstring())+L" -xf "+Quote(zip.wstring())+L" -C "+Quote(prepared.wstring());STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    Need(CreateProcessW(tar.c_str(),line.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)!=FALSE,"Cannot extract the update package");CloseHandle(pi.hThread);
    bool interrupted=false;auto start=GetTickCount64();while(WaitForSingleObject(pi.hProcess,100)==WAIT_TIMEOUT)if(cancel||GetTickCount64()-start>120000){TerminateProcess(pi.hProcess,1);WaitForSingleObject(pi.hProcess,5000);interrupted=true;break;}
    DWORD code=1;GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);Need(!interrupted&&code==0,"Update extraction failed or was cancelled");
    if(cancel)throw std::runtime_error("Update cancelled");ValidatePrepared(prepared,version);
}
void Apply(const fs::path& prepared,const fs::path& target,const fs::path& backup,const std::string& version){
    ValidatePrepared(prepared,version);NoReparse(target);NoReparse(backup);Need(fs::is_directory(target)&&!fs::exists(backup),"Invalid update transaction folders");
    Need(fs::is_regular_file(target/L"CyInsightsApp.exe"),"Target is not a CyInsights application directory");
    auto j=Manifest(prepared,version);std::vector<std::pair<std::string,bool>> changed;fs::create_directories(backup);
    try{
        std::vector<std::string> names;for(const auto& e:j["files"].Members())names.push_back(e.first);names.push_back("release-manifest.json");
        for(const auto& name:names){auto relative=fs::u8path(name);auto dest=target/relative;NoReparse(dest);bool existed=fs::exists(dest);
            if(existed){Need(fs::is_regular_file(dest),"An application file was replaced by a directory");fs::create_directories((backup/relative).parent_path());fs::copy_file(dest,backup/relative);}
            AtomicCopy(prepared/relative,dest);changed.emplace_back(name,existed);
        }
    }catch(...){
        auto failure=std::current_exception();bool restored=true;
        for(auto it=changed.rbegin();it!=changed.rend();++it)try{auto relative=fs::u8path(it->first);if(it->second)AtomicCopy(backup/relative,target/relative);else {NoReparse(target/relative);fs::remove(target/relative);}}catch(...){restored=false;}
        if(!restored)throw std::runtime_error("Update failed and rollback needs attention. Original files remain in the update transaction's backup folder.");std::rethrow_exception(failure);
    }
}
}
