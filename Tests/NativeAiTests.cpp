// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiNative.hpp"
#include "AiResources.hpp"
#include <cymcp/chat_ui.hpp>
#include <cymcp/context.hpp>
#include <windows.h>
#include <fstream>
#include <iostream>
using namespace cyinsights;
using namespace std::chrono_literals;
namespace {
int checks=0;
void Check(bool v,const char* what){++checks;if(!v)throw std::runtime_error(what);}
template<class F> void Fails(F f){try{f();}catch(const cymcp::Error&){++checks;return;}throw std::runtime_error("Expected failure");}
std::string Full(cymcp::ResourceRegistry& r,std::string uri,const cymcp::CallContext& c){
    auto first=r.read(uri,c);auto content=first.at("contents")[0];if(content.at("mimeType")=="application/json"){auto j=cymcp::parse_json(content.at("text"));if(j.contains("firstUri"))uri=j.at("firstUri");}
    std::string out;for(int i=0;i<10000;++i){auto part=r.read(uri,c);out+=part["contents"][0]["text"].get<std::string>();auto chunk=part.at("_meta").at("com.cymcp/chunk");if(chunk.at("eof").get<bool>())return out;uri=chunk.at("nextUri");}throw std::runtime_error("Unbounded chunks");
}
void Wait(cymcp::ChatController& c,const std::function<bool()>& done){auto deadline=cymcp::Clock::now()+15s;while(!done()){c.poll();if(cymcp::Clock::now()>deadline)throw std::runtime_error("Chat timed out: "+c.state().error_message);std::this_thread::sleep_for(5ms);}c.poll();}
}
int main(int argc,char** argv){try{
    auto catalog=NativeJson::parse(EmbeddedAiCatalog().Write(-1));auto root=std::filesystem::current_path()/"NativeAiTestData"/std::to_string(GetCurrentProcessId());cymcp::private_directory(root);
    if(argc==4&&std::string(argv[1])=="--http-instance"){
        auto pid=uint32_t(std::stoul(argv[2]));auto endpoint=ReadPrivateJson(std::filesystem::u8path(argv[3]));
        auto url=endpoint.at("url").get<std::string>();const bool tokenRequired=endpoint.at("requiresToken");
        if(tokenRequired){cymcp::McpClient unauthenticated(url);Fails([&]{unauthenticated.connect();});}
        else Check(!endpoint.contains("token"),"Address-only profile does not create a token");
        cymcp::McpClient client(url,endpoint.value("token",std::string()));client.connect();
        auto tools=client.list_tools();Check(tools.size()==5,"HTTP compact MCP discovery");
        auto read=client.call_tool("cy_tools_call",{{"name","get_state"},{"arguments",NativeJson::object()}});
        Check(!read.value("isError",false),"HTTP MCP reads the actual application");
        auto state=CallNativeInstance(pid,0,"get_state",NativeJson::object());const bool allowed=state["permissions"]["appControl"];
        auto changed=client.call_tool("cy_tools_call",{{"name","filter_function"},{"arguments",{{"function","HttpProfileTest"}}}});
        Check(changed.value("isError",false)!=allowed,"HTTP mutation follows the selected profile");
        if(allowed)Check(CallNativeInstance(pid,0,"get_state",NativeJson::object())["exactFunction"]=="HttpProfileTest","HTTP mutation changed real UI state");
        auto invalid=client.call_tool("cy_tools_call",{{"name","filter_function"},{"arguments",{{"function",12}}}});
        Check(invalid.value("isError",false),"Free access still validates tool arguments");
        std::cout<<checks<<" real HTTP profile checks passed\n";return 0;
    }
    if(argc==4&&std::string(argv[1])=="--denied"){
        cymcp::ProcessOptions process;process.executable=std::filesystem::u8path(argv[3]);process.arguments={"mcp","--pid",argv[2],"--full"};cymcp::McpClient client(process);client.connect();auto read=client.call_tool("get_state",NativeJson::object());Check(!read.value("isError",false),"MCP read in read-only instance");auto denied=client.call_tool("filter_function",{{"function","DeniedMcpMutation"}});Check(denied.value("isError",false),"MCP mutation denied in read-only instance");std::cout<<checks<<" MCP permission checks passed\n";return 0;
    }
    if(argc==5&&std::string(argv[1])=="--instance"){
        auto pid=uint32_t(std::stoul(argv[2]));auto registry=MakeNativeRegistry(catalog,[pid](const auto& op,const auto& p,const auto& c){return CallNativeInstance(pid,0,op,p,c);});
        cymcp::ChatController controller;cymcp::ChatConnection connection;connection.provider_id="openai-compatible";connection.model="fixture-model";connection.provider.settings={{"base_url",argv[3]},{"deferred_tools",false}};connection.provider.tools=registry;connection.history_directory=root/"history";connection.history_id="actual-instance";
        controller.open(connection);Wait(controller,[&]{return controller.state().connected||!controller.state().error_code.empty();});Check(controller.state().connected,"Native API connected");
        auto attachment=root/"note.txt";std::ofstream(attachment)<<"NATIVE_ATTACHMENT_MARKER";
        controller.send("Set the exact function filter to NativeStreamingTest",{attachment});
        bool partial=false;auto deadline=cymcp::Clock::now()+15s;do{controller.poll();for(auto& message:controller.state().messages)if(message.role=="assistant"&&message.markdown=="Working")partial=true;if(cymcp::Clock::now()>deadline)throw std::runtime_error("Tool stream timed out");std::this_thread::sleep_for(5ms);}while(controller.state().busy);
        Check(partial,"Progressive response observed before completion");Check(controller.state().error_code.empty(),"Provider tool round succeeded");Check(CallNativeInstance(pid,0,"get_state",NativeJson::object())["exactFunction"]=="NativeStreamingTest","Chat tool changed the real application filter");
        auto count=controller.state().messages.size();controller.disconnect();Wait(controller,[&]{return !controller.state().busy;});std::filesystem::remove(attachment);connection.restore=true;controller.open(connection);Wait(controller,[&]{return controller.state().connected||!controller.state().error_code.empty();});Check(controller.state().messages.size()>=count,"Conversation restored without replay");bool restoredAttachment=false;for(const auto& message:controller.state().messages)restoredAttachment|=!message.attachments.empty();Check(restoredAttachment,"Portable attachment restored without its original source file");
        controller.send("cancel-test");Wait(controller,[&]{for(auto& m:controller.state().messages)if(m.role=="assistant"&&m.markdown.find("Waiting")!=std::string::npos)return true;return false;});controller.cancel();Wait(controller,[&]{return !controller.state().busy;});bool interrupted=false;for(auto& m:controller.state().messages)interrupted|=m.interrupted;Check(interrupted,"Stream cancellation marked interrupted");
        std::atomic_int remoteCalls=0;auto remoteRegistry=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
        remoteRegistry->add({"echo","echo",{{"type","object"}},{{"type","object"}},true,[&](const auto&,const auto&){++remoteCalls;return NativeJson{{"remote",true}};}});
        remoteRegistry->add({"failure","failure",{{"type","object"}},{{"type","object"}},false,[&](const auto&,const auto&)->NativeJson{++remoteCalls;throw cymcp::Error("ExpectedRemoteError","Fixture remote error");}});
        cymcp::McpServer remoteServer(*remoteRegistry);auto token=cymcp::random_token();cymcp::LocalHttpServer remoteHttp(token,[&](const auto& p,const auto& c){return remoteServer.handle(p,c);});auto remotePort=remoteHttp.start();_putenv_s("CYINSIGHTS_REMOTE_TEST_TOKEN",token.c_str());
        auto merged=ConnectNativeRemote(registry,{{"url","http://127.0.0.1:"+std::to_string(remotePort)+"/mcp"},{"tokenEnv","CYINSIGHTS_REMOTE_TEST_TOKEN"},{"allowTools",{"echo","failure"}},{"notifications",false}},{},30,false);
        cymcp::ChatController remoteChat;auto remoteConnection=connection;remoteConnection.provider.tools=merged;remoteConnection.restore=false;remoteConnection.history_id="remote-test";remoteChat.open(remoteConnection);Wait(remoteChat,[&]{return remoteChat.state().connected;});remoteChat.send("remote-test");Wait(remoteChat,[&]{return !remoteChat.state().busy;});Check(remoteChat.state().error_code.empty()&&remoteCalls==2,"Chat receives remote success and tool error over actual MCP");remoteChat.disconnect();Wait(remoteChat,[&]{return !remoteChat.state().busy;});merged.reset();_putenv_s("CYINSIGHTS_REMOTE_TEST_TOKEN","");remoteHttp.stop();
        cymcp::ProcessOptions process;process.executable=std::filesystem::u8path(argv[4]);process.arguments={"mcp","--pid",argv[2],"--full"};cymcp::McpClient client(process);client.connect();auto tools=client.list_tools();Check(tools.size()>=39,"Real stdio MCP discovery");auto state=client.call_tool("get_state",NativeJson::object());Check(!state.value("isError",false),"Real stdio MCP read");auto changed=client.call_tool("filter_function",{{"function","NativeMcpTest"}});Check(!changed.value("isError",false),"Real stdio MCP mutation");Check(CallNativeInstance(pid,0,"get_state",NativeJson::object())["exactFunction"]=="NativeMcpTest","MCP mutation reaches the real instance");
        std::cout<<checks<<" real-instance checks passed\n";return 0;
    }
    Check(NativeAiOptions{}.access_profile==cymcp::LocalAccessProfile::free,"New configurations default to Free");
    Check(NativeAiOptions::From({{"performance",true}}).access_profile==cymcp::LocalAccessProfile::free,"Configurations without an explicit choice migrate to Free");
    for(auto profile:{cymcp::LocalAccessProfile::free,cymcp::LocalAccessProfile::local_controlled,cymcp::LocalAccessProfile::token_only,cymcp::LocalAccessProfile::protected_access}){
        NativeAiOptions o;o.access_profile=profile;Check(NativeAiOptions::From(o.Json()).access_profile==profile,"Explicit profile round-trips unchanged");
        auto directory=root/NativeAccessProfileName(profile);WritePrivateJson(directory/"options.json",o.Json());
        {NativeAi saved(catalog,NativeJson::object(),directory,[](const auto&,const auto&,const auto&){return NativeJson::object();});Check(saved.options.access_profile==profile,"Explicit profile survives configuration loading");}
    }
    Fails([&]{NativeAiOptions::From({{"accessProfile","invalid-explicit-choice"}});});
    WritePrivateJson(root/"unknown-options"/"options.json",{{"accessProfile","unknown"}});
    {NativeAi guarded(catalog,NativeJson::object(),root/"unknown-options",[](const auto&,const auto&,const auto&){return NativeJson::object();});Check(guarded.RequiresToken()&&guarded.ControlsOperations(),"Malformed explicit choices never fall back to Free");Check(ReadPrivateJson(root/"unknown-options"/"options.json")["accessProfile"]=="unknown","Malformed settings are preserved for repair");}
    std::atomic_int calls=0;std::atomic_bool slow=false;bool allowed=false;auto handler=[&](const std::string& op,const NativeJson&,const cymcp::CallContext& c){c.cancellation.check();if(op=="filter_function"&&!allowed)throw cymcp::Error("AccessDenied","Control disabled");++calls;while(slow){c.cancellation.check();std::this_thread::sleep_for(1ms);}return NativeJson{{"accepted",true},{"rows",NativeJson::array()},{"operation",op},{"trace","PERF_RESULT_SECRET"},{"permissions",{{"appControl",allowed}}}};};
    WritePrivateJson(root/"host"/"options.json",{{"performance",true},{"preservedOption","keep"}});
    std::string id;
    {
        NativeAi host(catalog,NativeJson::parse(EmbeddedAiMethod().Write(-1)),root/"host",handler);
        auto migrated=ReadPrivateJson(root/"host"/"options.json");Check(migrated["accessProfile"]=="free"&&migrated["preservedOption"]=="keep","Migration records Free without deleting other settings");
        Check(host.registry->size()==48,"All business and native utility operations registered");
        Fails([&]{host.Invoke("filter_function",{{"function","X"}});});allowed=true;Check(host.Invoke("filter_function",{{"function","PERF_INPUT_SECRET"}})["accepted"],"Authorized business handler");
        Fails([&]{host.Invoke("filter_function",{{"function",12}});});Fails([&]{host.Invoke("capture_probe",{{"ipv4","not-an-ip"}});});
        auto methods=host.gateway->tools()->describe();Check(methods.size()==5,"Compact discovery contains five gateway operations");
        cymcp::CallContext alice,bob;alice.principal="alice";bob.principal="bob";
        std::string large;for(int i=0;i<100000;++i)large+="é🙂 native trace data\n";
        auto ref=host.resources->store_stream([&](auto& out,const auto& c){for(size_t n=0;n<large.size();n+=4096){c.cancellation.check();out.write(large.data()+n,std::streamsize(std::min(size_t(4096),large.size()-n)));}},"text/plain",alice);
        Check(Full(*host.resources,ref.at("uri"),alice)==large,"Disk-spooled UTF-8 reconstructs completely");Fails([&]{host.resources->read(ref.at("uri"),bob);});
        auto job=host.Invoke("job_start",{{"operation","get_state"},{"arguments",NativeJson::object()}},alice);id=job.at("id");
        auto deadline=cymcp::Clock::now()+5s;NativeJson status;do{status=host.Invoke("job_status",{{"id",id}},alice);if(cymcp::Clock::now()>deadline)throw std::runtime_error("Job timed out");std::this_thread::sleep_for(5ms);}while(status["state"]!="completed");
        Check(host.Invoke("job_result",{{"id",id}},alice)["accepted"],"Durable native job result");Fails([&]{host.Invoke("job_result",{{"id",id}},bob);});
        slow=true;auto cancelledJob=host.Invoke("job_start",{{"operation","get_state"},{"arguments",NativeJson::object()}},alice).at("id");host.Invoke("job_cancel",{{"id",cancelledJob}},alice);
        deadline=cymcp::Clock::now()+3s;do{status=host.Invoke("job_status",{{"id",cancelledJob}},alice);if(cymcp::Clock::now()>deadline)throw std::runtime_error("Job cancellation timed out");std::this_thread::sleep_for(2ms);}while(status["state"]!="cancelled");slow=false;Check(status["state"]=="cancelled","Durable worker job cancels cooperatively");
        WritePrivateJson(root/"legacy.json",{{"outputs",{{"data",{{"oldResult",true}}}}}});auto oldHash=cymcp::sha256_file(root/"legacy.json");auto imported=host.Invoke("job_import",{{"path",(root/"legacy.json").u8string()}},alice);
        Check(host.Invoke("job_result",{{"id",imported.at("id")}},alice).at("oldResult"),"Old JSON result imported into native durable storage");Check(oldHash==cymcp::sha256_file(root/"legacy.json"),"Legacy report remains unchanged");
        host.UpdateState({{"revision",1}});auto rev=host.changes->sequence();host.UpdateState({{"revision",2}});Check(host.changes->sequence()>rev,"Changes published before TTL");
        host.Start(0);auto publicInfo=host.Connection();Check(!publicInfo.contains("token"),"UI connection metadata excludes bearer token");
        auto endpoint=ReadPrivateJson(root/"host"/("connection-"+std::to_string(GetCurrentProcessId())+".json"));
        cymcp::McpClient client(endpoint.at("url"),endpoint.value("token",std::string()));client.connect();auto first=client.read_resource("cyinsights://state");client.read_resource("cyinsights://state");Check(client.cache_stats().hits>0,"HTTP resource cache used before TTL");
        client.start_notifications({{"resourcesListChanged",true},{"toolsListChanged",true}});std::this_thread::sleep_for(100ms);host.UpdateState({{"revision",3}});
        auto until=cymcp::Clock::now()+3s;bool fresh=false;do{client.drain_notifications();auto state=client.read_resource("cyinsights://state");fresh=state.dump().find("revision\\\":3")!=std::string::npos;if(!fresh)std::this_thread::sleep_for(10ms);}while(!fresh&&cymcp::Clock::now()<until);
        Check(fresh,"HTTP subscription invalidates state cache before TTL");client.stop_notifications();
        host.SetAccessProfile(cymcp::LocalAccessProfile::protected_access);auto protectedEndpoint=ReadPrivateJson(root/"host"/("connection-"+std::to_string(GetCurrentProcessId())+".json"));
        Check(protectedEndpoint["requiresToken"]&&protectedEndpoint.contains("token"),"Profile switch restarts HTTP with token authentication");
        {cymcp::McpClient noToken(protectedEndpoint.at("url"));Fails([&]{noToken.connect();});cymcp::McpClient authenticated(protectedEndpoint.at("url"),protectedEndpoint.at("token"));authenticated.connect();}
        Check(ReadPrivateJson(root/"host"/"options.json")["accessProfile"]=="protected_access","Explicit selected profile is persisted");
        // Exercise the same remote connector used by the Assistant, including
        // an exact allowlist and a structured tool error over real HTTP MCP.
        auto remoteRegistry=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
        remoteRegistry->add({"echo","echo",{{"type","object"}},{{"type","object"}},true,[](const auto&,const auto&){return NativeJson{{"remote",true}};}});
        remoteRegistry->add({"failure","failure",{{"type","object"}},{{"type","object"}},false,[](const auto&,const auto&)->NativeJson{throw cymcp::Error("FixtureFailure","Expected remote failure");}});
        cymcp::McpServer remoteServer(*remoteRegistry);auto token=cymcp::random_token();cymcp::LocalHttpServer http(token,[&](const auto& request,const auto& c){return remoteServer.handle(request,c);});auto port=http.start();_putenv_s("CYINSIGHTS_REMOTE_TEST_TOKEN",token.c_str());
        NativeJson config={{"url","http://127.0.0.1:"+std::to_string(port)+"/mcp"},{"tokenEnv","CYINSIGHTS_REMOTE_TEST_TOKEN"},{"allowTools",{"echo","failure"}},{"notifications",false}};
        cymcp::Cancellation remotePermission;auto merged=ConnectNativeRemote(host.registry,config,remotePermission,30,false);Check(!merged->invoke("remote.echo",NativeJson::object()).value("isError",false),"Explicitly allowed remote tool works");Fails([&]{merged->invoke("remote.failure",NativeJson::object());});Fails([&]{merged->invoke("remote.unlisted",NativeJson::object());});remotePermission.cancel();Fails([&]{merged->invoke("remote.echo",NativeJson::object());});merged.reset();_putenv_s("CYINSIGHTS_REMOTE_TEST_TOKEN","");http.stop();host.Stop();
    }
    auto interrupted=cymcp::random_token(16);WritePrivateJson(root/"host"/"jobs"/(interrupted+".json"),{{"id",interrupted},{"owner",cymcp::sha256("alice")},{"state","running"},{"pid",0},{"operation","get_state"}});const auto beforeRecovery=calls.load();
    auto committed=cymcp::random_token(16);WritePrivateJson(root/"host"/"jobs"/(committed+".json"),{{"id",committed},{"owner",cymcp::sha256("alice")},{"state","running"},{"pid",0},{"operation","get_state"}});WritePrivateJson(root/"host"/"jobs"/(committed+".result"),{{"committedBeforeCrash",true}});
    {
        NativeAi host(catalog,NativeJson::object(),root/"host",handler);cymcp::CallContext alice;alice.principal="alice";Check(host.Invoke("job_result",{{"id",id}},alice)["accepted"],"Durable result survives service restart");
        Check(host.Invoke("job_status",{{"id",interrupted}},alice)["state"]=="interrupted"&&calls.load()==beforeRecovery,"Interrupted jobs recover without replay");
        Check(host.Invoke("job_result",{{"id",committed}},alice)["committedBeforeCrash"],"A result committed before a status-write crash is recovered without replay");
    }
    {std::string metrics;for(auto& f:std::filesystem::directory_iterator(root/"host"))if(f.path().filename().string().starts_with("performance-")){std::ifstream log(f.path());metrics.append(std::istreambuf_iterator<char>(log),{});}Check(metrics.find("PERF_INPUT_SECRET")==std::string::npos&&metrics.find("PERF_RESULT_SECRET")==std::string::npos&&metrics.find("filter_function")!=std::string::npos,"Actual operation telemetry excludes inputs and results");}
    cymcp::ResourceOptions shortLife;shortLife.result_ttl=15ms;cymcp::ResourceRegistry resources(cymcp::Policy::allow_all(),shortLife);auto ref=resources.store("private","text/plain");std::this_thread::sleep_for(20ms);Fails([&]{resources.read(ref.at("uri"));});
    cymcp::PerformanceOptions po;po.jsonl_file=root/"metrics.jsonl";{auto monitor=std::make_shared<cymcp::PerformanceMonitor>(po);monitor->observe({"tool","get_state","ok",1,17,24,false});monitor->drain();}std::ifstream metrics(root/"metrics.jsonl");std::string text((std::istreambuf_iterator<char>(metrics)),{});Check(text.find("private")==std::string::npos&&text.find("get_state")!=std::string::npos,"Performance file contains metadata only");
    cymcp::ContextOptions co;co.max_bytes=400;co.recent_turns=1;cymcp::ContextWindow window(co);NativeJson history=NativeJson::array({{{"role","system"},{"content","Preserved rules"}},{{"role","user"},{"content",std::string(2000,'x')}},{{"role","assistant"},{"content","previous"}},{{"role","user"},{"content","active"}}});auto original=history.dump();auto projected=window.project(history);Check(history.dump()==original,"Context projection preserves full history");Check(projected.messages.front()["role"]=="system"&&projected.messages.back()["content"]=="active","System and active turn retained");
    std::cout<<checks<<" native integration checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"After "<<checks<<" checks: "<<e.what()<<'\n';return 1;}}
