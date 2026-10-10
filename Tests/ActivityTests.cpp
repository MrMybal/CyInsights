// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ActivityStore.hpp"
#include "ActivityTrace.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <Windows.h>
using namespace cyinsights;
int main(int argc,char** argv){
 if(argc==4){if(std::string(argv[1]).find("crash")!=std::string::npos)ExitProcess(0xC0000005u);if(std::string(argv[1]).find("slow")!=std::string::npos)Sleep(10000);std::ofstream(argv[2])<<"CYACTIVITY1\nLane,0,network,Send,bytes\nEvent,0,1,1,12,0,4294967295,1,Packet\nEnd,1\n";return 0;}
 if(argc==3&&std::string(argv[1])=="--diagnose"){auto source=std::filesystem::u8path(argv[2]);auto binary=source;binary+=L".cyindex";auto store=ActivityStore::Build(source,binary);std::vector<uint32_t> ids;for(size_t i=0;i<store->lanes.size();++i)ids.push_back(static_cast<uint32_t>(i));auto result=store->Query(0,store->end+1,ids,100);std::cout<<"events="<<store->count<<" lanes="<<store->lanes.size()<<" blocks="<<result.blocks_read<<'\n';for(auto& l:result.lanes)std::cout<<store->lanes[l.lane].kind<<" observations="<<l.matched<<" values="<<l.value_sum<<" min="<<l.minimum<<" max="<<l.maximum<<'\n';return 0;}
 int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}};
 auto dir=std::filesystem::temp_directory_path()/("CyActivityTests_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(dir);auto journal=dir/"activity.csv",binary=dir/"activity.bin";
 std::ofstream out(journal);out<<"CYACTIVITY1\nLane,0,network,Send,bytes\nLane,1,storage,File operations,bytes\nLane,2,memory,Allocated,bytes\n";
 out<<"Event,0,1,1,100,7,4294967295,1,Send packet\nEvent,0,1,1,64,7,4294967295,9,\"RPC, \"\"Jump\"\"\"\nEvent,0,2,2,200,8,4294967295,1,Send packet\n";
 out<<"Event,1,0.5,2.5,4096,8192,3,0,Read\nEvent,1,3,3,0,0,3,2,Incomplete read\nEvent,2,0,1.5,18014398509482107,0,4294967295,16,Allocated\nEvent,2,1.5,3,100,0,4294967295,16,Allocated\n";
 for(int i=0;i<9000;++i)out<<"Event,1,"<<10+i<<','<<10.5+i<<",1,0,3,0,Later read\n";
 out<<"End,9007\n";out.close();auto store=ActivityStore::Build(journal,binary);
 check(store->count==9007&&store->lanes[1].count==9002,"All events indexed across disk blocks without an event acceptance limit");
 auto result=store->Query(1,2,{0,1,2},80);auto& net=result.lanes[0];auto& disk=result.lanes[1];auto& memory=result.lanes[2];
 check(net.matched==2&&net.content==1&&net.value_sum==100&&net.inclusive_seconds==0,"Packet contents use bits and do not double count bytes or fabricate durations; half-open packet range");
 check(store->names[net.rows[1].name]=="RPC, \"Jump\""&&net.rows[1].identity==7,"Quoted content name and native packet identity preserved");
 check(disk.matched==1&&disk.inclusive_seconds==1&&disk.value_sum==0&&disk.rows[0].identity==8192,"Crossing file operation survives interval filter; duration clipped, bytes only attributed at operation start");
 check(memory.matched==2&&memory.maximum==18014398509482107ull&&memory.minimum==100&&memory.value_sum==0&&memory.inclusive_seconds==0,"Memory sample interval crosses range, exact 64-bit values, no invented execution time or sum of gauges");
 check(result.blocks_read<5,"Distant blocks skipped by interval index");
 auto filtered=store->Query(0,4,{0},10,"jump");check(filtered.lanes[0].matched==1&&filtered.lanes[0].value_sum==0,"Case-insensitive content filter retains bits semantics");
 auto page=store->Query(10,9011,{1},20,{},1);check(page.lanes[0].matched==9000&&page.lanes[0].rows.size()==128,"All observations counted with bounded, paged UI details");
 auto duplicate=store->Query(0,4,{1,1},5);check(duplicate.lanes.size()==1,"Duplicate requested lanes do not duplicate aggregates");
 auto incomplete=store->Query(3,4,{1},3);check(incomplete.lanes[0].matched==1&&incomplete.lanes[0].complete==0&&incomplete.lanes[0].inclusive_seconds==0,"Incomplete operation has unavailable duration");
 std::atomic<bool> cancel{true};bool rejected=false;try{store->Query(0,4,{0},10,{},0,&cancel);}catch(...){rejected=true;}check(rejected,"Query cancellation honored");
 auto signedJournal=dir/"signed.csv";std::ofstream(signedJournal)<<"CYACTIVITY1\nLane,0,memory,LLM,bytes\nEvent,0,1,2,18446744073709551614,0,0,80,LLM\nEvent,0,2,3,7,0,0,80,LLM\nEnd,2\n";auto signedStore=ActivityStore::Build(signedJournal,dir/"signed.bin");auto signedQuery=signedStore->Query(0,4,{0},5);check(signedQuery.lanes[0].signed_values&&signedQuery.lanes[0].signed_minimum==-2&&signedQuery.lanes[0].signed_maximum==7,"Signed LLM observations preserved rather than omitted or displayed as huge unsigned memory values");
    auto bad=dir/"bad.csv";std::ofstream(bad)<<"CYACTIVITY1\nLane,0,network,Send,bytes\nEvent,0,nan,1,3,0,0,1,Packet\nEnd,1\n";rejected=false;try{ActivityStore::Build(bad,dir/"bad.bin");}catch(...){rejected=true;}check(rejected&&!std::filesystem::exists(dir/"bad.bin.partial")&&!std::filesystem::exists(dir/"bad.bin"),"Malformed timestamps rejected without publishing partial cache");
 std::ofstream(bad)<<"CYACTIVITY1\nLane,0,network,Send,bytes\nEvent,0,1,1,3,0,0,1,Packet\n";rejected=false;try{ActivityStore::Build(bad,dir/"bad.bin");}catch(...){rejected=true;}check(rejected,"Incomplete decoder export rejected");
 std::ofstream(dir/"UnrealInsights-TraceAnalysis.dll")<<"fixture";auto source=dir/"source.utrace";std::ofstream(source)<<"preserved";std::string error,message;std::shared_ptr<ActivityStore> decoded;ActivityTrace reader;
 check(reader.Start(std::filesystem::absolute(argv[0]),dir,source,dir/"output",error),"Decoder job starts in background");
 auto poll=[&]{auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(4);while(reader.Busy()&&std::chrono::steady_clock::now()<limit){reader.Poll(decoded,message);std::this_thread::sleep_for(std::chrono::milliseconds(1));}return !reader.Busy();};
 check(poll()&&decoded&&decoded->count==1,"Completed decoder result publishes an indexed store");
 auto crash=dir/"crash.utrace";std::ofstream(crash)<<"preserved";decoded.reset();
 check(reader.Start(std::filesystem::absolute(argv[0]),dir,crash,dir/"output",error)&&poll()&&!decoded&&message.find("0xC0000005")!=std::string::npos&&std::filesystem::file_size(crash)==9,"Decoder access violation reports its code, preserves source and publishes no partial result");
 auto slow=dir/"slow.utrace";std::ofstream(slow)<<"preserved";check(reader.Start(std::filesystem::absolute(argv[0]),dir,slow,dir/"output",error),"Owned decoder cancellation fixture starts");reader.Cancel();decoded.reset();check(poll()&&!decoded&&message.find("cancelled")!=std::string::npos,"Cancelling owned process publishes no partial result");
 check(std::filesystem::file_size(source)==9&&std::filesystem::file_size(slow)==9,"Original trace inputs remain unchanged");
 std::filesystem::remove_all(dir);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
