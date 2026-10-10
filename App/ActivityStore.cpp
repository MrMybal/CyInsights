// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ActivityStore.hpp"
#include "NativeTrace.hpp"
#include <fstream>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <limits>
#include <stdexcept>
namespace cyinsights {
namespace {
void Check(const std::atomic<bool>* c){if(c&&c->load())throw std::runtime_error("Activity analysis cancelled");}
uint64_t Number(const std::string& s){size_t used=0;if(s.empty()||s[0]=='-')throw std::runtime_error("Invalid activity integer");auto n=std::stoull(s,&used);if(used!=s.size())throw std::runtime_error("Invalid activity integer");return n;}
uint32_t Small(const std::string& s){auto n=Number(s);if(n>UINT32_MAX)throw std::runtime_error("Invalid activity identifier");return static_cast<uint32_t>(n);}
double Time(const std::string& s){size_t used=0;double n=std::stod(s,&used);if(used!=s.size()||!std::isfinite(n))throw std::runtime_error("Invalid activity timestamp");return n;}
std::string Lower(std::string s){for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
void Add(uint64_t& sum,uint64_t v){if(v>UINT64_MAX-sum)throw std::runtime_error("Activity aggregate overflow");sum+=v;}
}
std::shared_ptr<ActivityStore> ActivityStore::Build(const std::filesystem::path& journal,const std::filesystem::path& binary,const std::atomic<bool>* cancel){
 auto store=std::make_shared<ActivityStore>();store->file_=binary;auto partial=binary;partial+=L".partial";
 try{
  std::ifstream input(journal);std::ofstream out(partial,std::ios::binary);std::string line;
  if(!input||!out||!std::getline(input,line))throw std::runtime_error("Cannot open activity journal/cache");if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="CYACTIVITY1")throw std::runtime_error("Unsupported activity journal version");out.write("CYACTI01",8);
  std::unordered_map<std::string,uint32_t> names;std::vector<ActivityRecord> batch;uint32_t batch_lane=UINT32_MAX;
  auto flush=[&]{if(batch.empty())return;double a=batch.front().start,b=batch.front().end;for(auto& r:batch){a=std::min(a,r.start);b=std::max(b,r.end);}auto offset=static_cast<uint64_t>(out.tellp());out.write(reinterpret_cast<const char*>(batch.data()),batch.size()*sizeof(ActivityRecord));if(!out)throw std::runtime_error("Activity cache write failed");store->indexes_[batch_lane].blocks.push_back({offset,static_cast<uint32_t>(batch.size()),a,b});batch.clear();};
  bool complete=false;
  while(std::getline(input,line)){Check(cancel);if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;auto c=NativeTrace::CsvFields(line);
   if(c[0]=="End"){if(c.size()!=2||Number(c[1])!=store->count)throw std::runtime_error("Incomplete activity count");complete=true;break;}
   if(c[0]=="Lane"){
    if(c.size()!=5||Small(c[1])!=store->lanes.size()||(c[2]!="network"&&c[2]!="storage"&&c[2]!="loading"&&c[2]!="memory")||(c[4]!="bytes"&&c[4]!="events"))throw std::runtime_error("Invalid activity lane");
    store->lanes.push_back({c[2],c[3],c[4],0});store->indexes_.emplace_back();continue;
   }
   if(c[0]!="Event"||c.size()!=9)throw std::runtime_error("Invalid activity row");auto lane=Small(c[1]);if(lane>=store->lanes.size())throw std::runtime_error("Unknown activity lane");
   ActivityRecord r;r.start=Time(c[2]);r.end=Time(c[3]);r.value=Number(c[4]);r.identity=Number(c[5]);r.thread=Small(c[6]);r.flags=Small(c[7]);
   if(r.end<r.start||(r.flags&~127u)||((r.flags&ActivitySignedGauge)&&!(r.flags&ActivityGauge))||((r.flags&ActivityPoint)&&r.start!=r.end))throw std::runtime_error("Invalid activity interval/flags");
   auto name=names.try_emplace(c[8],static_cast<uint32_t>(store->names.size()));if(name.second)store->names.push_back(c[8]);r.name=name.first->second;
   if(!store->count)store->start=r.start;store->start=std::min(store->start,r.start);store->end=std::max(store->end,r.end);++store->count;++store->lanes[lane].count;
   if(batch_lane!=lane){flush();batch_lane=lane;}batch.push_back(r);if(batch.size()==4096)flush();
  }
  if(!complete||input.bad())throw std::runtime_error("Activity journal incomplete; original trace preserved");while(std::getline(input,line))if(!line.empty()&&line!="\r")throw std::runtime_error("Trailing activity data");flush();out.close();if(!out)throw std::runtime_error("Activity cache close failed");Check(cancel);
  for(auto& index:store->indexes_){std::stable_sort(index.blocks.begin(),index.blocks.end(),[](auto& x,auto& y){return x.start<y.start;});double end=-std::numeric_limits<double>::infinity();for(auto& block:index.blocks){end=std::max(end,block.end);index.prefix_end.push_back(end);}}
  std::filesystem::rename(partial,binary);return store;
 }catch(...){std::error_code ec;std::filesystem::remove(partial,ec);throw;}
}
ActivityWindow ActivityStore::Query(double a,double b,const std::vector<uint32_t>& ids,unsigned pixels,const std::string& search,uint64_t page,const std::atomic<bool>* cancel)const{
 Check(cancel);if(!std::isfinite(a)||!std::isfinite(b)||b<=a||pixels==0||pixels>16384||page>UINT64_MAX/128)throw std::runtime_error("Invalid activity query");
 ActivityWindow result;result.start=a;result.end=b;std::ifstream input(file_,std::ios::binary);if(!input)throw std::runtime_error("Activity cache missing; reopen capture");std::vector<ActivityRecord> batch;
 std::vector<uint8_t> matches(names.size(),1);if(!search.empty())for(size_t i=0;i<names.size();++i)matches[i]=Lower(names[i]).find(Lower(search))!=std::string::npos;
 std::vector<uint32_t> unique=ids;std::sort(unique.begin(),unique.end());unique.erase(std::unique(unique.begin(),unique.end()),unique.end());
 for(auto lane:unique){if(lane>=lanes.size())throw std::runtime_error("Invalid activity lane query");ActivityLaneWindow w;w.lane=lane;w.bins.resize(pixels);const auto& index=indexes_[lane];
  auto first=std::lower_bound(index.prefix_end.begin(),index.prefix_end.end(),a)-index.prefix_end.begin();
  for(size_t i=static_cast<size_t>(first);i<index.blocks.size()&&index.blocks[i].start<b;++i){const auto& block=index.blocks[i];if(block.end<a)continue;Check(cancel);++result.blocks_read;
   batch.resize(block.count);input.clear();input.seekg(static_cast<std::streamoff>(block.offset));input.read(reinterpret_cast<char*>(batch.data()),batch.size()*sizeof(ActivityRecord));if(!input)throw std::runtime_error("Activity cache read failed");
   for(const auto& r:batch){bool instant=r.start==r.end;if(r.start>=b||(instant?r.start<a:r.end<=a)||!matches[r.name])continue;
    uint64_t row=w.matched++;if(row>=page*128&&w.rows.size()<128)w.rows.push_back(r);
    w.content+=(r.flags&ActivityContentBits)!=0;w.points+=(r.flags&ActivityPoint)!=0;
    if(!(r.flags&ActivityIncomplete)){++w.complete;if(!(r.flags&(ActivityGauge|ActivityBucketCount)))w.inclusive_seconds+=std::max(0.,std::min(b,r.end)-std::max(a,r.start));}
    if(!(r.flags&ActivityContentBits)){if(w.matched-w.content==1){w.minimum=w.maximum=r.value;}else {w.minimum=std::min(w.minimum,r.value);w.maximum=std::max(w.maximum,r.value);}if(!(r.flags&(ActivityGauge|ActivityBucketCount))&&r.start>=a)Add(w.value_sum,r.value);}
    if(r.flags&ActivitySignedGauge){auto v=static_cast<int64_t>(r.value);if(!w.signed_values){w.signed_minimum=w.signed_maximum=v;w.signed_values=true;}else{w.signed_minimum=std::min(w.signed_minimum,v);w.signed_maximum=std::max(w.signed_maximum,v);}}
    auto pixel=[&](double t){return static_cast<unsigned>(std::clamp((t-a)/(b-a)*pixels,0.,double(pixels-1)));};
    auto lo=pixel(r.start),hi=instant?lo:pixel(std::nextafter(std::min(b,r.end),a));
    for(unsigned x=lo;x<=hi;++x){auto& bin=w.bins[x];++bin.count;if(r.flags&ActivityContentBits)continue;double v=r.flags&ActivitySignedGauge?static_cast<double>(static_cast<int64_t>(r.value)):static_cast<double>(r.value);if(!bin.valid){bin.minimum=bin.maximum=v;bin.valid=true;}else {bin.minimum=std::min(bin.minimum,v);bin.maximum=std::max(bin.maximum,v);}}
   }
  }result.lanes.push_back(std::move(w));
 }return result;
}
}
