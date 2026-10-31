// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ReleaseUpdate.hpp"
#include <Windows.h>
#include <winhttp.h>
#include <objbase.h>
#include <array>
#include <fstream>
#include <stdexcept>
namespace cyinsights::updates {
namespace {
struct Handle {HINTERNET h=nullptr;~Handle(){if(h)WinHttpCloseHandle(h);}operator HINTERNET()const{return h;}};
void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(std::string(what)+" (Windows error "+std::to_string(GetLastError())+")");}
std::wstring Wide(const std::string& s){return std::wstring(s.begin(),s.end());}
void Read(const std::string& initial,std::atomic_bool& cancel,uint64_t limit,const std::function<void(const char*,size_t)>& output){
    Handle session{WinHttpOpen(L"CyInsights-Updater/1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    Check(session.h!=nullptr,"Cannot initialize HTTPS");WinHttpSetTimeouts(session,5000,5000,10000,10000);
    std::string url=initial;
    for(int redirect=0;redirect<6;++redirect){
        if(cancel)throw std::runtime_error("Update cancelled");
        if(!AllowedHttps(url))throw std::runtime_error("Refusing an unexpected download host or insecure URL");
        auto full=Wide(url);URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
        Check(WinHttpCrackUrl(full.c_str(),0,0,&parts)!=FALSE,"Invalid update URL");
        std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
        Handle connection{WinHttpConnect(session,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0)};Check(connection.h!=nullptr,"Cannot connect to GitHub");
        Handle request{WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};Check(request.h!=nullptr,"Cannot create update request");
        DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;Check(WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))!=FALSE,"Cannot restrict HTTPS redirects");
        DWORD auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;WinHttpSetOption(request,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth));
        const wchar_t* headers=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
        Check(WinHttpSendRequest(request,headers,DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)!=FALSE,"Cannot send update request");
        Check(WinHttpReceiveResponse(request,nullptr)!=FALSE,"Cannot read GitHub response");
        DWORD status=0,length=sizeof(status);Check(WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&length,WINHTTP_NO_HEADER_INDEX)!=FALSE,"Cannot read HTTP status");
        if(status==301||status==302||status==303||status==307||status==308){
            wchar_t location[16384]{};length=sizeof(location);Check(WinHttpQueryHeaders(request,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location,&length,WINHTTP_NO_HEADER_INDEX)!=FALSE,"Cannot read update redirect");
            std::wstring next(location);url.assign(next.begin(),next.end());continue;
        }
        if(status!=200)throw std::runtime_error("GitHub returned HTTP "+std::to_string(status)+(status==403||status==429?". Rate limit reached; try again later.":status==404?". No public stable release is available.":"."));
        std::array<char,65536> buffer;uint64_t received=0;
        for(;;){
            if(cancel)throw std::runtime_error("Update cancelled");DWORD count=0;
            Check(WinHttpReadData(request,buffer.data(),DWORD(buffer.size()),&count)!=FALSE,"Update download failed");if(!count)break;
            received+=count;if(received>limit)throw std::runtime_error("Update response exceeds its declared size");output(buffer.data(),count);
        }return;
    }throw std::runtime_error("Too many GitHub redirects");
}
}
cygi::Json FetchLatest(std::atomic_bool& cancel){
    std::string text;Read(Api,cancel,2*1024*1024,[&](const char* p,size_t n){text.append(p,n);});cygi::Json value;
    if(!cygi::Json::Parse(text,value)||!value.IsObject())throw std::runtime_error("GitHub returned invalid release metadata");return value;
}
void Download(const Release& release,const fs::path& destination,std::atomic_bool& cancel,Progress progress){
    auto partial=destination;partial+=L".partial";
    try{
        fs::create_directories(destination.parent_path());std::ofstream out(partial,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("Cannot create update download");uint64_t received=0;
        Read(release.url_download,cancel,release.size,[&](const char* p,size_t n){out.write(p,n);if(!out)throw std::runtime_error("Cannot save update download");received+=n;if(progress)progress(received,release.size);});
        out.close();VerifyFile(partial,release.sha256,release.size);if(cancel)throw std::runtime_error("Update cancelled");fs::rename(partial,destination);
    }catch(...){std::error_code ignored;fs::remove(partial,ignored);throw;}
}
fs::path NewWorkDirectory(){
    wchar_t local[32768]{};auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,DWORD(std::size(local)));
    if(!count||count>=std::size(local)||!fs::path(local).is_absolute())throw std::runtime_error("Local application data is unavailable");
    GUID guid{};Check(SUCCEEDED(CoCreateGuid(&guid)),"Cannot create update transaction");wchar_t id[40]{};StringFromGUID2(guid,id,int(std::size(id)));
    auto path=fs::path(local)/L"CyInsights"/L"Updates"/id;if(!fs::create_directories(path))throw std::runtime_error("Update transaction already exists");return path;
}
}
