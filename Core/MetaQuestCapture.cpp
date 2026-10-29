// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "MetaQuestCapture.hpp"
#include "../Tools/NativeTiming/MetaGpuPerfetto.hpp"
#include <Windows.h>
#include <chrono>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>
namespace cyinsights {
namespace {
std::wstring Wide(const std::string& s){return fs::u8path(s).wstring();}
std::wstring Quote(const std::wstring& s){std::wstring out=L"\"";unsigned slash=0;for(wchar_t c:s){if(c==L'\\'){++slash;continue;}out.append(c==L'"'?slash*2+1:slash,L'\\');slash=0;out+=c;}out.append(slash*2,L'\\');return out+L'"';}
std::string Read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
bool Token(const std::string& s,const char* expression){return std::regex_match(s,std::regex(expression));}
struct Handle{HANDLE h=nullptr;~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}};
}
fs::path MetaQuestCapture::FindAdb(const fs::path& explicit_path){
    if(!explicit_path.empty())return fs::is_regular_file(explicit_path)?explicit_path:fs::path();
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);auto bundled=fs::path(path).parent_path()/"Tools/adb/adb.exe";
    if(fs::is_regular_file(bundled))return bundled;
    for(const wchar_t* env:{L"ANDROID_SDK_ROOT",L"ANDROID_HOME",L"LOCALAPPDATA"}){auto n=GetEnvironmentVariableW(env,path,32768);if(!n||n>=32768)continue;auto root=fs::path(path);if(std::wstring(env)==L"LOCALAPPDATA")root/=L"Android/Sdk";auto adb=root/L"platform-tools/adb.exe";if(fs::is_regular_file(adb))return adb;}
    if(SearchPathW(nullptr,L"adb.exe",nullptr,32768,path,nullptr)&&fs::is_regular_file(path))return path;return {};
}
std::string MetaQuestCapture::Call(const std::vector<std::wstring>& args,unsigned timeout,bool cleanup){
    Check(!adb_.empty(),"ADB unavailable: select adb.exe or install Android SDK platform-tools");
    const auto dir=staging_.empty()?fs::temp_directory_path():staging_;fs::create_directories(dir);
    const auto outPath=dir/("meta-adb-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetCurrentThreadId())+".log");
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};Handle log;log.h=CreateFileW(outPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);Check(log.h!=INVALID_HANDLE_VALUE,"Cannot create Meta capture command log");
    Handle input;input.h=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;startup.StartupInfo.wShowWindow=SW_HIDE;startup.StartupInfo.hStdOutput=startup.StartupInfo.hStdError=log.h;startup.StartupInfo.hStdInput=input.h;
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);std::vector<unsigned char> attributes(bytes);startup.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());Check(InitializeProcThreadAttributeList(startup.lpAttributeList,1,0,&bytes)!=0,"Cannot initialize ADB process attributes");
    HANDLE inherited[]={log.h,input.h};if(!UpdateProcThreadAttribute(startup.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)){DeleteProcThreadAttributeList(startup.lpAttributeList);throw std::runtime_error("Cannot restrict ADB inherited handles");}
    std::wstring command=Quote(adb_.wstring());for(const auto& arg:args)command+=L" "+Quote(arg);
    PROCESS_INFORMATION process{};const bool launched=CreateProcessW(adb_.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&startup.StartupInfo,&process)!=0;DeleteProcThreadAttributeList(startup.lpAttributeList);Check(launched,"Cannot launch ADB");Handle ph,th;ph.h=process.hProcess;th.h=process.hThread;
    auto begin=std::chrono::steady_clock::now();bool aborted=false;
    while(WaitForSingleObject(ph.h,25)==WAIT_TIMEOUT){if((!cleanup&&Stopping())||std::chrono::steady_clock::now()-begin>std::chrono::milliseconds(timeout)){TerminateProcess(ph.h,125);WaitForSingleObject(ph.h,1000);aborted=true;break;}}
    DWORD code=0;GetExitCodeProcess(ph.h,&code);CloseHandle(log.h);log.h=nullptr;
    auto text=Read(outPath);std::error_code ec;fs::remove(outPath,ec);
    if(aborted)throw std::runtime_error(cleanup?"Meta cleanup timed out":"Meta command cancelled or timed out");
    if(code)throw std::runtime_error("ADB command failed: "+text.substr(0,1500));return text;
}
std::string MetaQuestCapture::Shell(const std::vector<std::wstring>& args,unsigned timeout,bool cleanup){std::vector<std::wstring> all={L"-s",Wide(device_),L"shell"};all.insert(all.end(),args.begin(),args.end());return Call(all,timeout,cleanup);}
bool MetaQuestCapture::SelectDevice(const MetaQuestOptions& options,const std::string& ipv4,const cygi::Json& metadata){try{
        adb_=FindAdb(options.adb);Check(!adb_.empty(),"Meta GPU: adb.exe missing; select it in capture options");
        device_=options.device;
        if(device_.empty()){
            auto devices=Call({L"devices"});std::istringstream lines(devices);std::string line;std::vector<std::string> matches;
            while(std::getline(lines,line)){std::istringstream row(line);std::string serial,status;row>>serial>>status;if(status!="device"||!Token(serial,"[A-Za-z0-9_.:-]+"))continue;
                device_=serial;if(serial.rfind(ipv4+":",0)==0){matches.push_back(serial);continue;}
                try{const auto ip=Shell({L"ip",L"-o",L"-4",L"addr",L"show",L"wlan0"});if(ip.find("inet "+ipv4+"/")!=std::string::npos)matches.push_back(serial);}catch(...){if(Stopping())throw;}
            }
            Check(matches.size()==1,"Meta GPU: no unique authorized ADB device matches the game IP; connect/authorize the Quest or choose its ADB identifier");device_=matches.front();
        }
        Check(Token(device_,"[A-Za-z0-9_.:-]+"),"Invalid ADB device identifier");
        package_=options.package.empty()?metadata["android_package"].AsString():options.package;
        Check(Token(package_,"[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z0-9_]+)+"),"Meta GPU: update the plugin for automatic Android package identity, or enter the game's package");
        const auto known=metadata["android_package"].AsString();Check(known.empty()||known==package_,"Meta GPU package does not match the connected game's identity");
        return true;
    }catch(const std::exception& e){message_=e.what();return false;}}
