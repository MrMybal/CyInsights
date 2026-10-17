// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeSetup.hpp"
#include "NativeTrace.hpp"
#include <CyGPUInspectorCore/Json.hpp>
#include <CyGPUInspectorCore/Sha256.hpp>
#include <cmath>
namespace cyinsights {
namespace {
std::filesystem::path EnvironmentPath(const wchar_t* name) {
    wchar_t path[32768]{};auto n=GetEnvironmentVariableW(name,path,32768);
    return n&&n<32768?std::filesystem::path(path):std::filesystem::path();
}
struct Handle {HANDLE value=nullptr;~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct OwnedMutex {HANDLE value=nullptr;bool owned=false;~OwnedMutex(){if(owned)ReleaseMutex(value);if(value)CloseHandle(value);}};
}
NativeSetup::~NativeSetup(){Cancel();if(prepare_.valid())prepare_.wait();}
void NativeSetup::Cancel(){cancel_=true;}
namespace {
bool SameAnalyzer(const std::filesystem::path& a,const std::filesystem::path& b) {
    return _wcsicmp(a.lexically_normal().c_str(),b.lexically_normal().c_str())==0;
}
}
std::filesystem::path NativeSetup::EngineRoot(const std::filesystem::path& analyzer) {
    auto engine=AnalyzerEngine(analyzer);
    return _wcsicmp(engine.filename().c_str(),L"Engine")==0?engine.parent_path():engine;
}
std::filesystem::path NativeSetup::ResolveEngineRoot(const std::filesystem::path& root,std::string& error) {
    error.clear();
    try {
        if(root.empty())throw std::runtime_error("Enter the root folder of your Unreal engine.");
        auto engine=root;std::error_code ec;
        if(!std::filesystem::is_regular_file(engine/"Build/Build.version",ec))engine/=L"Engine";
        auto analyzer=engine/L"Binaries/Win64/UnrealInsights.exe";
        if(!std::filesystem::is_regular_file(analyzer,ec))throw std::runtime_error("UnrealInsights.exe is missing from this engine's Binaries/Win64 folder.");
        cygi::Json version;
        if(!cygi::Json::Parse(ReadNativeFile(engine/"Build/Build.version"),version)||!version.IsObject())
            throw std::runtime_error("The engine's Build/Build.version file is missing or invalid.");
        const auto major=version["MajorVersion"].AsNumber(-1),minor=version["MinorVersion"].AsNumber(-1);
        if(!((major==4&&minor==27)||(major==5&&minor>=0&&minor<=8&&minor==std::floor(minor))))
            throw std::runtime_error("Select an Unreal engine from UE 4.27 to UE 5.8.");
        return std::filesystem::canonical(analyzer);
    }catch(const std::exception& e){error=e.what();return {};}
}
bool NativeSetup::SaveRegistered(std::string& error) const {
    try {
        const auto directory=NativeDataDirectory();std::filesystem::create_directories(directory);
        auto config=cygi::Json::Object();config["version"]=1;config["engines"]=cygi::Json::Array();
        for(const auto& analyzer:installations)config["engines"].Push(AnalyzerEngine(analyzer).u8string());
        config["selected_engine"]=selected.empty()?std::string():AnalyzerEngine(selected).u8string();
        const auto temporary=directory/("engines-"+std::to_string(GetCurrentProcessId())+".tmp");
        std::ofstream out(temporary,std::ios::binary|std::ios::trunc);out<<config.Write(2);out.close();
        if(!out||!MoveFileExW(temporary.c_str(),(directory/"engines.json").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot save the registered Unreal engines.");
        error.clear();return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
bool NativeSetup::AddEngineRoot(const std::filesystem::path& root,std::string& error) {
    if(Busy()){error="Wait for reader preparation to finish before changing engines.";return false;}
    auto analyzer=ResolveEngineRoot(root,error);if(analyzer.empty())return false;
    auto previous=installations;auto previousSelected=selected;
    auto found=std::find_if(installations.begin(),installations.end(),[&](const auto& p){return SameAnalyzer(p,analyzer);});
    if(found==installations.end())installations.push_back(analyzer);else analyzer=*found;
    selected=analyzer;
    if(SaveRegistered(error))return true;
    installations=std::move(previous);selected=std::move(previousSelected);return false;
}
bool NativeSetup::SelectEngine(const std::filesystem::path& analyzer,std::string& error) {
    if(Busy()){error="Wait for reader preparation to finish before changing engines.";return false;}
    auto found=std::find_if(installations.begin(),installations.end(),[&](const auto& p){return SameAnalyzer(p,analyzer);});
    if(found==installations.end()){error="Add this engine's root folder in Session first.";return false;}
    auto previous=selected;selected=*found;
    if(SaveRegistered(error))return true;
    selected=std::move(previous);return false;
}
bool NativeSetup::RemoveEngine(const std::filesystem::path& analyzer,std::string& error) {
    if(Busy()){error="Wait for reader preparation to finish before changing engines.";return false;}
    auto previous=installations;auto previousSelected=selected;
    installations.erase(std::remove_if(installations.begin(),installations.end(),[&](const auto& p){return SameAnalyzer(p,analyzer);}),installations.end());
    if(SameAnalyzer(selected,analyzer))selected=installations.empty()?std::filesystem::path():installations.front();
    if(SaveRegistered(error)){if(selected.empty()){state=State::Idle;message="Add an Unreal engine root folder in Session to read native traces.";}return true;}
    installations=std::move(previous);selected=std::move(previousSelected);return false;
}
void NativeSetup::LoadRegistered(const std::filesystem::path& preferred) {
    if(Busy())return;
    installations.clear();selected.clear();state=State::Idle;
    try {
        const auto text=ReadNativeFile(NativeDataDirectory()/"engines.json");
        if(!text.empty()) {
            cygi::Json config;
            if(!cygi::Json::Parse(text,config)||!config.IsObject()||config["version"].AsNumber()!=1||!config["engines"].IsArray())
                throw std::runtime_error("The registered engines file is invalid. Add your engine root folder again in Session.");
            for(const auto& value:config["engines"].Items()) {
                auto engine=std::filesystem::u8path(value.AsString());if(!engine.is_absolute())continue;
                auto analyzer=(engine/"Binaries/Win64/UnrealInsights.exe").lexically_normal();
                if(std::none_of(installations.begin(),installations.end(),[&](const auto& p){return SameAnalyzer(p,analyzer);}))installations.push_back(analyzer);
            }
            auto chosen=std::filesystem::u8path(config["selected_engine"].AsString())/"Binaries/Win64/UnrealInsights.exe";
            for(const auto& analyzer:installations)if(SameAnalyzer(chosen,analyzer)){selected=analyzer;break;}
        }
        // An environment override is an explicit path; never probe the registry,
        // Epic Launcher inventory, sibling installations or whole drives.
        if(!preferred.empty()) {
            std::string error;if(!AddEngineRoot(AnalyzerEngine(preferred),error))throw std::runtime_error(error);
        }
        if(selected.empty()&&!installations.empty())selected=installations.front();
        if(selected.empty())message="Add an Unreal engine root folder in Session to read native traces.";
        else Prepare(selected);
    }catch(const std::exception& e){state=State::Failed;message=e.what();}
}
std::string NativeSetup::SourceStamp(const std::filesystem::path& app) {
    return NativeReaderSourceStamp(app);
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
    if(!std::filesystem::is_regular_file(analyzer,ec))return {State::Unavailable,"The registered engine's Unreal Insights executable is missing. Add its current root folder in Session."};
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
    auto now=std::chrono::steady_clock::now();
    if(state==State::Preparing&&now-last_log_>std::chrono::milliseconds(300)) {
        last_log_=now;std::ifstream in(log,std::ios::binary);if(in){in.seekg(0,std::ios::end);auto size=in.tellg();if(size>0){in.seekg(std::max<std::streamoff>(0,static_cast<std::streamoff>(size)-16384));log_tail={(std::istreambuf_iterator<char>(in)),{}};}}
        if(log_tail.find("Building NativeActivities")!=std::string::npos)phase=3;
        else if(log_tail.find("Building NativeTasks")!=std::string::npos)phase=2;
        else if(log_tail.find("Building NativeTiming")!=std::string::npos)phase=1;
    }
    if(prepare_.valid()&&prepare_.wait_for(std::chrono::seconds(0))==std::future_status::ready) {
        auto result=prepare_.get();state=result.state;message=std::move(result.message);log_tail=ReadNativeFile(log);if(log_tail.size()>16384)log_tail.erase(0,log_tail.size()-16384);

    }
}
}
