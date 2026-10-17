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
        NativeSetup registered;registered.LoadRegistered();
        check(registered.installations.empty()&&!registered.Busy(),"first launch waits for an explicitly registered engine");
        std::string error;
        auto sibling=root/"UE_sibling/Engine/Binaries/Win64/UnrealInsights.exe";
        write(sibling,"fixture");write(AnalyzerEngine(sibling)/"Build/Build.version","{\"MajorVersion\":5,\"MinorVersion\":5}");
        check(registered.AddEngineRoot(root/"UE_fixture",error),"register installation root without choosing an executable");
        check(registered.installations.size()==1&&registered.selected==fs::canonical(analyzer),"sibling engines are never automatically discovered");
        check(registered.AddEngineRoot(AnalyzerEngine(analyzer),error)&&registered.installations.size()==1,"Engine subfolder resolves to the same registration");
        check(!registered.AddEngineRoot(root/"missing",error)&&!error.empty()&&registered.installations.size()==1,"invalid root explains failure and retains existing engines");
        check(registered.AddEngineRoot(NativeSetup::EngineRoot(sibling),error)&&registered.installations.size()==2,"multiple engines can be added explicitly");
        check(registered.SelectEngine(analyzer,error),"registered engine selection");
        NativeSetup reloaded;reloaded.LoadRegistered();wait(reloaded);
        check(reloaded.installations.size()==2&&reloaded.selected==fs::canonical(analyzer),"registered list and selection survive restart");
        check(reloaded.RemoveEngine(sibling,error)&&reloaded.installations.size()==1,"removing an engine changes only its registration");
        check(fs::exists(sibling),"removing a registration never deletes the engine installation");
        check(!reloaded.SelectEngine(sibling,error),"unregistered engines cannot silently become selected");
        NativeSetup afterRemoval;afterRemoval.LoadRegistered();wait(afterRemoval);
        check(afterRemoval.installations.size()==1,"removed engines stay removed after restart");
        check(NativeSetup::EngineRoot(analyzer)==fs::canonical(root/"UE_fixture"),"root shown in interface is installation folder");
        auto unicode=root/fs::u8path(u8"Moteur été/Engine/Binaries/Win64/UnrealInsights.exe");
        write(unicode,"fixture");write(AnalyzerEngine(unicode)/"Build/Build.version","{\"MajorVersion\":4,\"MinorVersion\":27}");
        check(registered.AddEngineRoot(NativeSetup::EngineRoot(unicode),error),"UE 4.27 and Unicode root are accepted for CSV reading");
        NativeSetup unicodeReload;unicodeReload.LoadRegistered();wait(unicodeReload);
        check(unicodeReload.selected==fs::canonical(unicode),"Unicode root selection survives persistence");
        check(registered.RemoveEngine(unicode,error),"remove selected engine");
        check(registered.selected==fs::canonical(analyzer),"selected engine removal chooses a remaining registration");
        check(registered.SelectEngine(analyzer,error),"restore selected fixture engine");
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
        check(fs::is_regular_file(NativeDataDirectory()/"engines.json"),"manual engine list persisted");
        write(app/"Tools/NativeTiming/Format.hpp","changed source");
        check(!NativeSetup::ReadersReady(analyzer,app),"changed source triggers preparation");
        check(FindNativeReader("Timing",analyzer,app)==app/"NativeTiming/5.8/CyNativeTiming.exe","obsolete cache cannot hide a newer bundled reader");
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
        NativeSetup legacy;legacy.LoadRegistered();wait(legacy);
        for(auto p=legacy.installations;p.size();p.pop_back())check(legacy.RemoveEngine(p.back(),error),"clear manual registrations");
        write(NativeDataDirectory()/"analyzer.json","{\"analyzer\":\"untrusted old automatic selection\"}");
        NativeSetup noLegacy;noLegacy.LoadRegistered();check(noLegacy.installations.empty()&&!noLegacy.Busy(),"old automatic discovery selection is not imported");
        auto invalid=root/"UE_invalid/Engine/Binaries/Win64/UnrealInsights.exe";write(invalid,"fixture");
        write(AnalyzerEngine(invalid)/"Build/Build.version","{\"MajorVersion\":5,\"MinorVersion\":9}");
        check(!noLegacy.AddEngineRoot(NativeSetup::EngineRoot(invalid),error)&&error.find("4.27")!=std::string::npos,"unsupported engine reports supported range");
        write(NativeDataDirectory()/"engines.json","invalid json");
        NativeSetup corrupt;corrupt.LoadRegistered();check(corrupt.state==NativeSetup::State::Failed&&!corrupt.message.empty(),"invalid settings have a visible error");
        check(corrupt.AddEngineRoot(NativeSetup::EngineRoot(analyzer),error),"manual addition repairs invalid settings");
        std::cout<<checks<<" native setup checks passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
