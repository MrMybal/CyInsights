// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Distribution.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
using namespace cyinsights;
int main() {
    int checks=0,failures=0;auto check=[&](bool v,const char* name){++checks;if(!v){++failures;std::cerr<<name<<'\n';}};
    auto near=[](double a,double b){return std::abs(a-b)<1e-9;};
    std::vector<DistributionSample> samples;for(uint64_t i=0;i<1000;++i)samples.push_back({double(i),double(i),double(i+1),i,0,true});
    samples.push_back({0,0,0,0,0,false});samples.push_back({NAN});samples.push_back({-1});
    SampleVisitor visit=[&](const SampleConsumer& consume){for(auto& s:samples)consume(s);};
    auto exact=BuildDistribution(visit,true);
    check(exact.count==1000&&exact.unavailable==3,"All valid values including zero counted; unavailable/nonfinite/negative excluded");
    check(exact.min==0&&exact.max==999&&near(exact.mean,499.5),"Exact min/max and complete-population mean");
    check(exact.percentiles==std::array<double,4>{499,899,949,989},"Exact nearest-rank percentiles use all frame samples");
    check(std::accumulate(exact.bins.begin(),exact.bins.end(),uint64_t{})==1000,"Histogram preserves all samples");
    check(exact.Bin(999)==63&&exact.Bounds(63).second==999,"Maximum belongs to last inclusive bin");
    check(exact.examples.size()==128&&exact.examples.front().index==999&&exact.examples.back().index==872,"Bounded slowest-values heap retains actual slowest samples");
    auto approximate=BuildDistribution(visit,false);
    bool bounded=true;for(size_t q=0;q<4;++q)bounded&=approximate.percentile_bounds[q].first<=exact.percentiles[q]&&approximate.percentile_bounds[q].second>=exact.percentiles[q];
    check(!approximate.exact_percentiles&&bounded,"Scope estimates expose bounds containing exact percentile");
    for(int bin=0;bin<64;++bin){auto chosen=BuildDistribution(visit,false,bin);check(chosen.matched==chosen.bins[static_cast<size_t>(bin)]&&std::all_of(chosen.examples.begin(),chosen.examples.end(),[&](auto s){return chosen.Bin(s.value)==static_cast<size_t>(bin);}),"Bin drill-down returns every matching sample for small populations");}
    std::vector<DistributionSample> constant;for(uint64_t i=0;i<401;++i)constant.push_back({.012,double(i),double(i+1),i});
    SampleVisitor same=[&](const SampleConsumer& c){for(auto s:constant)c(s);};auto flat=BuildDistribution(same,true);
    check(flat.bins[0]==401&&flat.min==flat.max&&near(flat.percentiles[3],.012),"Constant distributions do not divide by zero");
    std::vector<uint64_t> ids;for(uint64_t page=0;page<4;++page){auto selected=BuildDistribution(same,false,0,page);for(auto s:selected.examples)ids.push_back(s.index);}
    check(ids.size()==401&&std::adjacent_find(ids.begin(),ids.end())==ids.end()&&ids.front()==0&&ids.back()==400,"Paging enumerates entire bin without drops or duplicates");
    auto beyond=BuildDistribution(same,false,0,UINT64_MAX);check(beyond.examples.empty()&&beyond.matched==401,"Huge page offsets cannot wrap into existing samples");
    auto empty=BuildDistribution([](const SampleConsumer&){},true);check(empty.count==0&&empty.examples.empty(),"Empty data remains empty");
    auto large=BuildDistribution([](const SampleConsumer& c){c({1e308});c({1e308});},false);
    check(std::isfinite(large.mean)&&large.mean==1e308,"Mean remains finite for large finite values");
    auto extreme=BuildDistribution([](const SampleConsumer& c){c({1e308});c({1.2e308});},false);
    check(std::all_of(extreme.percentiles.begin(),extreme.percentiles.end(),[](double value){return std::isfinite(value);}),"Estimated quantile midpoints do not overflow");
    auto logarithmic=BuildDistribution(visit,true,-1,0,nullptr,true);
    check(std::accumulate(logarithmic.bins.begin(),logarithmic.bins.end(),uint64_t{})==1000&&logarithmic.Bin(0)==0&&logarithmic.Bin(999)==63,"Log duration bins retain zero, extremes and every sample");
    check(logarithmic.percentiles==exact.percentiles,"Log duration axis never changes exact quantiles");
    bool log_bounded=true;for(auto s:samples)if(s.valid&&std::isfinite(s.value)&&s.value>=0){auto bounds=logarithmic.Bounds(logarithmic.Bin(s.value));log_bounded&=s.value>=bounds.first-1e-9&&s.value<=bounds.second+1e-9;}
    check(log_bounded,"Log bin bounds contain original untransformed durations");
    unsigned visits=0;SampleVisitor counted=[&](const SampleConsumer& consume){++visits;for(auto s:constant)consume(s);};
    auto cached=SelectDistributionSamples(counted,flat,0,2);
    check(visits==1&&cached.count==401&&cached.matched==401&&cached.examples.size()==128&&cached.examples.front().index==256,"Cached pagination visits the immutable source only once and retains complete counts");
    check(cached.bins==flat.bins&&cached.percentiles==flat.percentiles&&cached.mean==flat.mean,"Pagination does not recompute or alter histogram statistics");
    auto past=SelectDistributionSamples(same,flat,0,UINT64_MAX);check(past.examples.empty()&&past.matched==401,"Cached huge page cannot wrap");
    bool changed=false;try{SelectDistributionSamples([](const SampleConsumer&){},flat,0,0);}catch(...){changed=true;}check(changed,"Cached summary cannot silently publish a changed source");
    bool invalid=false;try{SelectDistributionSamples(same,flat,64,0);}catch(...){invalid=true;}check(invalid,"Invalid bin rejected before reading samples");
    std::atomic<bool> cancel{true};bool cancelled=false;try{BuildDistribution(visit,false,-1,0,&cancel);}catch(...){cancelled=true;}
    check(cancelled,"Cancelled analysis never publishes partial distribution");
    cancelled=false;try{SelectDistributionSamples(same,flat,0,0,&cancel);}catch(...){cancelled=true;}check(cancelled,"Cached page respects cancellation before exposing values");
    auto tiny=BuildDistribution([](const SampleConsumer& consume){consume({0});consume({1e-310});},true);
    check(tiny.Bin(0)==0&&tiny.Bin(1e-310)==63,"Subnormal duration range cannot overflow its bin mapping");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
