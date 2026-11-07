// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiRuntime.hpp"
#include <Windows.h>
#include <fstream>
#include <iostream>
using namespace cyinsights::ai_runtime;
namespace {
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void Reject(F f,const char* message){bool failed=false;try{f();}catch(...){failed=true;}Check(failed,message);}
void Write(const fs::path& file,const std::string& text){fs::create_directories(file.parent_path());std::ofstream(file)<<text;}
}
int main(){auto root=fs::temp_directory_path()/("CyInsights-runtime-unit-"+std::to_string(GetCurrentProcessId()));
try{
    auto source=root/"source",storage=root/"private";fs::create_directories(storage);
    for(auto name:{"PrepareRuntime.py","dependency-lock.json","requirements.lock.txt","app/CyTool.json","app/tool.py","app/bridge.py"})Write(source/name,"original");
    auto first=Setup::Fingerprint(source);Write(source/"app/tool.py","changed");Check(first!=Setup::Fingerprint(source),"Changed adapter must require verification");
    Reject([&]{Setup::ReceiptPython(storage,first);},"Missing runtime cannot be ready");
    for(auto path:{"../outside/python/python.exe","C:/outside/python/python.exe","installs/../outside/python/python.exe","installs/id/python/other.exe"}){
        Write(storage/"current.json",std::string("{\"schema_version\":1,\"fingerprint\":\"")+first+"\",\"python\":\""+path+"\"}");
        Reject([&]{Setup::ReceiptPython(storage,first);},"Unowned interpreter path accepted");
    }
    Write(storage/"installs/id/python/python.exe","fixture");
    Write(storage/"current.json",std::string("{\"schema_version\":1,\"fingerprint\":\"")+first+"\",\"python\":\"installs/id/python/python.exe\"}");
    Check(Setup::ReceiptPython(storage,first)==storage/"installs/id/python/python.exe","Valid private runtime receipt rejected");
    Reject([&]{Setup::ReceiptPython(storage,"different");},"Stale runtime receipt accepted");
    Reject([&]{Setup::ValidateExtracted(storage/"installs/id");},"Runtime without license accepted");
    Write(storage/"installs/id/python/LICENSE.txt","fixture license");Setup::ValidateExtracted(storage/"installs/id");
    Write(storage/"installs/id/unexpected.txt","fixture");Reject([&]{Setup::ValidateExtracted(storage/"installs/id");},"Unexpected archive root accepted");
    {Setup setup(source,root/"empty-profile");Check(setup.Check(),"Cannot start missing runtime check");while(setup.Busy()){Sleep(10);setup.Poll();}Check(setup.state==State::Missing,"Missing runtime not reported");}
    fs::remove_all(root);std::cout<<"Runtime path, ownership, freshness, extraction and missing-installation checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
