// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiJobs.hpp"
#include <regex>
#include <windows.h>
namespace cyinsights {
using J=NativeJson;
namespace {
std::string Owner(const cymcp::CallContext& c){return cymcp::sha256(c.principal);}
uint64_t ProcessEpoch(DWORD pid){
    HANDLE process=pid?OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid):nullptr;if(!process)return 0;
    FILETIME created{},exit{},kernel{},user{};const bool alive=WaitForSingleObject(process,0)==WAIT_TIMEOUT&&GetProcessTimes(process,&created,&exit,&kernel,&user);CloseHandle(process);
    return alive?(uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime:0;
}
}
AiJobs::AiJobs(std::filesystem::path p,NativeHandler h,std::shared_ptr<cymcp::ResourceRegistry> r):root_(std::move(p)),invoke_(std::move(h)),resources_(std::move(r)){
    cymcp::private_directory(root_);
    for(const auto& f:std::filesystem::directory_iterator(root_))if(f.path().extension()==".json")try{
        auto j=ReadPrivateJson(f.path());const auto epoch=ProcessEpoch(j.value("pid",DWORD(0)));
        const bool alive=epoch&&epoch==j.value("processEpoch",uint64_t(0));
        if(!alive&&(j.value("state","")=="running"||j.value("state","")=="queued")){
            auto result=f.path();result.replace_extension(".result");
            if(std::filesystem::is_regular_file(result)&&std::filesystem::file_size(result)<=64*1024*1024){j["state"]="completed";j["recovered"]=true;j["bytes"]=std::filesystem::file_size(result);}
            else {j["state"]="interrupted";j["error"]={{"code","Interrupted"},{"message","Application closed before completion; no action was replayed"}};}
            WritePrivateJson(f.path(),j);
        }
    }catch(...){} // An invalid old record never prevents opening the application.
}
AiJobs::~AiJobs(){CancelAll();}
void AiJobs::CancelAll(){lifetime_.cancel();std::lock_guard lock(mutex_);for(auto& [id,j]:live_)j->context.cancellation.cancel();}
J AiJobs::Load(const std::string& id,const cymcp::CallContext& c){
    if(!std::regex_match(id,std::regex("[a-f0-9]{32}")))throw cymcp::Error("InvalidArguments","Invalid job ID");
    auto found=live_.find(id);auto j=found==live_.end()?ReadPrivateJson(root_/(id+".json")):found->second->metadata;if(j.value("owner","")!=Owner(c))throw cymcp::Error("AccessDenied","Job unavailable for this identity");return j;
}
void AiJobs::Register(cymcp::Registry& r){
    auto obj=J{{"type","object"},{"additionalProperties",false}};
    auto id=obj;id["properties"]={{"id",{{"type","string"},{"minLength",32},{"maxLength",32}}}};id["required"]={"id"};
    auto start=obj;start["properties"]={{"operation",{{"type","string"}}},{"arguments",{{"type","object"}}}};start["required"]={"operation","arguments"};
    r.replace({"job_start","Run an operation asynchronously and retain its result across restarts. Does not replay interrupted work.",start,{{"type","object"}},false,[this](const J& p,const cymcp::CallContext& c){
        auto op=p.at("operation").get<std::string>();if(op.starts_with("job_"))throw cymcp::Error("InvalidArguments","Nested jobs are unavailable");
        // Bound durable storage independently of the SDK spool. Existing results are
        // never evicted silently; users can export/delete them explicitly.
        std::lock_guard lock(mutex_);uintmax_t bytes=0;size_t records=0;
        for(auto& f:std::filesystem::directory_iterator(root_))if(f.is_regular_file()){bytes+=f.file_size();++records;}
        if(bytes+64ULL*1024*1024*(live_.size()+1)>1024ULL*1024*1024||records>8192||live_.size()>=32)throw cymcp::Error("QuotaExceeded","Durable job storage or worker limit reached");
        auto job=std::make_shared<Job>();auto key=cymcp::random_token(16);job->context=c;
        // A durable job can outlive the requesting chat turn or HTTP socket.
        // Never retain borrowed notification/event callbacks into those objects.
        job->context.events={};job->context.notifications={};job->context.cancellation=cymcp::Cancellation{};job->context.cancellation.ancestors.push_back(lifetime_.flag);
        job->metadata={{"id",key},{"operation",op},{"owner",Owner(c)},{"state","queued"},{"pid",GetCurrentProcessId()},{"processEpoch",ProcessEpoch(GetCurrentProcessId())}};
        WritePrivateJson(root_/(key+".json"),job->metadata);live_[key]=job;
        try{workers_.post([this,key,op,args=p.at("arguments"),job]{
            try{
                {std::lock_guard lock(mutex_);job->context.cancellation.check();job->metadata["state"]="running";WritePrivateJson(root_/(key+".json"),job->metadata);}
                auto result=invoke_(op,args,job->context);job->context.cancellation.check();
                auto text=result.dump();if(text.size()>64*1024*1024)throw cymcp::Error("QuotaExceeded","A durable result exceeds 64 MiB; use filtered or paginated operations");
                WritePrivateJson(root_/(key+".result"),result);
                std::lock_guard lock(mutex_);job->metadata["state"]="completed";job->metadata["bytes"]=text.size();
            }catch(const cymcp::Error& e){std::lock_guard lock(mutex_);job->metadata["state"]=job->context.cancellation.cancelled()?"cancelled":"failed";job->metadata["error"]={{"code",e.code},{"message",cymcp::redact_error_message(e.what())}};
            }catch(const std::exception&){std::lock_guard lock(mutex_);job->metadata["state"]="failed";job->metadata["error"]={{"code","InternalError"},{"message","Native operation failed"}};}
            std::lock_guard lock(mutex_);job->finished=true;
            try{WritePrivateJson(root_/(key+".json"),job->metadata);live_.erase(key);}catch(...){
                std::error_code error;job->metadata["state"]="storage_error";job->metadata["resultAvailable"]=std::filesystem::is_regular_file(root_/(key+".result"),error);job->metadata["error"]={{"code","StorageError"},{"message","Could not commit the job status; the original action will not be replayed"}};
            }
        });}catch(...){live_.erase(key);job->metadata["state"]="failed";WritePrivateJson(root_/(key+".json"),job->metadata);throw;}
        auto result=job->metadata;result.erase("owner");return result;
    }});
    r.replace({"job_status","Read a durable job state.",id,{{"type","object"}},true,[this](const J& p,const cymcp::CallContext& c){std::lock_guard lock(mutex_);auto j=Load(p.at("id"),c);j.erase("owner");return j;}});
    r.replace({"job_result","Read the retained result. Large results use private, chunked SDK resources.",id,{{"type","object"}},true,[this](const J& p,const cymcp::CallContext& c){std::lock_guard lock(mutex_);auto key=p.at("id").get<std::string>();auto j=Load(key,c);if(j["state"]!="completed"&&!j.value("resultAvailable",false))throw cymcp::Error("NotReady","Job has no completed result");auto result=ReadPrivateJson(root_/(key+".result"),64*1024*1024);return result.dump().size()>16384?resources_->store_json(result,c):result;}});
    r.replace({"job_cancel","Cancel queued/running work cooperatively. Accepted UI mutations and recordings require their explicit inverse/stop operation.",id,{{"type","object"}},false,[this](const J& p,const cymcp::CallContext& c){std::lock_guard lock(mutex_);auto key=p.at("id").get<std::string>();Load(key,c);auto it=live_.find(key);const bool active=it!=live_.end()&&!it->second->finished;if(active)it->second->context.cancellation.cancel();return J{{"accepted",active}};}});
    auto page=obj;page["properties"]={{"offset",{{"type","integer"},{"minimum",0}}},{"limit",{{"type","integer"},{"minimum",1},{"maximum",128}}}};
    r.replace({"job_list","List this identity's durable jobs with stable ID ordering and bounded pagination.",page,{{"type","object"}},true,[this](const J& p,const cymcp::CallContext& c){std::lock_guard lock(mutex_);std::vector<std::filesystem::path> files;for(auto& f:std::filesystem::directory_iterator(root_))if(f.path().extension()==".json")files.push_back(f.path());std::sort(files.begin(),files.end());J rows=J::array();size_t matched=0,offset=p.value("offset",size_t(0)),limit=p.value("limit",size_t(20));for(auto& f:files){c.cancellation.check();auto found=live_.find(f.stem().string());auto j=found==live_.end()?ReadPrivateJson(f):found->second->metadata;if(j.value("owner","")!=Owner(c))continue;if(matched++<offset||rows.size()>=limit)continue;j.erase("owner");rows.push_back(j);}return J{{"rows",rows},{"offset",offset},{"matched",matched},{"hasMore",offset+rows.size()<matched}};}});
    auto file=obj;file["properties"]={{"path",{{"type","string"},{"maxLength",32760}}}};file["required"]={"path"};
    r.replace({"job_import","Import an existing CyTools JSON result into durable native storage. Requires application-control permission; never modifies old files or databases.",file,{{"type","object"}},false,[this](const J& p,const cymcp::CallContext& c){
        auto state=invoke_("get_state",J::object(),c);if(!state.contains("permissions")||!state["permissions"].value("appControl",false))throw cymcp::Error("AccessDenied","Enable AI application control before importing a local report");
        auto path=std::filesystem::u8path(p.at("path").get<std::string>());if(!path.is_absolute()||path.extension()!=L".json")throw cymcp::Error("InvalidArguments","Choose an absolute JSON report path");auto value=ReadPrivateJson(path,64*1024*1024);if(value.contains("outputs")&&value["outputs"].contains("data"))value=value["outputs"]["data"];
        std::lock_guard lock(mutex_);uintmax_t bytes=0;for(auto& f:std::filesystem::directory_iterator(root_))if(f.is_regular_file())bytes+=f.file_size();if(bytes+cymcp::serialized_size(value)>1024ULL*1024*1024)throw cymcp::Error("QuotaExceeded","Durable result quota exceeded");
        auto key=cymcp::random_token(16);WritePrivateJson(root_/(key+".result"),value);J metadata={{"id",key},{"owner",Owner(c)},{"operation","legacy_report"},{"state","completed"},{"imported",true}};WritePrivateJson(root_/(key+".json"),metadata);metadata.erase("owner");return metadata;
    }});
    r.replace({"job_delete","Explicitly remove a finished durable job and its result. Running jobs must be cancelled and finish first.",id,{{"type","object"}},false,[this](const J& p,const cymcp::CallContext& c){std::lock_guard lock(mutex_);auto key=p.at("id").get<std::string>();Load(key,c);if(live_.contains(key)&&!live_.at(key)->finished)throw cymcp::Error("Busy","Wait for cancellation/completion before deletion");live_.erase(key);std::filesystem::remove(root_/(key+".result"));std::filesystem::remove(root_/(key+".json"));return J{{"deleted",true}};}});
}
}
