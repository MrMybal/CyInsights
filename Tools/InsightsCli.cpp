// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiNative.hpp"
#include "AiResources.hpp"
#include "AiAnalysis.hpp"
#include "NativeTrace.hpp"
#include "AiExtendedAnalysis.hpp"
#include "AppPaths.hpp"
#include <windows.h>
#include <iostream>
#include <fstream>
using namespace cyinsights;
namespace {
cymcp::Cancellation cancelled;
std::mutex progress_mutex;
NativeReadProgress* active_progress=nullptr;
struct IndexProgressScope {
    explicit IndexProgressScope(NativeReadProgress& p){std::lock_guard lock(progress_mutex);active_progress=&p;}
    ~IndexProgressScope(){std::lock_guard lock(progress_mutex);active_progress=nullptr;}
};
BOOL WINAPI Interrupt(DWORD event){if(event==CTRL_C_EVENT||event==CTRL_BREAK_EVENT){cancelled.cancel();std::lock_guard lock(progress_mutex);if(active_progress)active_progress->cancel=true;return TRUE;}return FALSE;}
void Help(){std::cout<<"CyInsightsCli - native CyMCPSDK "<<cymcp::version<<"\n"
    "  list [--pid PID --document ID | --connection FILE | --trace timing.csv]\n"
    "  describe OPERATION\n  invoke OPERATION [--json JSON | --params-file FILE]\n"
    "  mcp [--pid PID --document ID | --trace timing.csv] [--full]\n"
    "  providers\n"
    "Instance options apply to invoke/describe/mcp as well. Without an instance,\n"
    "list/describe work offline. --trace analyzes timing CSV with native indexes,\n"
    "without a window. GUI/capture actions require an enabled local bridge.\n"
    "Ctrl+C cancels invocation/indexing; MCP clients use protocol cancellation.\n"
    "Cancellation does not undo accepted actions or stop recording.\n"
    "Exit codes: 0 success, 2 arguments/schema, 3 connection, 4 denied,\n"
    "5 operation failure, 130 cancelled. stdout is JSON, diagnostics use stderr.\n"
    "Existing CyInsightsApp flags and CyCaptureTool pack/inspect/extract remain available.\n";}
}
int wmain(int argc,wchar_t** argv){
    SetConsoleOutputCP(CP_UTF8);SetConsoleCtrlHandler(Interrupt,TRUE);
    try{
        std::vector<std::string> args;for(int i=1;i<argc;++i)args.push_back(std::filesystem::path(argv[i]).u8string());
        if(args.empty()||args[0]=="help"||args[0]=="--help"||args[0]=="-h"){Help();return 0;}
        const auto command=args[0];uint32_t pid=0;uint64_t document=0;std::filesystem::path trace,connection;bool full=false;std::string operation;NativeJson parameters=NativeJson::object();
        for(size_t i=1;i<args.size();++i){auto value=[&](){if(i+1>=args.size())throw cymcp::Error("InvalidArguments","Missing option value");return args[++i];};
            if(args[i]=="--pid")pid=static_cast<uint32_t>(std::stoul(value()));
            else if(args[i]=="--document")document=std::stoull(value());
            else if(args[i]=="--trace")trace=std::filesystem::u8path(value());
            else if(args[i]=="--connection")connection=std::filesystem::u8path(value());
            else if(args[i]=="--json")parameters=cymcp::parse_json(value(),65536);
            else if(args[i]=="--params-file")parameters=ReadPrivateJson(std::filesystem::u8path(value()),65536);
            else if(args[i]=="--full")full=true;
            else if(!args[i].starts_with("-")&&operation.empty())operation=args[i];
            else throw cymcp::Error("InvalidArguments","Unknown option or extra argument");
        }
        cymcp::CallContext context;context.cancellation=cancelled;
        if(command=="providers"){auto providers=cymcp::ProviderRegistry();NativeJson rows=NativeJson::array();for(auto& d:providers.discover({},context))rows.push_back({{"id",d.provider_id},{"name",d.name},{"state",d.state},{"source",d.source},{"models",d.models},{"errorCode",d.error_code}});std::cout<<rows.dump()<<'\n';return 0;}
        if((pid?1:0)+(!trace.empty()?1:0)+(!connection.empty()?1:0)>1)throw cymcp::Error("InvalidArguments","Choose one target: --pid, --connection or --trace");
        auto catalog=NativeJson::parse(EmbeddedAiCatalog().Write(-1));std::shared_ptr<PagedTimingStore> store;
        if(!trace.empty()){
            if(trace.extension()!=L".csv")throw cymcp::Error("InvalidArguments","Headless analysis expects an exported timing CSV; use an open instance for .utrace/.cycapture");
            trace=std::filesystem::absolute(trace);auto index=UserDataDirectory()/L"AI"/L"Indexes"/(cymcp::sha256(trace.u8string())+".index");cymcp::private_directory(index.parent_path());
            NativeReadProgress progress;IndexProgressScope active(progress);context.cancellation.check();std::string error;store=PagedTimingStore::Build(trace,index,progress,error);context.cancellation.check();if(!store)throw cymcp::Error("TraceError",error);
        }
        NativeJson endpoint;if(!connection.empty()){endpoint=ReadPrivateJson(connection,8192);if(!cymcp::parse_url(endpoint.at("url")).loopback)throw cymcp::Error("InvalidArguments","An application connection must use loopback; configure remote tools separately");}
        NativeHandler invoke=[&](const std::string& op,const NativeJson& p,const cymcp::CallContext& c)->NativeJson{
            if(pid)return CallNativeInstance(pid,document,op,p,c);
            if(!connection.empty()){
                cymcp::HttpRequest request;request.method="POST";request.url=endpoint.at("url").get<std::string>();if(request.url.ends_with("/mcp"))request.url.replace(request.url.size()-4,4,"/v1");request.headers["Authorization"]="Bearer "+endpoint.at("token").get<std::string>();request.headers["Content-Type"]="application/json";request.body=NativeJson{{"method","invoke"},{"name",op},{"arguments",p}}.dump();
                auto response=cymcp::HttpClient().perform(request,c.cancellation);if(response.status==401||response.status==403)throw cymcp::Error("AccessDenied","The local application rejected this connection token");if(response.body.empty())throw cymcp::Error("ConnectionError","Local HTTP service returned no JSON result");auto out=cymcp::parse_json(response.body,16*1024*1024);if(response.status!=200||(out.contains("error")&&out["error"].is_object()))throw cymcp::Error(out.value("error",NativeJson::object()).value("code","ConnectionError"),"Native HTTP invocation failed");return out;
            }
            if(op=="get_analysis_guide")return {{"method",NativeJson::parse(EmbeddedAiMethod().Write(-1))},{"scope","headless_native_analysis"}};
            if(op=="get_capabilities"){
                NativeJson operations=NativeJson::array();for(const auto& row:catalog.at("operations")){auto name=row.at("id").get<std::string>();const bool available=name=="get_analysis_guide"||name=="get_capabilities"||(store&&(name=="get_state"||AiAnalysisOperation(name)||name=="query_scopes"||name=="query_frames"));operations.push_back({{"id",name},{"available",available},{"requiresInstance",!available}});}
                return {{"scope","headless_timing_csv"},{"operations",operations},{"providers",{{"timing",{{"loaded",bool(store)}}}}},{"limitations",{{"guiCaptureAndOtherProviders","connect_to_open_instance"}}}};
            }
            if(!store)throw cymcp::Error("NotConnected","Select --pid PID or --trace timing.csv");
            if(op=="get_state")return {{"ready",true},{"events",std::to_string(store->count)},{"headless",true},{"domain",{{"start",store->start},{"end",store->end}}},{"permissions",{{"appControl",false},{"remoteCapture",false}}}};
            auto params=p;if(!params.contains("start"))params["start"]=store->start;if(!params.contains("end"))params["end"]=store->end;
            cygi::Json native;cygi::Json::Parse(params.dump(),native);cygi::Json result;
            if(AiAnalysisOperation(op))result=AnalyzeAi(store,nullptr,op,native,c.cancellation.flag.get());
            else if(op=="query_scopes"||op=="query_frames") {AiDataSnapshot snapshot;snapshot.timing=store;result=AnalyzeAiExtended(snapshot,op,native,c.cancellation.flag.get());}
            else throw cymcp::Error("NotConnected","This action requires an open application document");
            c.cancellation.check();return NativeJson::parse(result.Write(-1));
        };
        auto registry=MakeNativeRegistry(catalog,invoke);
        if(command=="list"){std::cout<<NativeJson{{"sdkVersion",cymcp::version},{"operations",registry->describe()}}.dump()<<'\n';return 0;}
        if(command=="describe"){if(operation.empty())throw cymcp::Error("InvalidArguments","Operation required");std::cout<<registry->describe(operation).dump()<<'\n';return 0;}
        if(command=="invoke"){if(operation.empty())throw cymcp::Error("InvalidArguments","Operation required");std::cout<<registry->invoke(operation,parameters,context).dump()<<'\n';return 0;}
        if(command=="mcp"){
            cymcp::ToolGateway gateway(registry);auto tools=full?std::shared_ptr<const cymcp::Registry>(registry):gateway.tools();
            std::shared_ptr<cymcp::Registry> proxy;
            if(!full&&(pid||!connection.empty())){
                // Keep result references and their owner in the actual app's
                // gateway. A second local spool could not read its handles.
                proxy=std::make_shared<cymcp::Registry>(cymcp::Policy::allow_all());
                for(const auto& d:gateway.tools()->describe()){auto name=d.at("name").get<std::string>();proxy->add({name,d.at("description"),d.at("inputSchema"),d.value("outputSchema",NativeJson::object()),false,[invoke,name](const auto& p,const auto& c){return invoke(name,p,c);}});}tools=proxy;
            }
            for(auto name:{"analysis-method","operations"}){
                auto value=std::make_shared<const std::string>(std::string(name)=="operations"?catalog.dump():EmbeddedAiMethod().Write(-1));
                cymcp::Resource resource;resource.uri=std::string("cyinsights://")+name;resource.name=name;resource.mime_type="application/json";resource.size=value->size();resource.read=[value](size_t offset,size_t length,const auto&){return value->substr(std::min(offset,value->size()),length);};gateway.resources()->add(std::move(resource));
            }
            cymcp::McpServerOptions options;options.resources=gateway.resources();cymcp::McpServer server(*tools,"CyInsights",options);
            // The daemon uses SDK JSON-RPC cancellation. Let console Ctrl+C
            // terminate an idle stdio daemon instead of swallowing it while
            // the SDK is waiting for the next input line.
            SetConsoleCtrlHandler(Interrupt,FALSE);server.serve_stdio(std::cin,std::cout);return 0;
        }
        throw cymcp::Error("InvalidArguments","Unknown command; use --help");
    }catch(const cymcp::Error& e){std::cerr<<NativeJson{{"error",{{"code",e.code},{"message",cymcp::redact_error_message(e.what())}}}}.dump()<<'\n';if(e.code=="Cancelled")return 130;if(e.code=="AccessDenied")return 4;if(e.code=="NotConnected"||e.code=="Disconnected"||e.code=="ConnectionError")return 3;if(e.code=="InvalidArguments"||e.code=="InvalidParameters"||e.code=="SchemaValidation")return 2;return 5;
    }catch(const std::exception& e){std::cerr<<NativeJson{{"error",{{"code","InvalidArguments"},{"message",cymcp::redact_error_message(e.what())}}}}.dump()<<'\n';return 2;}
}
