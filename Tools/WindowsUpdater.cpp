// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ReleaseUpdate.hpp"
#include <Windows.h>
#include <fstream>
#include <stdexcept>
using namespace cyinsights::updates;
namespace {
cygi::Json Read(const fs::path& path){if(fs::file_size(path)>65536)throw std::runtime_error("Invalid update job size");std::ifstream in(path);std::string text((std::istreambuf_iterator<char>(in)),{});cygi::Json j;if(!cygi::Json::Parse(text,j)||!j.IsObject())throw std::runtime_error("Invalid update job");return j;}
void Launch(const fs::path& exe,const cygi::Json& job){
    auto args=Quote(exe.wstring());for(const auto& a:job["reopen"].Items()){auto path=fs::u8path(a.AsString());if(path.is_absolute()&&fs::is_regular_file(path))args+=L" "+Quote(path.wstring());}
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};if(!CreateProcessW(exe.c_str(),args.data(),nullptr,nullptr,FALSE,0,nullptr,exe.parent_path().c_str(),&si,&pi))throw std::runtime_error("Cannot restart CyInsights. Start CyInsightsApp.exe manually.");CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
}
}
int wmain(int argc,wchar_t** argv){
    fs::path work;bool parent_closed=false;cygi::Json job;
    try{
        if(argc!=3||std::wstring(argv[1])!=L"--apply")throw std::runtime_error("Launch this helper from CyInsights > Help > Updates.");
        auto path=fs::canonical(argv[2]);wchar_t self[32768]{};GetModuleFileNameW(nullptr,self,DWORD(std::size(self)));
        if(path.filename()!=L"install.json"||fs::canonical(self).parent_path().parent_path()!=path.parent_path())throw std::runtime_error("Invalid updater transaction location");
        work=path.parent_path();job=Read(path);auto version=job["version"].AsString();auto v=Version::Parse(version);
        if(!v||v->prerelease)throw std::runtime_error("Only stable releases can be installed by this updater");
        auto installed=Version::Parse(CYINSIGHTS_UPDATE_VERSION);if(!installed||!v->NewerThan(*installed))throw std::runtime_error("The update is not newer than the installed application");
        auto target=fs::u8path(job["target"].AsString());if(!target.is_absolute()||fs::canonical(target)==fs::canonical(work))throw std::runtime_error("Invalid application destination");
        VerifyFile(work/L"update.zip",job["sha256"].AsString(),job["size"].AsUInt());ValidatePrepared(work/L"prepared",version);
        auto pid=job["pid"].AsUInt();if(pid==0||pid>MAXDWORD)throw std::runtime_error("Invalid application process");
        HANDLE parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,DWORD(pid));
        if(parent){
            wchar_t image[32768]{};DWORD chars=DWORD(std::size(image));FILETIME created{},exit{},kernel{},user{};ULARGE_INTEGER time{};
            bool valid=QueryFullProcessImageNameW(parent,0,image,&chars)&&GetProcessTimes(parent,&created,&exit,&kernel,&user);
            time.LowPart=created.dwLowDateTime;time.HighPart=created.dwHighDateTime;
            if(!valid||!fs::equivalent(image,target/L"CyInsightsApp.exe")||std::to_string(time.QuadPart)!=job["process_created"].AsString()){CloseHandle(parent);throw std::runtime_error("The application process identity changed");}
            auto waited=WaitForSingleObject(parent,120000);CloseHandle(parent);if(waited!=WAIT_OBJECT_0)throw std::runtime_error("CyInsights did not close. No application file was replaced.");
        }else if(GetLastError()!=ERROR_INVALID_PARAMETER)throw std::runtime_error("Cannot verify the application process");
        parent_closed=true;Apply(work/L"prepared",target,work/L"backup",version);
        auto result=cygi::Json::Object();result["status"]="installed";result["version"]=version;std::ofstream(work/L"result.json")<<result.Write(2);
        Launch(target/L"CyInsightsApp.exe",job);return 0;
    }catch(const std::exception& e){
        if(!work.empty()){auto result=cygi::Json::Object();result["status"]="failed";result["error"]=e.what();std::ofstream(work/L"result.json")<<result.Write(2);}
        auto text=std::string("CyInsights update failed: ")+e.what();MessageBoxA(nullptr,text.c_str(),"CyInsights updater",MB_OK|MB_ICONERROR);
        if(parent_closed)try{Launch(fs::u8path(job["target"].AsString())/L"CyInsightsApp.exe",job);}catch(...){}
        return 1;
    }
}
