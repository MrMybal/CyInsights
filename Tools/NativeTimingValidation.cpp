// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeTrace.hpp"
#include "TimingModel.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <map>
#include <tuple>
#include <cstring>
using namespace cyinsights;
struct Digest {uint64_t count=0,sum=0,xor_value=0,unfinished=0;};
using Key=std::tuple<std::string,std::string,uint64_t,int,uint64_t>;
struct Result {std::map<Key,Digest> entries;uint64_t count=0;double seconds=0;};
static uint64_t Mix(uint64_t v){v^=v>>30;v*=0xbf58476d1ce4e5b9ull;v^=v>>27;v*=0x94d049bb133111ebull;return v^(v>>31);}
static Result Scan(const std::filesystem::path& file){
 Result result;std::string error;auto begin=std::chrono::steady_clock::now();
 auto consume=[&](TimingEvent&& e){
  // Epic's timestamp CSV has 7 decimal places. Compare at its precision;
  // the native binary itself retains the full provider doubles.
  uint64_t a=static_cast<uint64_t>(static_cast<int64_t>(std::llround(e.start*1e7))),b=e.open_ended?UINT64_MAX:static_cast<uint64_t>(static_cast<int64_t>(std::llround(e.end*1e7)));
  auto hash=Mix(a)^Mix(b+11)^Mix(uint64_t(e.depth)+37)^Mix(e.frame_type>=0?e.frame_index:e.timer_id);
  const uint64_t thread=TimingMatches(e.thread,"gpu")?0:e.thread_id;
  auto& entry=result.entries[{e.thread,e.frame_type>=0?"Native frame":e.name,e.frame_type>=0?0:e.timer_id,e.frame_type,thread}];++entry.count;entry.unfinished+=e.open_ended;entry.sum+=hash;entry.xor_value^=hash;++result.count;
 };
 bool ok=file.extension()==".cynt"?NativeTrace::ScanBinary(file,consume,error):NativeTrace::ScanCsv(file,consume,error);if(!ok)throw std::runtime_error(error);
 result.seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();return result;
}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"Usage: CyNativeTimingValidation timing.cynt timing.csv\n";return 2;}
 try{auto native=Scan(std::filesystem::u8path(argv[1])),csv=Scan(std::filesystem::u8path(argv[2]));bool matches=native.count==csv.count&&native.entries.size()==csv.entries.size();size_t differences=0;
 for(const auto& [key,value]:native.entries){auto i=csv.entries.find(key);if(i==csv.entries.end()||i->second.count!=value.count||i->second.unfinished!=value.unfinished||i->second.sum!=value.sum||i->second.xor_value!=value.xor_value){matches=false;++differences;if(differences<=8)std::cerr<<std::get<0>(key)<<" / "<<std::get<1>(key)<<": mismatch\n";}}
 std::cout<<std::setprecision(10)<<"{\"native_events\":"<<native.count<<",\"csv_events\":"<<csv.count<<",\"native_groups\":"<<native.entries.size()<<",\"csv_groups\":"<<csv.entries.size()<<",\"native_scan_seconds\":"<<native.seconds<<",\"csv_scan_seconds\":"<<csv.seconds<<",\"matching_100ns_event_digests\":"<<(matches?"true":"false")<<",\"different_groups\":"<<differences<<"}\n";
 return matches?0:1;
 }catch(const std::exception& e){std::cerr<<e.what();return 3;}
}
