// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Capture.hpp"
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    std::string error; bool ok=false;
    if(argc==4 && std::wstring(argv[1])==L"pack") ok=cyinsights::Pack(argv[2],argv[3],error);
    else if(argc>=3 && std::wstring(argv[1])==L"inspect") {
        cyinsights::Capture c;ok=cyinsights::Inspect(argv[2],c,error);
        if(ok)std::cout<<c.manifest.Write(2)<<"\n"<<c.entries.size()<<" verified payloads\n";
    }else if(argc==4 && std::wstring(argv[1])==L"extract") {
        cyinsights::Capture c;ok=cyinsights::Inspect(argv[2],c,error)&&cyinsights::Extract(c,argv[3],error);
    }else error="Usage: CyCaptureTool pack <staging> <file.cycapture> | inspect <file> | extract <file> <new-directory>";
    if(!ok)std::cerr<<error<<"\n";return ok?0:1;
}
