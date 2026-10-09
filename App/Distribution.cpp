// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Distribution.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace cyinsights {
namespace {
constexpr double log_reference=.000001;
double LogValue(double value){return value>1?std::log(value)-std::log(log_reference)+std::log1p(log_reference/value):std::log1p(value/log_reference);}
}
size_t Distribution::Bin(double value) const {
    if(max<=min)return 0;
    auto sample=logarithmic_values?LogValue(value):value;
    if(!std::isfinite(bin_factor))return static_cast<size_t>(std::clamp((sample-bin_origin)/((logarithmic_values?LogValue(max):max)-bin_origin)*bin_count,0.,double(bin_count-1)));
    return static_cast<size_t>(std::clamp((sample-bin_origin)*bin_factor,0.,double(bin_count-1)));
}
std::pair<double,double> Distribution::Bounds(size_t bin) const {
    if(max<=min)return {min,max};
    if(logarithmic_values){auto first=LogValue(min),width=(LogValue(max)-first)/bin_count;
        auto inverse=[&](size_t edge){return std::clamp(std::exp(first+edge*width+std::log(log_reference))-log_reference,min,max);};
        return {bin?inverse(bin):min,bin+1==bin_count?max:inverse(bin+1)};}
    auto width=(max-min)/bin_count;return {min+bin*width,bin+1==bin_count?max:min+(bin+1)*width};
}
Distribution BuildDistribution(const SampleVisitor& visit,bool exact,int selected_bin,uint64_t page,const std::atomic<bool>* cancel,bool logarithmic_values) {
    auto check=[&]{if(cancel&&cancel->load())throw std::runtime_error("Distribution cancelled");};
    Distribution result;result.exact_percentiles=exact;result.logarithmic_values=logarithmic_values;result.selected_bin=selected_bin;result.page=page;
    std::vector<double> values;
    visit([&](const DistributionSample& sample){check();
        if(!sample.valid||!std::isfinite(sample.value)||sample.value<0){++result.unavailable;return;}
        if(!result.count)result.min=result.max=sample.value;
        result.min=std::min(result.min,sample.value);result.max=std::max(result.max,sample.value);
        ++result.count;result.mean+=(sample.value-result.mean)/static_cast<double>(result.count);if(exact)values.push_back(sample.value);
    });
    if(!result.count)return result;
    result.bin_origin=logarithmic_values?LogValue(result.min):result.min;
    auto last=logarithmic_values?LogValue(result.max):result.max;
    result.bin_factor=last>result.bin_origin?Distribution::bin_count/(last-result.bin_origin):0;
    const uint64_t skip=page>UINT64_MAX/Distribution::page_size?UINT64_MAX:page*Distribution::page_size;
    auto compare=[](const auto& a,const auto& b){return a.value>b.value||(a.value==b.value&&a.start<b.start);};
    visit([&](const DistributionSample& sample){check();if(!sample.valid||!std::isfinite(sample.value)||sample.value<0)return;
        auto bin=result.Bin(sample.value);++result.bins[bin];
        if(selected_bin>=0) {
            if(bin!=static_cast<size_t>(selected_bin))return;
            if(result.matched>=skip&&result.examples.size()<Distribution::page_size)result.examples.push_back(sample);
            ++result.matched;
        }else {
            // Keep the slowest page only; histogram/statistics still count all.
            if(result.examples.size()<Distribution::page_size){result.examples.push_back(sample);std::push_heap(result.examples.begin(),result.examples.end(),compare);}
            else if(compare(sample,result.examples.front())){std::pop_heap(result.examples.begin(),result.examples.end(),compare);result.examples.back()=sample;std::push_heap(result.examples.begin(),result.examples.end(),compare);}
        }
    });
    if(selected_bin<0){result.matched=result.count;std::sort(result.examples.begin(),result.examples.end(),compare);}
    const double quantiles[]={.5,.9,.95,.99};
    if(exact){check();std::sort(values.begin(),values.end());}
    for(size_t q=0;q<4;++q) {
        auto rank=static_cast<uint64_t>(std::max(1.,std::ceil(quantiles[q]*result.count)));
        if(exact){result.percentiles[q]=values[static_cast<size_t>(rank-1)];result.percentile_bounds[q]={result.percentiles[q],result.percentiles[q]};}
        else {uint64_t sum_count=0;for(size_t bin=0;bin<Distribution::bin_count;++bin){sum_count+=result.bins[bin];if(sum_count>=rank){auto bounds=result.Bounds(bin);result.percentile_bounds[q]=bounds;result.percentiles[q]=bounds.first+(bounds.second-bounds.first)*.5;break;}}}
    }
    check();return result;
}
Distribution SelectDistributionSamples(const SampleVisitor& visit,const Distribution& summary,int selected_bin,uint64_t page,const std::atomic<bool>* cancel) {
    if(selected_bin<0||selected_bin>=static_cast<int>(Distribution::bin_count))throw std::runtime_error("Invalid histogram bin");
    Distribution result=summary;result.examples.clear();result.selected_bin=selected_bin;result.page=page;result.matched=summary.bins[selected_bin];
    uint64_t matched=0,skip=page>UINT64_MAX/Distribution::page_size?UINT64_MAX:page*Distribution::page_size;
    auto check=[&]{if(cancel&&cancel->load())throw std::runtime_error("Distribution cancelled");};check();
    visit([&](const DistributionSample& sample){check();if(!sample.valid||!std::isfinite(sample.value)||sample.value<0||result.Bin(sample.value)!=static_cast<size_t>(selected_bin))return;
        if(matched>=skip&&result.examples.size()<Distribution::page_size)result.examples.push_back(sample);++matched;});
    if(matched!=result.matched)throw std::runtime_error("Distribution source changed; analyze again");check();return result;
}
}
