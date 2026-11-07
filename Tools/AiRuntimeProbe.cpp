// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiRuntime.hpp"
#include <Windows.h>
#include <CyGPUInspectorCore/Json.hpp>
#include <iostream>
using namespace cyinsights::ai_runtime;
int wmain(int argc,wchar_t** argv){try{
    if(argc!=4)throw std::runtime_error("Usage: CyAiRuntimeProbe <AI source> <private storage> install|check|cancel");
    Setup setup(fs::absolute(argv[1]),fs::absolute(argv[2]));std::wstring mode=argv[3];
    // Prove that the integrated path does not need an installed Python or its environment.
    wchar_t system[32768]{};GetSystemDirectoryW(system,32768);SetEnvironmentVariableW(L"PATH",system);
    SetEnvironmentVariableW(L"PYTHONHOME",L"invalid-test-home");SetEnvironmentVariableW(L"PYTHONPATH",L"invalid-test-path");
    if(mode==L"check")setup.Check();else if(mode==L"install"||mode==L"cancel")setup.Install();else throw std::runtime_error("Unknown action");
    auto start=GetTickCount64();unsigned previous=99;
    while(setup.Busy()){
        setup.Poll();auto phase=setup.phase.load();if(phase!=previous){std::cout<<"phase "<<phase<<'\n'<<std::flush;previous=phase;}
        if(mode==L"cancel"&&setup.received>0)setup.Cancel();
        if(GetTickCount64()-start>900000)setup.Cancel();Sleep(20);
    }
    cygi::Json result=cygi::Json::Object();result["state"]=int(setup.state);result["ready"]=setup.state==State::Ready;result["python"]=setup.python.u8string();result["error"]=setup.error;result["log"]=setup.log.u8string();
    std::cout<<result.Write(2)<<'\n';return mode==L"cancel"?(setup.state==State::Cancelled?0:2):(setup.state==State::Ready?0:1);
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
