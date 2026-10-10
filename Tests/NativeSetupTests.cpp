// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeSetup.hpp"
#include <iostream>
#include <thread>
using namespace cyinsights;
namespace fs=std::filesystem;
int main() {
    unsigned checks=0;
    auto check=[&](bool condition,const char* name){++checks;if(!condition)throw std::runtime_error(name);};
    auto root=fs::current_path()/"native-setup-tests"/std::to_string(GetCurrentProcessId());
    fs::create_directories(root/"profile");SetEnvironmentVariableW(L"LOCALAPPDATA",(root/"profile").c_str());
    auto write=[](const fs::path& file,const std::string& value){fs::create_directories(file.parent_path());std::ofstream out(file,std::ios::binary);out<<value;};
    auto analyzer=root/"UE_fixture/Engine/Binaries/Win64/UnrealInsights.exe";
    write(analyzer,"fixture");write(AnalyzerEngine(analyzer)/"Build/Build.version","{\"MajorVersion\":5,\"MinorVersion\":8}");
    auto app=root/"app";fs::create_directories(app);
    auto wait=[&](NativeSetup& setup){auto begin=std::chrono::steady_clock::now();while(setup.Busy()){setup.Poll();if(std::chrono::steady_clock::now()-begin>std::chrono::seconds(15))throw std::runtime_error("setup timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));}};
    try {
        check(AnalyzerVersion(analyzer)=="5.8","analyzer version");
        auto paths=NativeSetup::Detect(analyzer);check(!paths.empty()&&paths.front()==fs::weakly_canonical(analyzer),"explicit analyzer priority");
        auto cache=NativeCacheDirectory(analyzer);
        write(app/"Tools/InstallNativeReaders.ps1","param($Engine,$Destination)\nStart-Sleep -Seconds 10\n");
        auto stamp=NativeSetup::SourceStamp(app);
        check(!NativeSetup::ReadersReady(analyzer,app),"empty cache unavailable");
        for(const auto* name:{"Timing","Tasks","Activities"})write(cache/(std::string("Native")+name)/"5.8"/(std::string("CyNative")+name+".exe"),"fixture");
        check(!NativeSetup::ReadersReady(analyzer,app),"partial build not ready");
        check(FindNativeReader("Timing",analyzer,app).parent_path().parent_path().parent_path()==app,"unpublished cache ignored");
        write(cache/"ready.txt",stamp);
        check(NativeSetup::ReadersReady(analyzer,app),"complete cache ready");
        check(FindNativeReader("Timing",analyzer,app)==cache/"NativeTiming/5.8/CyNativeTiming.exe","timing uses cached reader");
        check(FindNativeReader("Tasks",analyzer,app)==cache/"NativeTasks/5.8/CyNativeTasks.exe","tasks use cached reader");
        check(FindNativeReader("Activities",analyzer,app)==cache/"NativeActivities/5.8/CyNativeActivities.exe","activities use cached reader");
        NativeSetup ready;ready.Prepare(analyzer,app);wait(ready);check(ready.state==NativeSetup::State::Ready,"no compiler needed for valid cache");
        check(fs::is_regular_file(NativeDataDirectory()/"analyzer.json"),"analyzer selection persisted");
        auto saved=NativeSetup::Detect();check(!saved.empty()&&saved.front()==fs::weakly_canonical(analyzer),"saved selection priority");
        write(app/"Tools/NativeTiming/Format.hpp","changed source");
        check(!NativeSetup::ReadersReady(analyzer,app),"changed source triggers preparation");
        auto originalKey=NativeCacheKey(analyzer);
        write(AnalyzerEngine(analyzer)/"Build/Build.version","{\"MajorVersion\":5,\"MinorVersion\":8,\"Changelist\":123}");
        check(NativeCacheKey(analyzer)!=originalKey,"engine update separates cache");
        auto other=root/"other/Engine/Binaries/Win64/UnrealInsights.exe";
        write(other,"fixture");write(AnalyzerEngine(other)/"Build/Build.version",ReadNativeFile(AnalyzerEngine(analyzer)/"Build/Build.version"));
        check(NativeCacheKey(analyzer)!=NativeCacheKey(other),"engine location separates cache");
        NativeSetup missing;missing.Prepare(root/"missing.exe",app);wait(missing);check(missing.state==NativeSetup::State::Unavailable,"missing analyzer explicit");
        write(AnalyzerEngine(other)/"Build/Build.version","{\"MajorVersion\":4,\"MinorVersion\":27}");
        NativeSetup old;old.Prepare(other,app);wait(old);check(old.state==NativeSetup::State::Unavailable,"old analyzer retains CSV");
        NativeSetup sources;sources.Prepare(analyzer,root/"no-sources");wait(sources);check(sources.state==NativeSetup::State::Failed&&sources.message.find("sources")!=std::string::npos,"missing sources explicit");
        NativeSetup cancelled;cancelled.Prepare(analyzer,app);
        auto begin=std::chrono::steady_clock::now();
        while(!fs::is_regular_file(cancelled.log)&&std::chrono::steady_clock::now()-begin<std::chrono::seconds(5)){cancelled.Poll();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
        cancelled.Cancel();wait(cancelled);check(cancelled.state==NativeSetup::State::Cancelled,"build process cancellation");
        check(!fs::exists(NativeCacheDirectory(analyzer)/"ready.txt"),"cancelled build not published");
        write(app/"Tools/InstallNativeReaders.ps1","param($Engine,$Destination)\nWrite-Output 'Fixture failure'\nexit 7\n");
        NativeSetup failed;failed.Prepare(analyzer,app);wait(failed);check(failed.state==NativeSetup::State::Failed,"build failure explicit");
        check(failed.log_tail.find("Fixture failure")!=std::string::npos,"build log visible");
        check(!fs::exists(NativeCacheDirectory(analyzer)/"ready.txt"),"failed build not published");
        std::cout<<checks<<" native setup checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
