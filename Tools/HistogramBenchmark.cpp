// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "PagedTimingStore.hpp"
#include "NativeTrace.hpp"
#include "Distribution.hpp"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cstring>
using namespace cyinsights;
using Clock=std::chrono::steady_clock;
static double Ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
int main(int argc,char** argv){
 if(argc!=3){std::cerr<<"Usage: CyHistogramBenchmark timing.csv disposable.cyindex\n";return 2;}
 NativeReadProgress progress;std::string error;auto begin=Clock::now();auto store=PagedTimingStore::Build(std::filesystem::u8path(argv[1]),std::filesystem::u8path(argv[2]),progress,error);
 if(!store){std::cerr<<error;return 1;}std::cout<<std::setprecision(17)<<"events,"<<store->count<<",index_ms,"<<Ms(begin)<<'\n';
 std::cout<<"function,samples,generic_ms,indexed_ms,generic_records,indexed_records,full_bin_ms,cached_page_ms,full_visits,cached_visits,fingerprint\n";
 for(const auto* name:{"FEngineLoop::Tick","ExecuteForegroundTask","MobileSceneRender","CyAbsentFunction"}){
  auto mask=store->FunctionMask(name);std::vector<double> generic_ms,indexed_ms,full_ms,cached_ms;uint64_t expected_count=0,expected_hash=0;
  ScopeVisitMetrics generic_metrics,indexed_metrics;Distribution base;
  for(int repeat=0;repeat<3;++repeat){
   auto scan=[&](bool indexed,ScopeVisitMetrics& metrics){uint64_t count=0,hash=14695981039346656037ull;
    auto integer=[&](uint64_t n){for(int byte=0;byte<8;++byte){hash^=(n>>(byte*8))&255;hash*=1099511628211ull;}};
    store->VisitScopes(SIZE_MAX,store->start,store->end,false,[&](const StoredScope& s){if(!mask[s.timer])return;++count;uint64_t a,b;std::memcpy(&a,&s.start,8);std::memcpy(&b,&s.end,8);integer(a);integer(b);integer(s.timer);integer(s.track);integer(s.depth);integer(s.unfinished);},nullptr,indexed?&mask:nullptr,&metrics);
    return std::make_pair(count,hash);};
   store->ClearReadCache();begin=Clock::now();auto generic=scan(false,generic_metrics);generic_ms.push_back(Ms(begin));
   store->ClearReadCache();begin=Clock::now();auto indexed=scan(true,indexed_metrics);indexed_ms.push_back(Ms(begin));
   if(generic!=indexed){std::cerr<<"Function index changed native samples: "<<name;return 3;}expected_count=indexed.first;expected_hash=indexed.second;
   unsigned visits=0;SampleVisitor visitor=[&](const SampleConsumer& consume){++visits;store->VisitScopes(SIZE_MAX,store->start,store->end,false,[&](const StoredScope& s){consume({s.end-s.start,s.start,s.end,0,s.timer,!s.unfinished});},nullptr,&mask);};
   base=BuildDistribution(visitor,false,-1,0,nullptr,true);
   if(!base.count)continue;int bin=static_cast<int>(std::max_element(base.bins.begin(),base.bins.end())-base.bins.begin());
   store->ClearReadCache();visits=0;begin=Clock::now();auto full=BuildDistribution(visitor,false,bin,0,nullptr,true);full_ms.push_back(Ms(begin));if(visits!=2)return 4;
   store->ClearReadCache();visits=0;begin=Clock::now();auto cached=SelectDistributionSamples(visitor,base,bin,0);cached_ms.push_back(Ms(begin));if(visits!=1)return 5;
   if(full.count!=cached.count||full.bins!=cached.bins||full.percentiles!=cached.percentiles||full.matched!=cached.matched||full.examples.size()!=cached.examples.size())return 6;
   for(size_t i=0;i<full.examples.size();++i)if(full.examples[i].start!=cached.examples[i].start||full.examples[i].end!=cached.examples[i].end||full.examples[i].timer!=cached.examples[i].timer)return 7;
  }
  auto median=[](std::vector<double> values){if(values.empty())return 0.;std::sort(values.begin(),values.end());return values[values.size()/2];};
  std::cout<<name<<','<<expected_count<<','<<median(generic_ms)<<','<<median(indexed_ms)<<','<<generic_metrics.records_scanned<<','<<indexed_metrics.records_scanned<<','<<median(full_ms)<<','<<median(cached_ms)<<",2,1,"<<expected_hash<<'\n';
 }
}
