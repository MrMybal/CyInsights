// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiRpc.hpp"
#include <windows.h>
#include <sddl.h>
#include <chrono>
#include <algorithm>
namespace cyinsights {
cygi::Json AiError(const std::string& code,const std::string& message){auto out=cygi::Json::Object();out["error"]["code"]=code;out["error"]["message"]=message;return out;}
AiRpc::~AiRpc(){Stop();}
std::string AiRpc::Name() const{return name_;}
bool AiRpc::Start(std::string& error){
    if(Running())return true;
    HANDLE token=nullptr;DWORD size=0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){error="Cannot read Windows identity";return false;}
    GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<unsigned char> data(size);
    bool valid=GetTokenInformation(token,TokenUser,data.data(),size,&size)!=0;CloseHandle(token);
    LPWSTR sid=nullptr;if(!valid||!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&sid)){error="Cannot secure local AI pipe";return false;}
    std::wstring security=L"D:P(A;;GA;;;"+std::wstring(sid)+L")";LocalFree(sid);
    name_="\\\\.\\pipe\\CyInsights.AI."+std::to_string(GetCurrentProcessId());
    std::wstring name(name_.begin(),name_.end());running_=true;
    for(int i=0;i<4;++i)workers_.emplace_back([this,name,security]{Serve(name,security);});
    return true;
}
void AiRpc::Stop(){
    running_=false;
    for(auto& worker:workers_)CancelSynchronousIo(worker.native_handle());
    for(auto& worker:workers_)if(worker.joinable())worker.join();workers_.clear();
    std::lock_guard<std::mutex> lock(mutex_);for(auto& request:queue_)request->cancelled=true;queue_.clear();
}
std::vector<std::shared_ptr<AiRequest>> AiRpc::Take(){std::lock_guard<std::mutex> lock(mutex_);auto result=std::move(queue_);queue_.clear();return result;}
void AiRpc::Serve(std::wstring name,std::wstring security){
    PSECURITY_DESCRIPTOR descriptor=nullptr;
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(security.c_str(),SDDL_REVISION_1,&descriptor,nullptr))return;
    SECURITY_ATTRIBUTES attributes{sizeof(attributes),descriptor,FALSE};
    while(running_){
        HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_NOWAIT|PIPE_REJECT_REMOTE_CLIENTS,4,65536,65536,0,&attributes);
        if(pipe==INVALID_HANDLE_VALUE)break;
        bool connected=false;
        while(running_){if(ConnectNamedPipe(pipe,nullptr)||GetLastError()==ERROR_PIPE_CONNECTED){connected=true;break;}if(GetLastError()!=ERROR_PIPE_LISTENING)break;Sleep(5);}
        if(connected&&running_){
            std::string line;char buffer[4096];DWORD read=0;bool complete=false;
            auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(running_&&line.size()<=65536&&std::chrono::steady_clock::now()<deadline){
                if(!ReadFile(pipe,buffer,sizeof(buffer),&read,nullptr)){if(GetLastError()==ERROR_NO_DATA){Sleep(5);continue;}break;}
                if(!read){Sleep(5);continue;}line.append(buffer,read);if(line.find('\n')!=std::string::npos){complete=true;break;}
            }
            auto result=AiError("InvalidRequest","Expected one JSON request of at most 64 KiB");cygi::Json document;
            if(complete&&line.size()<=65536&&cygi::Json::Parse(line.substr(0,line.find('\n')),document)&&document.IsObject()){
                auto request=std::make_shared<AiRequest>();request->document=std::move(document);auto future=request->response.get_future();
                {std::lock_guard<std::mutex> lock(mutex_);queue_.push_back(request);}
                while(running_&&future.wait_for(std::chrono::milliseconds(10))!=std::future_status::ready){
                    DWORD available=0;if(!PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr)){request->cancelled=true;break;}
                }
                if(running_&&!request->cancelled&&future.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{result=future.get();}catch(...){result=AiError("InternalError","Native request failed");}}
                else request->cancelled=true;
            }
            auto text=result.Write(-1)+"\n";DWORD written=0;size_t offset=0;deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(running_&&offset<text.size()&&std::chrono::steady_clock::now()<deadline){
                DWORD available=0;if(!PeekNamedPipe(pipe,nullptr,0,nullptr,&available,nullptr))break;
                auto size=static_cast<DWORD>(std::min<size_t>(4096,text.size()-offset));
                if(!WriteFile(pipe,text.data()+offset,size,&written,nullptr)){if(GetLastError()==ERROR_NO_DATA){Sleep(5);continue;}break;}
                if(!written)Sleep(5);offset+=written;
            }
            if(offset==text.size()&&running_)FlushFileBuffers(pipe);
        }
        DisconnectNamedPipe(pipe);CloseHandle(pipe);
    }
    LocalFree(descriptor);
}
}
