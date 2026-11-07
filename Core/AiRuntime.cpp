// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiRuntime.hpp"
#include "AiRuntimeConfig.hpp"
#include "ReleaseUpdate.hpp"
#include <CyGPUInspectorCore/Sha256.hpp>
#include <Windows.h>
#include <objbase.h>
#include <algorithm>
#include <fstream>
#include <vector>
#include <stdexcept>
namespace cyinsights::ai_runtime {
namespace {
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Handle {HANDLE h=nullptr;~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
void NoReparse(const fs::path& path){fs::path current;for(const auto& part:fs::absolute(path).lexically_normal()){
    current/=part;auto attrs=GetFileAttributesW(current.c_str());Need(attrs==INVALID_FILE_ATTRIBUTES||!(attrs&FILE_ATTRIBUTE_REPARSE_POINT),"Runtime folders cannot cross symbolic links or junctions");}}
cygi::Json Read(const fs::path& file){
    Need(fs::is_regular_file(file)&&fs::file_size(file)<1024*1024,"Runtime installation record is missing or invalid");
    std::ifstream in(file);std::string text((std::istreambuf_iterator<char>(in)),{});cygi::Json j;
    Need(cygi::Json::Parse(text,j)&&j.IsObject(),"Invalid runtime installation record");return j;
}
void Note(const fs::path& log,const std::string& message){std::ofstream(log,std::ios::app|std::ios::binary)<<message<<"\r\n";}
std::wstring Id(){GUID guid{};Need(SUCCEEDED(CoCreateGuid(&guid)),"Cannot create runtime transaction");wchar_t id[40]{};StringFromGUID2(guid,id,40);return id;}
void Process(const fs::path& executable,const std::vector<std::wstring>& arguments,const fs::path& cwd,
             const fs::path& log,std::atomic_bool& cancel,DWORD timeout){
    if(cancel)throw std::runtime_error("Runtime setup cancelled");
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    Handle output{CreateFileW(log.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    Handle input{CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    Need(output.h!=INVALID_HANDLE_VALUE&&input.h!=INVALID_HANDLE_VALUE,"Cannot open runtime installation log");
    Handle job{CreateJobObjectW(nullptr,nullptr)};Need(job.h!=nullptr,"Cannot isolate runtime installer process");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    Need(SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits))!=FALSE,"Cannot configure runtime process lifetime");
    auto command=updates::Quote(executable.wstring());for(const auto& arg:arguments)command+=L" "+updates::Quote(arg);
    STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=input.h;si.hStdOutput=si.hStdError=output.h;PROCESS_INFORMATION pi{};
    Need(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,cwd.c_str(),&si,&pi)!=FALSE,"Cannot start the private runtime installer");
    Handle process{pi.hProcess},thread{pi.hThread};
    if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,1);throw std::runtime_error("Cannot own the runtime installer process");}
    Need(ResumeThread(thread.h)!=DWORD(-1),"Cannot resume runtime installer");
    auto start=GetTickCount64();
    while(WaitForSingleObject(process.h,100)==WAIT_TIMEOUT){if(cancel||GetTickCount64()-start>timeout){
        TerminateJobObject(job.h,1);WaitForSingleObject(process.h,5000);throw std::runtime_error(cancel?"Runtime setup cancelled":"Runtime installer timed out; see its log");}}
    DWORD code=1;GetExitCodeProcess(process.h,&code);Need(code==0,"Runtime setup or validation failed; see the installation log");
}
}
Setup::Setup(fs::path src,fs::path data):source(std::move(src)),storage(std::move(data)){}
Setup::~Setup(){Cancel();if(task_.valid())task_.wait();}
std::string Setup::Fingerprint(const fs::path& source){
    cygi::Sha256 hash;hash.Update(PythonSha256,sizeof(PythonSha256)-1);
    for(auto name:{"PrepareRuntime.py","dependency-lock.json","requirements.lock.txt","app/CyTool.json","app/tool.py","app/bridge.py"}){
        const auto value=updates::HashFile(source/name);hash.Update(value.data(),value.size());}
    return hash.Finish().ToHex();
}
fs::path Setup::ReceiptPython(const fs::path& storage,const std::string& fingerprint){
    NoReparse(storage);auto j=Read(storage/"current.json");auto name=j["python"].AsString();
    Need(j["schema_version"].AsNumber()==1&&j["fingerprint"].AsString()==fingerprint,"Runtime dependencies changed; prepare the current version");
    Need(updates::SafeRelativePath(name)&&name.rfind("installs/",0)==0&&name.size()>18&&name.substr(name.size()-18)=="/python/python.exe","Invalid private Python path");
    auto file=storage/fs::u8path(name);NoReparse(file);Need(fs::is_regular_file(file),"Private Python is missing; reinstall the runtime");return file;
}
void Setup::ValidateExtracted(const fs::path& directory){
    uint64_t bytes=0,count=0;for(const auto& entry:fs::recursive_directory_iterator(directory)){
        NoReparse(entry.path());auto relative=entry.path().lexically_relative(directory).generic_u8string();
        Need(relative=="python"||relative.rfind("python/",0)==0,"Unexpected private Python archive root");
        Need(updates::SafeRelativePath(relative),"Unsafe private Python archive path");
        if(entry.is_directory())continue;Need(entry.is_regular_file(),"Unexpected private Python filesystem entry");
        bytes+=entry.file_size();Need(++count<30000&&bytes<1024ull*1024*1024,"Expanded Python runtime exceeds its budget");
    }
    Need(fs::is_regular_file(directory/"python/python.exe")&&fs::is_regular_file(directory/"python/LICENSE.txt"),"Python runtime or its license is missing");
}
bool Setup::Check(){return Start(false);}
bool Setup::Install(){return Start(true);}
bool Setup::Start(bool install){
    if(Busy()||task_.valid())return false;state=install?State::Installing:State::Checking;error.clear();log_tail.clear();phase=0;received=total=0;cancel_=false;
    try{Need(source.is_absolute()&&storage.is_absolute(),"Runtime directories must be absolute");NoReparse(storage/"logs");fs::create_directories(storage/"logs");
        log=storage/"logs"/(Id()+L".log");task_=std::async(std::launch::async,[this,install,journal=log]{return Work(install,journal);});return true;
    }catch(const std::exception& e){state=State::Failed;error=e.what();return false;}
}
Result Setup::Work(bool install,const fs::path& journal){
    try{
        NoReparse(storage);auto stamp=Fingerprint(source);fs::create_directories(storage);
        Handle lock{CreateFileW((storage/"setup.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
        Need(lock.h!=INVALID_HANDLE_VALUE,"Another CyInsights instance is preparing this runtime; retry when it finishes");
        if(!install){
            fs::path existing;
            try{existing=ReceiptPython(storage,stamp);}catch(...){
                auto legacy=source/"runtime/python/Scripts/python.exe";
                if(fs::is_regular_file(legacy))existing=legacy;
                else return {State::Missing,{},"The external MCP/SDK runtime is not installed"};
            }
            Note(journal,"Checking the complete MCP/SDK installation");
            Process(existing,{L"-I",(source/"PrepareRuntime.py").wstring(),L"verify",L"--source",source.wstring()},source,journal,cancel_,60000);
            return {State::Ready,existing,{}};
        }
        Note(journal,"Preparing private Python "+std::string(PythonVersion)+" and the supplied CyToolsCore/MCP wheels");
        auto cache=storage/"downloads";fs::create_directories(cache);auto archive=cache/(std::string(PythonSha256)+".tar.gz");NoReparse(archive);
        phase=1;total=PythonBytes;
        if(fs::exists(archive)){
            try{updates::VerifyFile(archive,PythonSha256,PythonBytes);received=PythonBytes;}
            catch(...){fs::rename(archive,cache/(Id()+L".invalid"));}
        }
        if(!fs::exists(archive)){
            updates::Release component;component.url_download=PythonUrl;component.sha256=PythonSha256;component.size=PythonBytes;
            updates::Download(component,archive,cancel_,[this](uint64_t n,uint64_t size){received=n;total=size;});
        }
        updates::VerifyFile(archive,PythonSha256,PythonBytes);
        if(cancel_)throw std::runtime_error("Runtime setup cancelled");
        phase=2;auto stage=storage/"installs"/Id();NoReparse(stage);Need(fs::create_directories(stage),"Runtime transaction directory already exists");
        wchar_t system[32768]{};Need(GetSystemDirectoryW(system,32768)!=0,"Windows system directory unavailable");auto tar=fs::path(system)/L"tar.exe";
        Need(fs::is_regular_file(tar),"Windows tar.exe is required to extract the private runtime");
        Note(journal,"Extracting the SHA-256-verified Python distribution");
        Process(tar,{L"-xf",archive.wstring(),L"-C",stage.wstring()},stage,journal,cancel_,120000);ValidateExtracted(stage);
        auto executable=stage/"python/python.exe";phase=3;Note(journal,"Installing the included wheels offline with required SHA-256 hashes");
        Process(executable,{L"-I",(source/"PrepareRuntime.py").wstring(),L"install",L"--source",source.wstring()},source,journal,cancel_,600000);
        phase=4;Note(journal,"Verifying imports, pinned versions and the complete operation contract");
        Process(executable,{L"-I",(source/"PrepareRuntime.py").wstring(),L"verify",L"--source",source.wstring()},source,journal,cancel_,60000);
        if(cancel_)throw std::runtime_error("Runtime setup cancelled");
        auto receipt=cygi::Json::Object();receipt["schema_version"]=1;receipt["fingerprint"]=stamp;receipt["python"]=executable.lexically_relative(storage).generic_u8string();receipt["python_version"]=PythonVersion;
        auto temporary=storage/(Id()+L".json");std::ofstream out(temporary,std::ios::binary);out<<receipt.Write(2);out.close();
        Need(bool(out)&&MoveFileExW(temporary.c_str(),(storage/"current.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"Cannot activate the verified runtime");
        phase=5;Note(journal,"MCP/SDK runtime ready");return {State::Ready,executable,{}};
    }catch(const std::exception& e){Note(journal,e.what());return {cancel_?State::Cancelled:State::Failed,{},e.what()};}
}
void Setup::Poll(){
    if(task_.valid()&&task_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        auto result=task_.get();state=result.state;python=std::move(result.python);error=std::move(result.error);}
    auto now=std::chrono::steady_clock::now();if(!log.empty()&&now-last_log_>std::chrono::milliseconds(250)){
        last_log_=now;std::ifstream in(log,std::ios::binary);if(in){in.seekg(0,std::ios::end);auto size=in.tellg();in.seekg(std::max<std::streamoff>(0,static_cast<std::streamoff>(size)-32768));log_tail.assign(std::istreambuf_iterator<char>(in),{});}}
}
}
