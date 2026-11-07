// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Core/AppPaths.hpp"
#include <fstream>
#include <iostream>
#include <string>
int main(){
    namespace fs=std::filesystem;
    auto root=fs::temp_directory_path()/("CyInsights-paths-"+std::to_string(GetCurrentProcessId()));
    try {
        auto app=root/L"portable application",local=root/L"profile";
        fs::create_directories(app);
        if(cyinsights::ResolveUserDataDirectory(app,local)!=local/L"CyInsights")return 1;
        std::ofstream(app/L"cyinsights-portable.txt")<<"Portable mode\n";
        if(cyinsights::ResolveUserDataDirectory(app,local)!=app/L"Data")return 2;
        if(cyinsights::ResolveUserDataDirectory(app,{})!=app/L"Data")return 3;
        std::ofstream(app/L"Data-user.txt")<<"keep";
        fs::remove(app/L"cyinsights-portable.txt");
        if(cyinsights::ResolveUserDataDirectory(app,local)!=local/L"CyInsights")return 4;
        bool rejected=false;try{cyinsights::ResolveUserDataDirectory(app,"relative");}catch(...){rejected=true;}
        if(!rejected)return 5;
        fs::create_directories(app/L"cyinsights-portable.txt");
        if(cyinsights::ResolveUserDataDirectory(app,local)!=local/L"CyInsights")return 6;
        fs::remove(app/L"cyinsights-portable.txt");fs::remove(app/L"Data-user.txt");fs::remove(app);fs::remove(root);
        std::cout<<"6 application path checks passed\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 10;}
}
