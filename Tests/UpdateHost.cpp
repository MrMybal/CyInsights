// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Dedicated self-update process fixture; never distributed with the application.
#include <Windows.h>
#include <filesystem>
#include <fstream>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR args,int){
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,DWORD(std::size(exe)));auto root=std::filesystem::path(exe).parent_path();
    if(std::wstring(args)==L"--wait"){
        std::ofstream(root/L"parent-ready.txt")<<"ready";auto start=GetTickCount64();
        while(!std::filesystem::exists(root/L"finish-request.txt")&&GetTickCount64()-start<30000)Sleep(50);
    }else std::ofstream(root/L"restart-proof.txt")<<"restarted";
    return 0;
}
