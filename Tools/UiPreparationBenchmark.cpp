// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "ActivityStore.hpp"
#include "CounterModel.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <Windows.h>
using namespace cyinsights;
using Clock=std::chrono::steady_clock;
template<class F> double Median(F work){std::vector<double> times;for(int i=0;i<7;++i){auto a=Clock::now();work();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-a).count());}std::sort(times.begin(),times.end());return times[times.size()/2];}
int main(){
 auto dir=std::filesystem::temp_directory_path()/("CyUiBenchmark_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(dir);
 auto csv=dir/"activity.csv";{std::ofstream out(csv);out<<"CYACTIVITY1\nLane,0,memory,Overlapping sample intervals,bytes\n";for(int i=0;i<100000;++i)out<<"Event,0,"<<(i%1000)/1000.<<",10,"<<i<<",0,0,16,Sample\n";out<<"End,100000\n";}
 auto store=ActivityStore::Build(csv,dir/"activity.bin");uint64_t activityFingerprint=0;auto activity=Median([&]{auto w=store->Query(0,10,{0},2048);activityFingerprint=w.lanes[0].matched;for(auto& b:w.lanes[0].bins)activityFingerprint+=b.count;});
 CounterModel counters;CounterSeries c;c.name="Synthetic counter";for(int i=0;i<1000000;++i)c.samples.push_back({i*.001,0,int64_t(i%10007)-5000,i%29!=0});counters.series.push_back(std::move(c));counters.Index();
 uint64_t counterFingerprint=0;double mean=0;
 auto stats=Median([&]{auto s=counters.Stats(0,50,950);counterFingerprint=s.count+s.unavailable;mean=s.mean;});
 auto plot=Median([&]{auto p=counters.Plot(0,50,950,2048);for(auto& b:p)counterFingerprint+=b.minimum+b.maximum;});
 std::cout<<"{\"synthetic\":true,\"activity_events\":100000,\"counter_samples\":1000000,\"pixels\":2048,\"activity_query_median_ms\":"<<activity<<",\"counter_stats_median_ms\":"<<stats<<",\"counter_plot_median_ms\":"<<plot<<",\"activity_fingerprint\":"<<activityFingerprint<<",\"counter_fingerprint\":"<<counterFingerprint<<",\"mean\":"<<mean<<"}\n";
 std::filesystem::remove_all(dir);
}
