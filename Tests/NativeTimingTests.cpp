// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeTrace.hpp"
#include "PagedTimingStore.hpp"
#include "../Tools/NativeTiming/Format.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>
#include <limits>
using namespace cyinsights;
int main(){
 auto root=std::filesystem::temp_directory_path()/("CyNativeTimingTests_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(root);
 auto source=root/"timing.cynt",index=root/"timing.cyindex";std::string error;int checks=0,failures=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}};
 std::ofstream out(source,std::ios::binary);auto value=[&](auto v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));};
 auto string=[&](std::string s){value(static_cast<uint32_t>(s.size()));out.write(s.data(),s.size());};
 out.write(cynt::magic,8);value(uint32_t(3));value(uint64_t(17));string("Outer");value(uint64_t(23));string("Child, \"quoted\" / é");value(uint64_t(24));string("Still running");
 value(uint32_t(2));value(uint64_t(91));string("GameThread");value(uint64_t(3));
 const auto first_event=static_cast<size_t>(out.tellp());
 value(cynt::Event{-1,4,0,0,0,0});value(cynt::Event{0,1,1,1,0,0});value(cynt::Event{2,INFINITY,2,1,1,0});
 value(uint64_t(1ull<<63));string("GPU");value(uint64_t(1));value(cynt::Event{0,2,1,0,0,0});
 value(uint64_t(2));value(cynt::Frame{-1,4,UINT64_MAX-1,0,0});value(cynt::Frame{-1,INFINITY,UINT64_MAX,1,0});
 out.write(cynt::footer,8);value(uint64_t(6));out.close();
 NativeReadProgress progress;std::vector<TimingEvent> events;auto read=[&]{events.clear();error.clear();return NativeTrace::ScanBinary(source,[&](TimingEvent&& e){events.push_back(std::move(e));},error,&progress);};
 check(read()&&events.size()==6,"Binary decoder retains scopes and both native frame types");
 check(events.size()==6&&events[1].name=="Child, \"quoted\" / é"&&events[1].timer_id==23&&events[3].thread_id==(1ull<<63),"UTF-8 dictionary names and distinct CPU/GPU identities survive decoding");
 check(events.size()==6&&events[2].open_ended&&events[5].open_ended&&events[4].frame_index==UINT64_MAX-1&&events[4].frame_type==0,"Incomplete durations and full-width native frame ordinals survive decoding");
 check(progress.events==6&&progress.bytes==progress.total,"Binary progress reports exact byte and event counts");
 auto store=PagedTimingStore::Build(source,index,progress,error,true);
 check(store&&store->count==6&&store->frames_native&&store->frames.size()==1&&store->frame_ids[0]==UINT64_MAX-1&&store->relations_valid,"Binary input uses the same paged index with native frames and valid nesting");
 if(store){auto window=store->Window(-1,4,-1,4,2000,nullptr,1);TimingModel model;model.Build(window.events);store->ApplyTotals(window,model);auto summary=store->Summary(window,model);
  auto outer=std::find_if(summary.begin(),summary.end(),[](const auto& s){return s.group.key.name=="Outer";});
  check(outer!=summary.end()&&outer->inclusive==5&&outer->exclusive==4,"Direct indexing preserves measured inclusive/exclusive cost without inventing unfinished child time");
  auto gpu=std::find_if(summary.begin(),summary.end(),[](const auto& s){return s.group.key.gpu;});check(gpu!=summary.end()&&gpu->count==1&&gpu->inclusive==2,"GPU cost remains a separate measured domain");
 }
 std::ifstream input(source,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});input.close();
 auto replace=[&](const std::string& content){std::ofstream file(source,std::ios::binary);file.write(content.data(),content.size());};
 replace(bytes.substr(0,bytes.size()-1));check(!read(),"Truncated footer is rejected");
 replace(bytes+"x");check(!read(),"Trailing payload is rejected");
 auto bad=bytes;bad[7]='2';replace(bad);check(!read(),"Unknown binary version is rejected");
 bad=bytes;uint64_t wrong=7;std::memcpy(bad.data()+bad.size()-8,&wrong,8);replace(bad);check(!read(),"Mismatching completion event count is rejected");
 bad=bytes;uint32_t oversized=UINT32_MAX;std::memcpy(bad.data()+20,&oversized,4);replace(bad);check(!read(),"Dictionary lengths are bounded by actual file bytes before allocation");
 bad=bytes;std::memcpy(bad.data()+first_event+16,&oversized,4);replace(bad);check(!read(),"Out-of-range timer dictionary references are rejected");
 bad=bytes;uint32_t reserved=1;std::memcpy(bad.data()+first_event+28,&reserved,4);replace(bad);check(!read(),"Reserved event bits are rejected");
 bad=bytes;double invalid=std::numeric_limits<double>::quiet_NaN();std::memcpy(bad.data()+first_event,&invalid,8);replace(bad);check(!read(),"Non-finite timing starts are rejected");
 replace(bytes);progress.cancel=true;check(!read(),"Direct binary decoding honors cancellation");progress.cancel=false;
 replace(bytes.substr(0,bytes.size()-1));auto existing=std::filesystem::file_size(index);check(!PagedTimingStore::Build(source,index,progress,error,true)&&std::filesystem::file_size(index)==existing&&!std::filesystem::exists(index.wstring()+L".partial"),"Incomplete decoding never replaces a published cache");
 std::filesystem::remove_all(root);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
