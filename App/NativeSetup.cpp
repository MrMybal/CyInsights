// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeSetup.hpp"
#include "NativeTrace.hpp"
#include <CyGPUInspectorCore/Json.hpp>
#include <CyGPUInspectorCore/Sha256.hpp>
#include <set>
namespace cyinsights {
namespace {
std::filesystem::path EnvironmentPath(const wchar_t* name) {
    wchar_t path[32768]{};auto n=GetEnvironmentVariableW(name,path,32768);
    return n&&n<32768?std::filesystem::path(path):std::filesystem::path();
}
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct OwnedMutex {HANDLE value=nullptr;bool owned=false;~OwnedMutex(){if(owned)ReleaseMutex(value);if(value)CloseHandle(value);}};
}
NativeSetup::~NativeSetup(){Cancel();if(discover_.valid())discover_.wait();if(prepare_.valid())prepare_.wait();}
void NativeSetup::Cancel(){cancel_=true;}
std::vector<std::filesystem::path> NativeSetup::Detect(const std::filesystem::path& preferred) {
    std::vector<std::filesystem::path> paths;
    auto add=[&](std::filesystem::path path){
        if(path.empty())return;
        if(path.filename()!=L"UnrealInsights.exe")path/=L"Engine/Binaries/Win64/UnrealInsights.exe";
        std::error_code ec;if(!std::filesystem::is_regular_file(path,ec))return;
        path=std::filesystem::weakly_canonical(path,ec);
        if(std::find(paths.begin(),paths.end(),path)==paths.end())paths.push_back(path);
    };
    add(preferred);
    try {cygi::Json config;if(cygi::Json::Parse(ReadNativeFile(NativeDataDirectory()/"analyzer.json"),config))add(std::filesystem::u8path(config["analyzer"].AsString()));}catch(...){}
    for(const auto hive:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER}) {
        HKEY key=nullptr;
        if(RegOpenKeyExW(hive,L"SOFTWARE\\EpicGames\\Unreal Engine",0,KEY_READ,&key)!=ERROR_SUCCESS)continue;
        wchar_t name[512]{};DWORD length=512;
        for(DWORD index=0;RegEnumKeyExW(key,index,name,&length,nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;++index,length=512) {
            HKEY child=nullptr;if(RegOpenKeyExW(key,name,0,KEY_READ,&child)!=ERROR_SUCCESS)continue;
            wchar_t path[32768]{};DWORD bytes=sizeof(path);
            if(RegGetValueW(child,nullptr,L"InstalledDirectory",RRF_RT_REG_SZ,nullptr,path,&bytes)==ERROR_SUCCESS)add(path);
            if(wcscmp(name,L"Builds")==0) {
                wchar_t label[512]{};DWORD labelLength=512,type=0;
                for(DWORD i=0;;++i) {
                    labelLength=512;bytes=sizeof(path);
                    if(RegEnumValueW(child,i,label,&labelLength,nullptr,&type,reinterpret_cast<BYTE*>(path),&bytes)!=ERROR_SUCCESS)break;
                    if(type==REG_SZ&&bytes>=sizeof(wchar_t)&&bytes<sizeof(path)){path[bytes/sizeof(wchar_t)]=0;add(path);}
                }
            }
            RegCloseKey(child);
        }
        RegCloseKey(key);
    }
    auto programData=EnvironmentPath(L"ProgramData");
    if(!programData.empty()) {
        cygi::Json launcher;
        if(cygi::Json::Parse(ReadNativeFile(programData/"Epic/UnrealEngineLauncher/LauncherInstalled.dat"),launcher))
            for(const auto& entry:launcher["InstallationList"].Items())if(entry["AppName"].AsString().rfind("UE_",0)==0)add(std::filesystem::u8path(entry["InstallLocation"].AsString()));
    }
    // Include sibling engines of known installations, without searching whole drives.
    auto known=paths;
    for(const auto& path:known) {
        std::error_code ec;auto parent=AnalyzerEngine(path).parent_path().parent_path();
        for(std::filesystem::directory_iterator it(parent,ec),end;!ec&&it!=end;it.increment(ec))
            if(it->path().filename().wstring().rfind(L"UE_",0)==0)add(it->path());
    }
    std::filesystem::path first;
    if(!preferred.empty()){std::error_code ec;auto path=std::filesystem::weakly_canonical(preferred,ec);if(std::find(paths.begin(),paths.end(),path)!=paths.end())first=path;}
    if(first.empty())try{cygi::Json config;if(cygi::Json::Parse(ReadNativeFile(NativeDataDirectory()/"analyzer.json"),config)){auto saved=std::filesystem::u8path(config["analyzer"].AsString());if(std::find(paths.begin(),paths.end(),saved)!=paths.end())first=saved;}}catch(...){}
    std::sort(paths.begin(),paths.end(),[&](const auto& a,const auto& b){if(a==b)return false;if(a==first)return true;if(b==first)return false;auto av=AnalyzerVersion(a),bv=AnalyzerVersion(b);if(av!=bv)return av>bv;return a.native()<b.native();});
    return paths;
}
std::string NativeSetup::SourceStamp(const std::filesystem::path& app) {
    cygi::Sha256 hash;
    for(const auto* part:{"InstallNativeReaders.ps1","NativeTiming","NativeTasks","NativeActivities"}) {
        auto path=app/"Tools"/part;
        std::vector<std::filesystem::path> files;std::error_code ec;
        if(std::filesystem::is_regular_file(path,ec))files.push_back(path);
        else if(std::filesystem::is_directory(path,ec))for(const auto& entry:std::filesystem::recursive_directory_iterator(path))if(entry.is_regular_file())files.push_back(entry.path());
        std::sort(files.begin(),files.end());
        for(const auto& file:files){auto name=file.lexically_relative(app).generic_u8string(),bytes=ReadNativeFile(file);hash.Update(name.data(),name.size());hash.Update(bytes.data(),bytes.size());}
    }
    return hash.Finish().ToHex();
}
bool NativeSetup::ReadersReady(const std::filesystem::path& analyzer,const std::filesystem::path& app) {
    auto cache=NativeCacheDirectory(analyzer);std::error_code ec;
    bool local=true,cached=ReadNativeFile(cache/"ready.txt")==SourceStamp(app);
    for(const auto* name:{"Timing","Tasks","Activities"}) {
        auto relative=std::filesystem::path(std::string("Native")+name)/AnalyzerVersion(analyzer)/(std::string("CyNative")+name+".exe");
        local&=std::filesystem::is_regular_file(app/relative,ec);ec.clear();cached&=std::filesystem::is_regular_file(cache/relative,ec);ec.clear();
    }
    return local||cached;
}
void NativeSetup::Discover(const std::filesystem::path& preferred) {
    if(Busy())return;cancel_=false;state=State::Discovering;message="Detecting installed Unreal analyzers...";started_=std::chrono::steady_clock::now();
    discover_=std::async(std::launch::async,[preferred]{return Detect(preferred);});
}
void NativeSetup::Prepare(const std::filesystem::path& analyzer,const std::filesystem::path& app) {
    if(Busy())return;selected=analyzer;cancel_=false;state=State::Preparing;phase=0;log_tail.clear();message="Preparing native trace readers...";started_=last_log_=std::chrono::steady_clock::now();
    try {
        auto cache=NativeCacheDirectory(analyzer);log=cache/"setup.log";
        prepare_=std::async(std::launch::async,[this,analyzer,app,cache,journal=log]{try{return Build(analyzer,app,cache,journal);}catch(const std::exception& error){return Result{State::Failed,error.what()};}});
    }catch(const std::exception& e){state=State::Failed;message=e.what();}
}
NativeSetup::Result NativeSetup::Build(const std::filesystem::path& analyzer,const std::filesystem::path& app,
    const std::filesystem::path& destination,const std::filesystem::path& journal) {
    std::error_code ec;
    if(!std::filesystem::is_regular_file(analyzer,ec))return {State::Unavailable,"Select your installed UnrealInsights.exe in Session."};
    auto version=AnalyzerVersion(analyzer);
    if(version!="5.5"&&version!="5.6"&&version!="5.7"&&version!="5.8")return {State::Unavailable,"Native readers support installed UE 5.5-5.8. The CSV timing reader remains available."};
    if(ReadersReady(analyzer,app))return {State::Ready,"Native trace readers ready."};
    auto script=app/"Tools/InstallNativeReaders.ps1";
    if(!std::filesystem::is_regular_file(script,ec))return {State::Failed,"Reader sources are missing. Extract the complete Windows release archive."};
    std::filesystem::create_directories(destination);
    OwnedMutex mutex;auto mutexName=L"Local\\CyInsightsNativeReaders_"+std::filesystem::path(NativeCacheKey(analyzer)).wstring();
    mutex.value=CreateMutexW(nullptr,FALSE,mutexName.c_str());if(!mutex.value)throw std::runtime_error("Cannot coordinate native reader preparation");
    while(!cancel_) {
        auto status=WaitForSingleObject(mutex.value,100);
        if(status==WAIT_OBJECT_0||status==WAIT_ABANDONED){mutex.owned=true;break;}
        if(status!=WAIT_TIMEOUT)throw std::runtime_error("Cannot lock native reader preparation");
    }
    if(cancel_)return {State::Cancelled,"Preparation cancelled. CSV timing reading remains available."};
    if(ReadersReady(analyzer,app))return {State::Ready,"Native trace readers ready."};
    auto staging=destination/("build-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(staging);
    auto powershell=EnvironmentPath(L"SystemRoot")/"System32/WindowsPowerShell/v1.0/powershell.exe";
    std::wstring command=NativeTrace::Quote(powershell.wstring())+L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+NativeTrace::Quote(script.wstring())+L" -Engine "+NativeTrace::Quote(AnalyzerEngine(analyzer).wstring())+L" -Destination "+NativeTrace::Quote(staging.wstring());
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
    Handle output{CreateFileW(journal.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
    Handle input{CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    if(output.value==INVALID_HANDLE_VALUE||input.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot open native preparation log");
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdOutput=startup.StartupInfo.hStdError=output.value;startup.StartupInfo.hStdInput=input.value;
    SIZE_T attributeBytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&attributeBytes);
    std::vector<unsigned char> attributes(attributeBytes);startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if(!InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&attributeBytes))throw std::runtime_error("Cannot initialize native preparation process");
    HANDLE inherited[]{output.value,input.value};
    bool updated=UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)!=FALSE;
    Handle job{CreateJobObjectW(nullptr,nullptr)};JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    bool configured=job.value&&SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&limits,sizeof(limits));
    PROCESS_INFORMATION process{};
    bool launched=updated&&configured&&CreateProcessW(powershell.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|CREATE_SUSPENDED|EXTENDED_STARTUPINFO_PRESENT,nullptr,app.c_str(),&startup.StartupInfo,&process);
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    if(!launched)throw std::runtime_error("Cannot start native preparation (Windows error "+std::to_string(GetLastError())+")");
    Handle child{process.hProcess},thread{process.hThread};
    if(!AssignProcessToJobObject(job.value,child.value)){TerminateProcess(child.value,1);throw std::runtime_error("Cannot supervise the native build process");}
    if(ResumeThread(thread.value)==DWORD(-1)){TerminateJobObject(job.value,1);throw std::runtime_error("Cannot run the native build process");}
    while(WaitForSingleObject(child.value,100)==WAIT_TIMEOUT) {
        if(cancel_){TerminateJobObject(job.value,1);WaitForSingleObject(child.value,5000);return {State::Cancelled,"Preparation cancelled. CSV timing reading remains available."};}
    }
    DWORD code=1;GetExitCodeProcess(child.value,&code);
    if(code!=0)return {State::Failed,"Native reader preparation failed. Check the log below; Visual Studio C++/CMake tools and licensed UE headers/libraries are required. CSV timing reading remains available."};
    if(cancel_)return {State::Cancelled,"Preparation cancelled. CSV timing reading remains available."};
    for(const auto* name:{"Timing","Tasks","Activities"}) {
        auto relative=std::filesystem::path(std::string("Native")+name)/version/(std::string("CyNative")+name+".exe");
        if(!std::filesystem::is_regular_file(staging/relative))throw std::runtime_error("Preparation did not produce every native reader");
    }
    // Publish only a complete successful build. Cancelled/failed files stay in staging.
    std::filesystem::remove(destination/"ready.txt",ec);
    for(const auto* name:{"Timing","Tasks","Activities"}) {
        auto relative=std::filesystem::path(std::string("Native")+name)/version;
        std::filesystem::create_directories(destination/relative);
        for(const auto& entry:std::filesystem::directory_iterator(staging/relative))if(entry.is_regular_file())std::filesystem::copy_file(entry.path(),destination/relative/entry.path().filename(),std::filesystem::copy_options::overwrite_existing);
    }
    std::ofstream ready(destination/"ready.txt",std::ios::binary);ready<<SourceStamp(app);ready.close();if(!ready)throw std::runtime_error("Cannot save native reader preparation state");
    return {State::Ready,"Native trace readers ready. Preparation is reused on subsequent launches."};
}
void NativeSetup::Poll() {
    if(discover_.valid()&&discover_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        try{installations=discover_.get();state=State::Idle;if(cancel_){state=State::Cancelled;message="Analyzer detection cancelled.";}else if(installations.empty()){state=State::Unavailable;message="Select your installed UnrealInsights.exe in Session.";}else Prepare(installations.front());}
        catch(const std::exception& e){state=State::Failed;message=e.what();}
    }
    auto now=std::chrono::steady_clock::now();
    if(state==State::Preparing&&now-last_log_>std::chrono::milliseconds(300)) {
        last_log_=now;std::ifstream in(log,std::ios::binary);if(in){in.seekg(0,std::ios::end);auto size=in.tellg();if(size>0){in.seekg(std::max<std::streamoff>(0,static_cast<std::streamoff>(size)-16384));log_tail={(std::istreambuf_iterator<char>(in)),{}};}}
        if(log_tail.find("Building NativeActivities")!=std::string::npos)phase=3;
        else if(log_tail.find("Building NativeTasks")!=std::string::npos)phase=2;
        else if(log_tail.find("Building NativeTiming")!=std::string::npos)phase=1;
    }
    if(prepare_.valid()&&prepare_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto result=prepare_.get();state=result.state;message=std::move(result.message);log_tail=ReadNativeFile(log);if(log_tail.size()>16384)log_tail.erase(0,log_tail.size()-16384);
        if(state==State::Ready||state==State::Unavailable)try{std::error_code ec;if(std::filesystem::is_regular_file(selected,ec)){std::filesystem::create_directories(NativeDataDirectory());cygi::Json config=cygi::Json::Object();config["analyzer"]=selected.u8string();std::ofstream out(NativeDataDirectory()/"analyzer.json");out<<config.Write();}}catch(...){}
    }
}
}