namespace {
std::string Base64(const std::string& input){static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;uint32_t buffer=0;int bits=-6;for(unsigned char c:input){buffer=(buffer<<8)|c;bits+=8;while(bits>=0){out+=abc[(buffer>>bits)&63];bits-=6;}}if(bits>-6)out+=abc[((buffer<<8)>>(bits+8))&63];while(out.size()%4)out+='=';return out;}
}
cygi::Json MetaQuestCapture::Companion(const cygi::Json& request,bool cleanup){
    const auto encoded=Base64(request.Write());auto text=Shell({L"CLASSPATH=/data/local/tmp/cyinsightovr-bridge.jar",L"app_process",L"/system/bin",L"org.cyinsights.ovr.ShellClient",L"--request",Wide(encoded)},30000,cleanup);cygi::Json result;std::string error;Check(cygi::Json::Parse(text,result,&error)&&result.IsObject(),"Invalid CyInsightOVR bridge response");
    // Dispatch failures have a single error field. Recording status also has an
    // error field (normally empty) alongside its identity and completion state.
    if(result.Has("error")&&result.Members().size()==1)throw std::runtime_error(result["error"].AsString().empty()?"CyInsightOVR returned an unspecified error":result["error"].AsString());return result;
}
void MetaQuestCapture::PullTiles(const std::string& id,const fs::path& destination){
    Check(Token(id,"[0-9a-f-]{36}"),"Invalid companion recording identity");Call({L"-s",Wide(device_),L"pull",Wide("/data/local/tmp/cyinsightovr-recordings/"+id),destination.wstring()},120000,true);
}
std::string MetaQuestCapture::DescribeStartup(const std::string& activities,const std::string& power){
    if(power.find("mWakefulness=Asleep")!=std::string::npos||power.find("mWakefulness=Dozing")!=std::string::npos)
        return "Quest is asleep; put on the headset and return to the game";
    // Activity history contains old dialogs too. Only a resumed dialog is a
    // current blocker, and multiple Quest displays can have resumed activities.
    std::istringstream lines(activities);std::string line;bool controllers=false,guardian=false;
    while(std::getline(lines,line))if(line.find("topResumedActivity=")!=std::string::npos||line.find("mResumedActivity:")!=std::string::npos){
        controllers|=line.find("LaunchCheckControllerRequiredDialogActivity")!=std::string::npos;
        guardian|=line.find("GuardianDialogActivity")!=std::string::npos;
    }
    if(guardian)return "Meta play-area confirmation is open; validate it in the headset";
    if(controllers)return "Meta controllers-required dialog is open; activate the controllers and validate it in the headset";
    return {};
}
std::string MetaQuestCapture::StartupHint(){
    if(adb_.empty()||device_.empty()||Stopping())return {};
    try{return DescribeStartup(Shell({L"dumpsys",L"activity",L"activities"},3000),Shell({L"dumpsys",L"power"},3000));}
    catch(...){return {};}
}
bool MetaQuestCapture::Prepare(const MetaQuestOptions& options,const std::string& ipv4,const cygi::Json& metadata){
    try {
        Check(SelectDevice(options,ipv4,metadata),message_.c_str());
        std::string sources;try{sources=Shell({L"perfetto",L"--query",L"--long"});}catch(...){if(Stopping())throw;sources=Shell({L"perfetto",L"--query"});}
        track_stage_source_=sources.find("gpu_renderstage")!=std::string::npos;
        stage_source_advertised_=track_stage_source_||sources.find("gpu.renderstages.oculus")!=std::string::npos;counter_source_advertised_=sources.find("gpu.counters.oculus")!=std::string::npos;
        Check(stage_source_advertised_,"Meta render-stage source unavailable on this headset");
        // A no-restart capture must preserve the running game's driver/context
        // setup. Read the current state before issuing any activation command.
        try {
            profiling_state_=Shell({L"ovrgpuprofiler",L"-i"}).substr(0,4096);
            if(profiling_state_.find("Detailed GPU profiling is enabled")!=std::string::npos)profiling_verification_="enabled";
            else if(profiling_state_.find("Detailed GPU profiling is disabled")!=std::string::npos)profiling_verification_="disabled";
            else profiling_verification_="unrecognized_reply";
        }catch(const std::exception& e){if(Stopping())throw;profiling_verification_="query_failed";profiling_state_=e.what();}
        if(options.restart_game){
            activation_=Shell({L"ovrgpuprofiler",L"-e",Wide(package_)}).substr(0,4096);
            const auto verified=Shell({L"ovrgpuprofiler",L"-i"}).substr(0,4096);profiling_state_=verified;
            if(verified.find("Detailed GPU profiling is enabled")!=std::string::npos)profiling_verification_="enabled";
            else if(verified.find("Detailed GPU profiling is disabled")!=std::string::npos)profiling_verification_="disabled";
        }else activation_="Existing profiling configuration preserved; no activation or game restart requested";
        Check(profiling_verification_!="disabled",options.restart_game?"Meta detailed profiling remains disabled after activation; check the Quest driver response":"Meta detailed profiling is disabled. Native capture continues; enable profiling before game launch, or explicitly select restart in capture options");
        if(options.restart_game){
            // Resolve the package's real launcher before stopping it. Monkey is
            // a random input generator; successful injection does not identify
            // which activity launched or establish capture-service readiness.
            const auto resolved=Shell({L"cmd",L"package",L"resolve-activity",L"--brief",L"-a",L"android.intent.action.MAIN",L"-c",L"android.intent.category.LAUNCHER",Wide(package_)});
            std::istringstream lines(resolved);std::string line;
            while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();const auto slash=line.find('/');if(slash!=std::string::npos&&line.substr(0,slash)==package_&&Token(line,"[A-Za-z0-9_.]+/[A-Za-z0-9_.$]+"))launch_component_=line;}
            Check(!launch_component_.empty(),"Cannot resolve the selected game's launcher activity; game was not stopped. Launch it manually after enabling detailed profiling");
            message_="Meta profiling enabled; restarting the selected Quest game before capture";Shell({L"am",L"force-stop",Wide(package_)});restarted_=true;
            const auto launched=Shell({L"am",L"start",L"-n",Wide(launch_component_),L"-a",L"android.intent.action.MAIN",L"-c",L"android.intent.category.LAUNCHER"});
            Check(launched.find("Error:")==std::string::npos&&launched.find("Exception")==std::string::npos,"Android refused the selected game's launcher activity; check capture diagnostics and headset permissions");
        }
        prepared_=true;message_=options.restart_game?"Meta GPU profiling enabled; waiting for restarted game":profiling_verification_=="enabled"?"Existing Meta GPU profiling preserved; capturing the running game without restart":"Meta GPU profiling state unverified; capturing the running game without restart";return true;
    }catch(const std::exception& e){message_=e.what();return false;}
}
bool MetaQuestCapture::Begin(const fs::path& staging,double seconds,const cygi::Json& metadata){
    staging_=staging;try {
        cpu_frequency_=metadata["cpu_frequency_hz"].AsNumber();
        Check(prepared_,"Meta GPU capture was not prepared");
        for(int attempt=0;attempt<60&&game_pids_.empty();++attempt){try{const auto pid=Shell({L"pidof",Wide(package_)});std::istringstream pids(pid);uint32_t n=0;while(pids>>n)if(n)game_pids_.push_back(n);}catch(...){if(Stopping())throw;}if(game_pids_.empty())std::this_thread::sleep_for(std::chrono::milliseconds(100));}Check(!game_pids_.empty(),"Meta GPU target game is not running; check the headset launch dialog");
        pid_=uint32_t(metadata["process_id"].AsUInt());if(pid_)Check(std::find(game_pids_.begin(),game_pids_.end(),pid_)!=game_pids_.end(),"ADB target does not match the connected game process");else pid_=game_pids_.front();
        // A reboot invalidates all Android monotonic timestamps and process
        // ownership, even if the same ADB serial reconnects immediately.
        try{auto id=Shell({L"cat",L"/proc/sys/kernel/random/boot_id"});while(!id.empty()&&std::isspace(static_cast<unsigned char>(id.back())))id.pop_back();if(Token(id,"[0-9a-fA-F-]{36}"))boot_id_=id;}catch(...){if(Stopping())throw;}
        const auto id=staging.filename().u8string();Check(Token(id,"[A-Za-z0-9_-]+"),"Unsafe Meta staging identity");
        remote_config_="/data/misc/perfetto-configs/cyinsights-"+id+".cfg";remote_trace_="/data/misc/perfetto-traces/cyinsights-"+id+".pftrace";
        const auto config=cymeta::Config(seconds>0?uint32_t((seconds+30)*1000):86400000,track_stage_source_);auto configPath=staging_/"meta-gpu.cfg";std::ofstream out(configPath,std::ios::binary);out.write(config.data(),config.size());out.close();Check(bool(out),"Cannot save Meta GPU configuration");
        Call({L"-s",Wide(device_),L"push",configPath.wstring(),Wide(remote_config_)});
        const auto result=Shell({L"perfetto",L"--background-wait",L"-c",Wide(remote_config_),L"-o",Wide(remote_trace_)},40000);
        std::istringstream lines(result);std::string line;while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(Token(line,"[0-9]+"))perfetto_pid_=uint32_t(std::stoul(line));}Check(perfetto_pid_>1,"Perfetto did not return an owned process id");
        message_=track_stage_source_?"Meta GPU high-precision render stages and counters recording":"Meta GPU legacy render stages and counters recording; fine detail must be verified";return true;
    }catch(const std::exception& e){message_=e.what();return false;}
}
void MetaQuestCapture::End(){
    if(ended_)return;ended_=true;
    bool safeCleanup=perfetto_pid_==0;
    try{if(perfetto_pid_){
        // Never signal a stale/reused PID. Verify our generated config identity
        // in the process command line, then wait for its graceful file flush.
        if(!boot_id_.empty()){
            try{auto id=Shell({L"cat",L"/proc/sys/kernel/random/boot_id"},5000,true);while(!id.empty()&&std::isspace(static_cast<unsigned char>(id.back())))id.pop_back();if(Token(id,"[0-9a-fA-F-]{36}"))finished_boot_id_=id;}catch(...){}
            Check(finished_boot_id_.empty()||finished_boot_id_==boot_id_,"Quest restarted during GPU capture; timestamps belong to a previous boot and the recording is incomplete");
        }
        bool owned=false;try{owned=Shell({L"cat",Wide("/proc/"+std::to_string(perfetto_pid_)+"/cmdline")},5000,true).find(remote_config_)!=std::string::npos;}catch(...){}
        Check(owned,"Meta GPU: cannot verify the recording process; raw file retained on the Quest");
        Shell({L"kill",L"-TERM",std::to_wstring(perfetto_pid_)},5000,true);
        bool flushed=false;
        for(int i=0;i<100;++i){
            // The numeric PID is generated locally, never shell input from a client.
            const auto command=L"if test -d /proc/"+std::to_wstring(perfetto_pid_)+L"; then echo alive; else echo gone; fi";
            // adb shell already sends a command string to the remote shell.
            // An extra unquoted sh -c reparses `then` in the outer shell and
            // aborts recovery of an otherwise valid GPU trace.
            auto state=Shell({command},5000,true);
            if(state.find("gone")!=std::string::npos){flushed=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        Check(flushed,"Meta GPU file is still flushing; raw file retained on the Quest");
        safeCleanup=true;perfetto_pid_=0;
        Call({L"-s",Wide(device_),L"pull",Wide(remote_trace_),(staging_/"meta-gpu.pftrace").wstring()},60000,true);
        cymeta::Decoder decoder;auto counts=decoder.Read(staging_/"meta-gpu.pftrace");stages_=counts.stages;fine_stages_=counts.fine_stages;stage_types_=std::move(counts.stage_types);counters_=counts.counters;stage_packets_=counts.stage_packets;timed_stage_packets_=counts.timed_stage_packets;unaligned_stages_=counts.unaligned_stages;result_=stages_||counters_?"measured":"unavailable";message_="Meta GPU: "+std::to_string(stages_)+" hardware stages, "+std::to_string(counters_)+" valid counter samples; "+std::to_string(fine_stages_)+" Binning/Render/Load/Store stages";
        if(stages_&&!fine_stages_)message_+=". Only coarse/auxiliary GPU events received; detailed render costs unavailable";
        if(!stages_)message_+=timed_stage_packets_?". Stage packets received but unavailable after clock/dictionary validation; raw trace retained":". No timed hardware-stage packets received. Detailed profiling must precede game launch; source availability does not prove recording";
    }}catch(const std::exception& e){message_=e.what();}
    if(!staging_.empty()){auto info=Source();info["process_id"]=int(pid_);info["process_ids"]=cygi::Json::Array();for(auto pid:game_pids_)info["process_ids"].Push(int(pid));info["android_package"]=package_;info["clock"]="android.monotonic";info["unreal_cpu_frequency_hz"]=cpu_frequency_;info["unreal_session_id"]=session_;std::ofstream(staging_/"meta-gpu.info.json")<<info.Write(2);}
    if(safeCleanup&&!remote_config_.empty())try{Shell({L"rm",L"-f",Wide(remote_config_),Wide(remote_trace_)},5000,true);}catch(...){}
}
MetaQuestCapture::~MetaQuestCapture(){try{End();}catch(...){}}
cygi::Json MetaQuestCapture::Source()const{
    auto source=cygi::Json::Object();source["id"]="meta.quest.gpu";source["status"]=result_;source["reason"]=message_;source["collector"]=track_stage_source_?"Perfetto track_event/gpu_renderstage + gpu.counters.oculus":"Perfetto gpu.renderstages.oculus + gpu.counters.oculus";
    source["render_stage_transport"]=track_stage_source_?"track_event/gpu_renderstage":"legacy";
    source["hardware_detail_status"]=fine_stages_?"measured":stages_?"coarse_only":"unavailable";source["fine_stage_count"]=std::to_string(fine_stages_);
    source["stage_types"]=cygi::Json::Array();for(const auto& pair:stage_types_){auto item=cygi::Json::Object();item["name"]=pair.first;item["count"]=std::to_string(pair.second);source["stage_types"].Push(item);}
    source["stage_count"]=std::to_string(stages_);source["counter_sample_count"]=std::to_string(counters_);source["stage_packet_count"]=std::to_string(stage_packets_);source["timed_stage_packet_count"]=std::to_string(timed_stage_packets_);source["unaligned_stage_count"]=std::to_string(unaligned_stages_);
    source["profiling_driver_status"]=profiling_verification_;source["profiling_driver_reply"]=profiling_state_;source["profiling_activation_reply"]=activation_;
    source["stage_source_advertised"]=stage_source_advertised_;source["counter_source_advertised"]=counter_source_advertised_;source["target_restarted_after_activation"]=restarted_;
    source["restart_launch_component"]=launch_component_;
    source["android_boot_id"]=boot_id_;source["android_boot_id_at_finish"]=finished_boot_id_;source["device_restarted"]=!boot_id_.empty()&&!finished_boot_id_.empty()&&boot_id_!=finished_boot_id_;
    source["target_context_profiling"]=restarted_?"restart_requested; context not independently verified":"unverified: driver status does not prove the existing game context is instrumented";
    source["alignment"]="Perfetto clock snapshots to Android MONOTONIC, then native CyInsights.Sync; unavailable without valid anchors";source["frame_identity"]="unavailable: execution time is not a game frame id";source["hardware_stage_status"]=stages_?"measured":"unavailable";source["counter_status"]=counters_?"measured":"unavailable";
    std::error_code ec;if(!staging_.empty()&&fs::is_regular_file(staging_/"meta-gpu.pftrace",ec))source["path"]="meta-gpu.pftrace";return source;
}
}
