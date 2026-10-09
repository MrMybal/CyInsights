// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiAnalysis.hpp"
#include "AiRpc.hpp"
#include "NativeTrace.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
using namespace cyinsights;
int main(){
    int checks=0,failures=0;auto check=[&](bool value,const char* name){++checks;if(!value){++failures;std::cerr<<name<<'\n';}};
    auto folder=std::filesystem::temp_directory_path()/("CyAiTests_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(folder);
    auto csv=folder/"scopes.csv";std::ofstream(csv)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n"
        "10,GameThread,1,FEngineLoop::Tick,0,0.030,0\n10,GameThread,2,Work,0.001,0.005,1\n"
        "10,GameThread,3,WaitForTasks,0.005,0.029,1\n10,GameThread,1,FEngineLoop::Tick,0.030,0.050,0\n"
        "10,GameThread,2,Work,0.031,0.049,1\n99,Worker,4,ExecuteForegroundTask,0.002,0.003,0\n"
        "99,Worker,5,Work,0.004,0.006,0\n99,Worker,6,work,0.007,0.008,0\n99,Worker,7,Unfinished,0.008,inf,0\n";
    NativeReadProgress progress;std::string error;auto store=PagedTimingStore::Build(csv,folder/"index",progress,error);check(store&&store->count==9,"All native records retained");if(!store)return 1;
    auto p=cygi::Json::Object();p["function"]="Work";auto result=AnalyzeAi(store,nullptr,"histogram",p);
    check(result["summary"]["count"].AsString()=="3","Same exact name on multiple timer IDs/threads counted");
    check(!result.Has("bins"),"Compact histogram omits bins by default");
    check(!result["summary"]["quantilesExact"].AsBool(),"Scope quantile estimation is explicit");
    check(result["provenance"].AsString()=="measured_native_inclusive_scope_duration","Native duration provenance retained");
    p["includeBins"]=true;p["start"]=0.004;p["end"]=0.031;result=AnalyzeAi(store,nullptr,"histogram",p);
    check(result["summary"]["count"].AsString()=="1","Half-open starts filter excludes upper boundary and keeps complete sample");
    check(result["bins"].Size()==64,"Requested histogram bins supplied");
    p["function"]="missing";result=AnalyzeAi(store,nullptr,"histogram",p);check(result["summary"]["min"].IsNull(),"Absent function remains unavailable, not zero");
    p=cygi::Json::Object();p["function"]="Unfinished";result=AnalyzeAi(store,nullptr,"histogram",p);check(result["summary"]["unavailable"].AsString()=="1","Unfinished endpoint is not fabricated");
    p=cygi::Json::Object();p["limit"]=1.;result=AnalyzeAi(store,nullptr,"list_functions",p);check(result["rows"].Size()==1&&result["hasMore"].AsBool(),"Catalogue response paginated, not population capped");
    check(result["rows"].Items()[0]["timerId"].AsString()=="1","IDs serialized as strings");
    result=AnalyzeAi(store,nullptr,"list_threads",p);check(result["matched"].AsString()=="2","Distinct native thread catalogue");
    p=cygi::Json::Object();p["targetFps"]=60.;result=AnalyzeAi(store,nullptr,"detect_issues",p);
    check(result["overBudget"].AsString()=="2","Both slow frame intervals detected");
    auto rows=result["findings"].Items();check(rows.size()==2&&rows[0]["probableCause"].AsString()=="Wait / pacing","Recognized waits separated from active work");
    check(rows[1]["probableCause"].AsString()=="GameThread candidate","Active thread budget candidate identified");
    check(!rows[0]["gpu"]["available"].AsBool()&&rows[0]["gpu"]["active"].IsNull(),"No CPU-only GPU invention");
    check(rows[0]["frameIdentity"].AsString()=="derived_tick_candidate","Derived frame identity explicit");
    check(result["provenance"].AsString()=="heuristic_not_causal_proof","Cause is not proof");
    p=cygi::Json::Object();p["source"]="frames";p["domain"]=2.;result=AnalyzeAi(store,nullptr,"histogram",p);check(result["summary"]["count"].AsString()=="0","Missing GPU histogram excludes missing values");
    p=cygi::Json::Object();p["source"]="frames";result=AnalyzeAi(store,nullptr,"histogram",p);check(result["summary"]["quantilesExact"].AsBool(),"Frame quantiles exact");
    p=cygi::Json::Object();p["start"]=0.001;p["end"]=0.006;p["search"]="Work";result=AnalyzeAi(store,nullptr,"hotspots",p);check(result["rows"].Size()>=1&&result["nestedInclusiveValuesAreNotThreadUtilization"].AsBool(),"Hotspots preserve clipped nesting semantics");
    p=cygi::Json::Object();p["candidates"]=true;result=AnalyzeAi(store,nullptr,"query_tasks",p);check(result["matched"].AsString()=="1","CPU task candidate query available");
    check(result["rows"].Items()[0]["id"].IsNull()&&result["rows"].Items()[0]["threadId"].AsString()=="99","Candidate has no invented task ID, correct native thread ID");
    bool rejected=false;try{AnalyzeAi(store,nullptr,"query_tasks",cygi::Json::Object());}catch(...){rejected=true;}check(rejected,"Native task data required unless candidates explicit");
    std::atomic<bool> cancel{true};rejected=false;try{AnalyzeAi(store,nullptr,"histogram",p,&cancel);}catch(...){rejected=true;}check(rejected,"Native analysis cooperatively cancelled");
    p["start"]=1.;p["end"]=0.;rejected=false;try{AnalyzeAi(store,nullptr,"histogram",p);}catch(...){rejected=true;}check(rejected,"Invalid range rejected");
    rejected=false;try{AnalyzeAi(nullptr,nullptr,"hotspots",cygi::Json::Object());}catch(...){rejected=true;}check(rejected,"No trace failure explicit");
    AiRpc rpc;check(rpc.Start(error),"Local authenticated RPC starts");
    auto reply=std::async(std::launch::async,[&]{HANDLE pipe=INVALID_HANDLE_VALUE;for(int i=0;i<400&&pipe==INVALID_HANDLE_VALUE;++i){pipe=CreateFileA(rpc.Name().c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);if(pipe==INVALID_HANDLE_VALUE)Sleep(5);}if(pipe==INVALID_HANDLE_VALUE)return std::string();std::string line="{\"operation\":\"get_state\",\"parameters\":{}}\n";DWORD count=0;WriteFile(pipe,line.data(),static_cast<DWORD>(line.size()),&count,nullptr);char buffer[4096];std::string response;while(ReadFile(pipe,buffer,sizeof(buffer),&count,nullptr)&&count){response.append(buffer,count);if(response.find('\n')!=std::string::npos)break;}CloseHandle(pipe);return response;});
    std::vector<std::shared_ptr<AiRequest>> requests;for(int i=0;i<400&&requests.empty();++i){requests=rpc.Take();Sleep(5);}check(requests.size()==1&&requests[0]->document["operation"].AsString()=="get_state","RPC marshals onto caller thread");
    if(!requests.empty()){auto response=cygi::Json::Object();response["ready"]=true;requests[0]->response.set_value(response);}check(reply.get().find("true")!=std::string::npos,"RPC delivers structured result");
    auto start=std::chrono::steady_clock::now();rpc.Stop();check(std::chrono::steady_clock::now()-start<std::chrono::seconds(2),"Idle RPC shutdown bounded");
    std::filesystem::remove_all(folder);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
