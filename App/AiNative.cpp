// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiNative.hpp"
#include "AiJobs.hpp"
#include <windows.h>
#include <fstream>
#include <regex>
#include <cstdio>
#include <cstdlib>
namespace cyinsights {
using J=NativeJson;
namespace {
J Schema(J s){
    if(s.is_object())for(auto it=s.begin();it!=s.end();){
        // Pattern constraints are still checked by the host below. SDK 0.7 uses
        // a closed schema profile, so don't advertise unsupported validation.
        if(it.key()=="pattern"||it.key().starts_with("x-")||it.key()=="$schema")it=s.erase(it);
        else {it.value()=Schema(it.value());++it;}
    }else if(s.is_array())for(auto& v:s)v=Schema(v);
    return s;
}
void Patterns(const J& schema,const J& p){
    if(schema.contains("pattern")&&p.is_string()&&!std::regex_match(p.get<std::string>(),std::regex(schema.at("pattern").get<std::string>())))throw cymcp::Error("InvalidArguments","Value violates the operation pattern constraint");
    if(p.is_object())for(auto it=p.begin();it!=p.end();++it)if(schema.contains("properties")&&schema["properties"].contains(it.key()))Patterns(schema["properties"][it.key()],it.value());
}
J Object(){return {{"type","object"},{"additionalProperties",false}};}
cymcp::Cancellation RemoteCancellation(const cymcp::CallContext& c,const cymcp::Cancellation& permission){
    if(permission.cancelled())throw cymcp::Error("AccessDenied","This remote MCP connection is no longer enabled");
    auto combined=c.cancellation.child();combined.ancestors.push_back(permission.flag);combined.ancestors.insert(combined.ancestors.end(),permission.ancestors.begin(),permission.ancestors.end());combined.deadline=std::min(combined.deadline,permission.deadline);combined.check();return combined;
}
}
J NativeDefaults(const J& schema,J p){
    if(p.is_object()&&schema.contains("properties"))for(auto it=schema["properties"].begin();it!=schema["properties"].end();++it){
        if(!p.contains(it.key())&&it.value().contains("default"))p[it.key()]=it.value()["default"];
        if(p.contains(it.key())&&p[it.key()].is_object())p[it.key()]=NativeDefaults(it.value(),p[it.key()]);
    }
    cymcp::validate(p,Schema(schema));Patterns(schema,p);return p;
}
std::shared_ptr<cymcp::Registry> MakeNativeRegistry(const J& catalog,NativeHandler h){
    auto r=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
    for(const auto& d:catalog.at("operations")){
        cymcp::Operation op;op.name=d.at("id");op.description=d.at("description");op.input_schema=Schema(d.at("inputSchema"));
        auto output=d.at("outputSchema");if(output.contains("properties")&&output["properties"].contains("data"))output=output["properties"]["data"];
        op.output_schema=Schema(output);op.read_only=d.value("permissions",J::array()).empty();
        op.handler=[h,name=op.name,schema=d.at("inputSchema")](const J& p,const cymcp::CallContext& c){return h(name,NativeDefaults(schema,p),c);};r->add(std::move(op));
    }
    auto id=Object();id["properties"]={{"id",{{"type","string"},{"minLength",32},{"maxLength",32}}}};id["required"]={"id"};
    auto start=Object();start["properties"]={{"operation",{{"type","string"}}},{"arguments",{{"type","object"}}}};start["required"]={"operation","arguments"};
    auto read=Object();read["properties"]={{"uri",{{"type","string"},{"maxLength",4096}}}};read["required"]={"uri"};
    auto file=Object();file["properties"]={{"path",{{"type","string"},{"maxLength",32760}}}};file["required"]={"path"};
    auto page=Object();page["properties"]={{"offset",{{"type","integer"},{"minimum",0}}},{"limit",{{"type","integer"},{"minimum",1},{"maximum",128}}}};
    auto grep=Object();grep["properties"]={{"text",{{"type","string"},{"maxLength",4096}}},{"cursor",{{"type","string"}}},{"ignoreCase",{{"type","boolean"}}}};grep["required"]={"text"};
    const std::map<std::string,std::string> descriptions={
        {"job_start","Run an operation asynchronously and retain its result across restarts. Does not replay interrupted work."},
        {"job_status","Read a durable job state."},
        {"job_result","Read the retained result. Large results use private, chunked SDK resources."},
        {"job_list","List this identity's durable jobs with stable ID ordering and bounded pagination."},
        {"job_cancel","Cancel queued/running work cooperatively. Accepted UI mutations and recordings require their explicit inverse/stop operation."},
        {"job_delete","Explicitly remove a finished durable job and its result. Running jobs must be cancelled and finish first."},
        {"job_import","Import an existing CyTools JSON result into durable native storage. Requires application-control permission; never modifies old files or databases."},
        {"read_resource","Read an exposed resource URI, including bounded chunks from retained results. Never traverses the filesystem."},
        {"grep_resources","Literal streaming grep over explicitly exposed text resources; no filesystem traversal."}
    };
    for(auto name:{"job_start","job_status","job_result","job_list","job_cancel","job_delete","job_import","read_resource","grep_resources"}){
        auto schema=std::string(name)=="job_start"?start:std::string(name)=="job_list"?page:std::string(name)=="job_import"?file:std::string(name)=="read_resource"?read:std::string(name)=="grep_resources"?grep:id;
        const bool ro=std::string(name)!="job_start"&&std::string(name)!="job_cancel"&&std::string(name)!="job_delete"&&std::string(name)!="job_import";
        r->add({name,descriptions.at(name),schema,{{"type","object"}},ro,[h,name](const J& p,const cymcp::CallContext& c){return h(name,p,c);}});
    }
    return r;
}
std::shared_ptr<cymcp::Registry> ConnectNativeRemote(std::shared_ptr<const cymcp::Registry> local,const J& config,const cymcp::Cancellation& cancel,int cache_seconds,bool notifications){
    auto url=config.at("url").get<std::string>();auto parsed=cymcp::parse_url(url);if(parsed.scheme!="https"&&!parsed.loopback)throw cymcp::Error("InvalidEndpoint","Use HTTPS for remote endpoints");
    std::string token;auto env=config.value("tokenEnv",std::string());if(!env.empty())if(const char* value=std::getenv(env.c_str()))token=value;
    cymcp::McpClientOptions co;co.max_cache_ttl=std::chrono::seconds(std::clamp(cache_seconds,0,3600));auto client=std::make_shared<cymcp::McpClient>(url,token,co);client->connect(cancel);client->list_tools(cancel);
    if(notifications&&config.value("notifications",true))try{client->start_notifications({{"toolsListChanged",true},{"resourcesListChanged",true}},cancel);}catch(const cymcp::Error& e){if(e.code!="Unsupported")throw;}
    auto merged=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
    for(const auto& d:local->describe()){auto name=d.at("name").get<std::string>();merged->add({name,d.at("description"),d.at("inputSchema"),d.value("outputSchema",J::object()),d.value("annotations",J::object()).value("readOnlyHint",false),[local,name](const J& p,const auto& c){return local->invoke(name,p,c);}});}
    if(!config.at("allowTools").is_array()||config.at("allowTools").size()>128)throw cymcp::Error("InvalidOptions","At most 128 explicit remote tools may be allowed");
    for(const auto& allowed:config.at("allowTools")){auto name=allowed.get<std::string>();auto d=client->get_tool(name,cancel);auto safe="remote."+name;
        merged->add({safe,"Explicitly allowed external tool: "+name,d.at("inputSchema"),J::object(),false,[client,name,token,permission=cancel](const J& p,const auto& c){auto cancellation=RemoteCancellation(c,permission);client->drain_notifications();auto result=client->call_tool(name,p,cancellation);if(result.value("isError",false))throw cymcp::Error("RemoteToolError",cymcp::redact_error_message(result.dump(),{token}));return result;}});
    }
    if(config.value("allowResources",false)){
        merged->add({"remote.resources_list","List resources from the explicitly connected server, using SDK pagination and cache.",Object(),{{"type","object"}},true,[client,permission=cancel](const J&,const auto& c){auto cancellation=RemoteCancellation(c,permission);client->drain_notifications();return client->list_resources(cancellation);}});
        auto read=Object();read["properties"]={{"uri",{{"type","string"},{"maxLength",4096}}}};read["required"]={"uri"};
        merged->add({"remote.resources_read","Read a resource URI from the explicitly connected server.",read,{{"type","object"}},true,[client,permission=cancel](const J& p,const auto& c){auto cancellation=RemoteCancellation(c,permission);client->drain_notifications();return client->read_resource(p.at("uri"),cancellation);}});
    }
    return merged;
}
void WritePrivateJson(const std::filesystem::path& p,const J& j){
    cymcp::private_directory(p.parent_path());auto temp=p;temp+=L".tmp-"+std::filesystem::path(cymcp::random_token(16)).wstring();
    try{auto f=cymcp::open_private_output_file(temp);auto text=j.dump(2);if(std::fwrite(text.data(),1,text.size(),f.get())!=text.size()||std::fflush(f.get())!=0)throw cymcp::Error("StorageError","Cannot write private data");f.reset();
        if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw cymcp::Error("StorageError","Cannot commit private data");
    }catch(...){std::error_code error;std::filesystem::remove(temp,error);throw;}
}
J ReadPrivateJson(const std::filesystem::path& p,size_t limit){
    if(!std::filesystem::is_regular_file(p)||std::filesystem::file_size(p)>limit)throw cymcp::Error("NotFound","Private record missing or too large");
    std::ifstream in(p,std::ios::binary);std::string s((std::istreambuf_iterator<char>(in)),{});return cymcp::parse_json(s,limit);
}
const char* NativeAccessProfileName(cymcp::LocalAccessProfile profile){
    switch(profile){
    case cymcp::LocalAccessProfile::free:return "free";
    case cymcp::LocalAccessProfile::local_controlled:return "local_controlled";
    case cymcp::LocalAccessProfile::token_only:return "token_only";
    case cymcp::LocalAccessProfile::protected_access:return "protected_access";
    }return "protected_access";
}
cymcp::LocalAccessProfile NativeAccessProfileFrom(const std::string& name){
    for(auto profile:{cymcp::LocalAccessProfile::free,cymcp::LocalAccessProfile::local_controlled,cymcp::LocalAccessProfile::token_only,cymcp::LocalAccessProfile::protected_access})
        if(name==NativeAccessProfileName(profile))return profile;
    throw cymcp::Error("InvalidOptions","Unknown local MCP access profile");
}
J NativeAiOptions::Json()const{return {{"accessProfile",NativeAccessProfileName(access_profile)},{"compact",compact},{"diskSpool",disk_spool},{"notifications",notifications},{"performance",performance},{"cacheSeconds",cache_seconds},{"retentionSeconds",retention_seconds},{"chunkKiB",chunk_kib},{"spoolMiB",spool_mib},{"contextKiB",context_kib}};}
NativeAiOptions NativeAiOptions::From(const J& p){NativeAiOptions o;if(p.contains("accessProfile"))o.access_profile=NativeAccessProfileFrom(p.at("accessProfile").get<std::string>());o.compact=p.value("compact",true);o.disk_spool=p.value("diskSpool",true);o.notifications=p.value("notifications",true);o.performance=p.value("performance",false);o.cache_seconds=std::clamp(p.value("cacheSeconds",30),0,3600);o.retention_seconds=std::clamp(p.value("retentionSeconds",300),10,86400);o.chunk_kib=std::clamp(p.value("chunkKiB",32),1,256);o.spool_mib=std::clamp(p.value("spoolMiB",256),16,4096);o.context_kib=std::clamp(p.value("contextKiB",size_t(128)),size_t(0),size_t(16384));return o;}
NativeAi::NativeAi(const J& catalog,const J& method,const std::filesystem::path& directory,NativeHandler h):directory_(directory),native_(std::move(h)){
    cymcp::private_directory(directory_);
    try{
        auto file=directory_/"options.json";
        if(std::filesystem::exists(file)){
            auto saved=ReadPrivateJson(file);options=NativeAiOptions::From(saved);
            // A missing profile is not an explicit choice. Keep unrelated options.
            if(!saved.contains("accessProfile")){saved["accessProfile"]="free";WritePrivateJson(file,saved);}
        }else WritePrivateJson(file,options.Json());
    }catch(const std::exception&){
        // An unreadable/unknown explicit choice must never turn into free access.
        options.access_profile=cymcp::LocalAccessProfile::protected_access;
        options_error_="Cannot load native AI options; Protected is active. Select and save a profile to repair the settings.";
    }
    changes=std::make_shared<cymcp::ChangeFeed>();
    if(options.performance){cymcp::PerformanceOptions p;p.jsonl_file=directory_/("performance-"+std::to_string(GetCurrentProcessId())+"-"+cymcp::random_token(16)+".jsonl");performance=std::make_shared<cymcp::PerformanceMonitor>(p);}
    registry=MakeNativeRegistry(catalog,[this](const auto& op,const J& p,const cymcp::CallContext& c){auto call=c;call.cancellation.ancestors.push_back(lifetime_.flag);call.cancellation.check();return native_(op,p,call);});
    cymcp::ResourceOptions ro;ro.chunk_bytes=size_t(options.chunk_kib)*1024;ro.result_ttl=std::chrono::seconds(options.retention_seconds);ro.max_disk_bytes=size_t(options.spool_mib)*1024*1024;if(options.disk_spool)ro.disk_directory=directory_/"spool"/std::to_string(GetCurrentProcessId());
    resources=std::make_shared<cymcp::ResourceRegistry>(cymcp::Policy::allow_all(),ro);
    if(options.notifications){registry->set_change_feed(changes);resources->set_change_feed(changes);}registry->set_performance_monitor(performance);
    auto text=std::make_shared<const std::string>(method.dump(2));cymcp::Resource guide;guide.uri="cyinsights://analysis-method";guide.name="Trace analysis method";guide.description="Evidence, units, availability and limitations";guide.mime_type="application/json";guide.size=text->size();guide.read=[text](size_t a,size_t n,const auto&){return text->substr(std::min(a,text->size()),n);};resources->add(guide);
    auto definitions=std::make_shared<const std::string>(catalog.dump(2));auto contract=guide;contract.uri="cyinsights://operations";contract.name="Application operation reference";contract.size=definitions->size();contract.read=[definitions](size_t a,size_t n,const auto&){return definitions->substr(std::min(a,definitions->size()),n);};resources->add(contract);
    registry->replace({"read_resource","Read an exposed resource URI, including bounded chunks from retained results. Never traverses the filesystem.",registry->describe("read_resource").at("inputSchema"),{{"type","object"}},true,[this](const J& p,const auto& c){return resources->read(p.at("uri"),c);}});
    registry->replace({"grep_resources","Literal streaming grep over explicitly exposed text resources; no filesystem traversal.",registry->describe("grep_resources").at("inputSchema"),{{"type","object"}},true,[this](const J& p,const auto& c){return resources->grep(p.at("text"),p.value("ignoreCase",false),p.value("cursor",""),c);}});
    jobs_=std::make_unique<AiJobs>(directory_/"jobs",[this](const auto& op,const J& p,const auto& c){return registry->invoke(op,p,c);},resources);jobs_->Register(*registry);
    cymcp::ToolGatewayOptions go;go.resources=ro;gateway=std::make_unique<cymcp::ToolGateway>(registry,go,resources);
}
NativeAi::~NativeAi(){Shutdown();}
std::shared_ptr<const cymcp::Registry> NativeAi::Tools()const{return options.compact?gateway->tools():registry;}
void NativeAi::Start(uint64_t document){
    if(http_)return;transport_=cymcp::Cancellation{};token_=RequiresToken()?cymcp::random_token():std::string();cymcp::McpServerOptions mo;mo.resources=resources;mo.cache_ttl=std::chrono::seconds(options.cache_seconds);mo.performance=performance;if(options.notifications)mo.changes=changes;
    mcp_=std::make_unique<cymcp::McpServer>(*Tools(),"CyInsights",mo);
    auto endpoint=[this](const J& p,const cymcp::CallContext& input){auto c=input;c.principal="local";c.cancellation.ancestors.push_back(lifetime_.flag);c.cancellation.ancestors.push_back(transport_.flag);if(!p.contains("jsonrpc")&&p.value("method","")=="invoke")return Invoke(p.at("name"),p.value("arguments",J::object()),c);return p.contains("jsonrpc")?mcp_->handle(p,c):cymcp::dispatch_app(*registry,p,c);};
    // SDK transport always binds 127.0.0.1. Free/Local controlled require no token.
    std::unique_ptr<cymcp::LocalHttpServer> server;
    if(RequiresToken())server=std::make_unique<cymcp::LocalHttpServer>(token_,endpoint);
    else server=std::make_unique<cymcp::LocalHttpServer>(cymcp::LocalHttpServer::UnauthenticatedLoopback{},endpoint);
    auto port=server->start();connection_={{"sdkVersion",cymcp::version},{"pid",GetCurrentProcessId()},{"document",document},{"url","http://127.0.0.1:"+std::to_string(port)+"/mcp"},{"accessProfile",NativeAccessProfileName(options.access_profile)},{"requiresToken",RequiresToken()},{"controlledOperations",ControlsOperations()}};
    if(RequiresToken())connection_["token"]=token_;
    connection_path_=directory_/("connection-"+std::to_string(GetCurrentProcessId())+".json");WritePrivateJson(connection_path_,connection_);http_=std::move(server);
}
void NativeAi::SetAccessProfile(cymcp::LocalAccessProfile profile){
    auto saved=options.Json();saved["accessProfile"]=NativeAccessProfileName(profile);
    WritePrivateJson(directory_/"options.json",saved);
    const bool running=Running();const auto document=connection_.value("document",uint64_t(0));
    Stop();options.access_profile=profile;options_error_.clear();
    if(running)Start(document);
}
void NativeAi::Stop(){transport_.cancel();if(mcp_)mcp_->cancel_all();if(http_)http_->stop();http_.reset();mcp_.reset();if(!connection_path_.empty()){std::error_code e;std::filesystem::remove(connection_path_,e);}connection_=J::object();token_.clear();}
void NativeAi::Cancel(){lifetime_.cancel();if(jobs_)jobs_->CancelAll();}
void NativeAi::Shutdown(){Cancel();Stop();jobs_.reset();}
void NativeAi::DrainPerformance(){if(performance)performance->drain();}
void NativeAi::UpdateState(const J& state){
    auto text=std::make_shared<const std::string>(state.dump());cymcp::Resource r;r.uri="cyinsights://state";r.name="Current document state";r.mime_type="application/json";r.size=text->size();r.cache_ttl=std::chrono::seconds(options.cache_seconds);r.read=[text](size_t a,size_t n,const auto&){return text->substr(std::min(a,text->size()),n);};resources->replace(std::move(r));
}
J NativeAi::Connection()const {auto j=connection_;j.erase("token");return j;}
J NativeAi::Invoke(const std::string& op,const J& p,cymcp::CallContext c){c.cancellation.ancestors.push_back(lifetime_.flag);for(const auto* name:{"cy_tools_search","cy_tools_describe","cy_tools_call","cy_resources_read","cy_resources_grep"})if(op==name)return gateway->tools()->invoke(op,p,c);return registry->invoke(op,p,c);}
J CallNativeInstance(uint32_t pid,uint64_t document,const std::string& op,const J& p,const cymcp::CallContext& c){
    std::wstring name=L"\\\\.\\pipe\\CyInsights.AI."+std::to_wstring(pid);if(document)name+=L"."+std::to_wstring(document);
    HANDLE pipe=INVALID_HANDLE_VALUE;const auto connect_deadline=cymcp::Clock::now()+std::chrono::seconds(2);
    do{c.cancellation.check();pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(pipe!=INVALID_HANDLE_VALUE)break;std::this_thread::sleep_for(std::chrono::milliseconds(10));}while(cymcp::Clock::now()<connect_deadline);
    if(pipe==INVALID_HANDLE_VALUE)throw cymcp::Error("NotConnected","Enable the local bridge in the selected CyInsights document");
    struct Guard {HANDLE h;~Guard(){CloseHandle(h);}} guard{pipe};
    auto transfer=[&](bool write,void* bytes,DWORD count){OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);Guard event{ov.hEvent};DWORD done=0;
        auto ok=write?WriteFile(pipe,bytes,count,&done,&ov):ReadFile(pipe,bytes,count,&done,&ov);
        if(!ok&&GetLastError()!=ERROR_IO_PENDING)throw cymcp::Error("Disconnected","Instance disconnected");
        if(!ok){while(WaitForSingleObject(ov.hEvent,10)==WAIT_TIMEOUT)if(c.cancellation.cancelled()||cymcp::Clock::now()>c.cancellation.deadline){CancelIoEx(pipe,&ov);GetOverlappedResult(pipe,&ov,&done,TRUE);c.cancellation.check();}
            if(!GetOverlappedResult(pipe,&ov,&done,FALSE))throw cymcp::Error("Disconnected","Instance disconnected");}return done;
    };
    auto line=J{{"operation",op},{"parameters",p}}.dump()+"\n";if(line.size()>65536)throw cymcp::Error("TooLarge","Native request exceeds 64 KiB");
    size_t offset=0;while(offset<line.size()){auto n=transfer(true,line.data()+offset,DWORD(line.size()-offset));if(!n)throw cymcp::Error("Disconnected","Empty pipe write");offset+=n;}
    std::string result;char bytes[8192];while(result.find('\n')==std::string::npos){auto n=transfer(false,bytes,sizeof(bytes));if(!n)throw cymcp::Error("Disconnected","Empty pipe read");result.append(bytes,n);if(result.size()>64*1024*1024)throw cymcp::Error("TooLarge","Use a durable job and resource chunks for large results");}
    auto j=cymcp::parse_json(result,64*1024*1024);if(j.contains("error")&&j["error"].is_object())throw cymcp::Error(j["error"].value("code","NativeError"),j["error"].value("message","Native request failed"));return j;
}
}
