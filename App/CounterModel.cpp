// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "CounterModel.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace cyinsights {
namespace {
std::vector<std::string> Row(const std::string& line) {
    std::vector<std::string> row;std::string cell;bool quoted=false,closed=false;
    for(size_t i=0;i<line.size();++i) {
        char c=line[i];if(c=='\r')continue;
        if(c=='"') {
            if(quoted&&i+1<line.size()&&line[i+1]=='"'){cell+='"';++i;}
            else if(quoted){quoted=false;closed=true;}
            else {if(!cell.empty()||closed)throw std::runtime_error("Malformed counter CSV quoting");quoted=true;}
        } else if(c==','&&!quoted){row.push_back(cell);cell.clear();closed=false;}
        else {if(closed)throw std::runtime_error("Unexpected text after counter CSV quote");cell+=c;}
    }
    if(quoted)throw std::runtime_error("Incomplete counter CSV quote");row.push_back(cell);return row;
}
std::vector<std::string> Header(std::istream& in) {
    std::string line;if(!std::getline(in,line))throw std::runtime_error("Missing counter CSV header");
    if(line.rfind("\xef\xbb\xbf",0)==0)line.erase(0,3);return Row(line);
}
int64_t Integer(const std::string& s) {
    size_t used=0;if(s.empty()||s.find_first_not_of("-0123456789")!=std::string::npos)throw std::runtime_error("Invalid integer counter value");
    auto value=std::stoll(s,&used);if(used!=s.size())throw std::runtime_error("Invalid integer counter value");return value;
}
double Real(const std::string& s) {
    size_t used=0;auto value=std::stod(s,&used);if(used!=s.size())throw std::runtime_error("Invalid counter number");return value;
}
bool ReadValues(const std::filesystem::path& file,CounterSeries& counter,size_t& total,uint64_t& bytes,const std::atomic<bool>* cancel) {
    try {
        auto size=std::filesystem::file_size(file);
        bytes+=size;
        std::ifstream in(file);if(!in)throw std::runtime_error("Counter values export missing");
        if(Header(in)!=std::vector<std::string>{"Time","Value"})throw std::runtime_error("Unsupported counter values header");
        std::vector<CounterSample> samples;std::string line;
        while(std::getline(in,line)) {
            if(line.empty()||line=="\r")continue;
            if(total%4096==0&&cancel&&cancel->load())throw std::runtime_error("Counter import cancelled");++total;
            const auto row=Row(line);if(row.size()!=2)throw std::runtime_error("Invalid counter sample row");
            CounterSample s;s.time=Real(row[0]);
            if(!std::isfinite(s.time)||s.time<0||(!samples.empty()&&s.time<samples.back().time))throw std::runtime_error("Invalid or unordered counter timestamps");
            if(counter.floating){s.real=Real(row[1]);s.valid=std::isfinite(s.real);}else s.integer=Integer(row[1]);
            samples.push_back(s);
        }
        if(!in.eof())throw std::runtime_error("Counter CSV read failed");
        counter.samples=std::move(samples);counter.reason=counter.samples.empty()?"No samples in this trace":"";return true;
    }catch(const std::exception& e){counter.reason=e.what();counter.samples.clear();return false;}
}
}
size_t CounterSeries::Lower(double time) const {
    return static_cast<size_t>(std::lower_bound(samples.begin(),samples.end(),time,[](const CounterSample& s,double t){return s.time<t;})-samples.begin());
}
std::string CounterSeries::Text(const CounterSample& s) const {
    if(!s.valid)return "Unavailable";
    if(!floating)return std::to_string(s.integer);
    std::ostringstream out;out<<std::setprecision(9)<<s.real;return out.str();
}
bool CounterModel::CanExport(const std::string& name) {
    // Epic's command accepts name wildcards/comma lists, not native IDs. Names
    // that cannot express an exact query remain unavailable rather than guessed.
    return !name.empty()&&name.size()<=1024&&name.find_first_of("*?,\"\\\r\n\0",0,8)==std::string::npos &&
        std::none_of(name.begin(),name.end(),[](unsigned char c){return c<32||c==127;})&&
        static_cast<unsigned char>(name.front())>32&&static_cast<unsigned char>(name.back())>32;
}
bool CounterModel::ReadCatalog(const std::filesystem::path& file,CounterModel& output,std::string& error) {
    try {
        std::ifstream in(file);if(!in)throw std::runtime_error("Counter catalog export missing");
        if(Header(in)!=std::vector<std::string>{"Id","Type","Name"})throw std::runtime_error("Unsupported counter catalog header");
        CounterModel result;std::set<uint32_t> ids;std::map<std::string,size_t> names;std::string line;
        while(std::getline(in,line)) {
            if(line.empty()||line=="\r")continue;
            const auto row=Row(line);if(row.size()!=3)throw std::runtime_error("Invalid counter catalog row");
            if(row[0].empty()||row[0].find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("Invalid native counter identity");
            const auto id=std::stoull(row[0]);if(id>UINT32_MAX||!ids.insert(static_cast<uint32_t>(id)).second)throw std::runtime_error("Invalid or duplicate counter identity");
            CounterSeries counter;counter.id=static_cast<uint32_t>(id);counter.name=row[2];
            auto type=row[1];
            if(type!="Int64"&&type!="Double"&&type!="Int64|ResetEveryFrame"&&type!="Double|ResetEveryFrame")throw std::runtime_error("Unsupported native counter type");
            counter.reset_per_frame=type.find("|ResetEveryFrame")!=std::string::npos;
            if(counter.reset_per_frame)type.erase(type.find('|'));
            if(type!="Int64"&&type!="Double")throw std::runtime_error("Unsupported native counter type");counter.floating=type=="Double";
            counter.meta=counter.name.rfind("CyInsights/Meta/",0)==0;
            counter.availability=counter.meta&&counter.name.size()>=10&&counter.name.compare(counter.name.size()-10,10,"/available")==0;
            auto unit=counter.name.rfind(" ["),end=counter.name.rfind("]/");
            if(counter.meta&&unit!=std::string::npos&&end!=std::string::npos&&end>unit+2)counter.unit=counter.name.substr(unit+2,end-unit-2);
            if(!CanExport(counter.name))counter.reason="Counter name cannot be queried exactly by installed analyzer";
            ++names[counter.name];result.series.push_back(std::move(counter));
        }
        if(!in.eof())throw std::runtime_error("Counter catalog read failed");
        for(auto& counter:result.series)if(names[counter.name]>1)counter.reason="Duplicate counter name: values cannot be assigned uniquely by installed analyzer";
        output=std::move(result);return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void CounterModel::LoadValues(const std::filesystem::path& directory,const std::atomic<bool>* cancel) {
    size_t total=0;uint64_t bytes=0;
    for(auto& counter:series)if(counter.reason.empty()) {
        if(cancel&&cancel->load())throw std::runtime_error("Counter import cancelled");
        ReadValues(directory/(std::to_string(counter.id)+".csv"),counter,total,bytes,cancel);
    }
    Index(cancel);
}
void CounterModel::Index(const std::atomic<bool>* cancel) {
    start=INFINITY;end=-INFINITY;std::map<std::string,int> names;
    std::map<std::string,unsigned> types;
    for(const auto& counter:series)if(counter.meta&&!counter.availability)
        types[counter.name.substr(0,counter.name.rfind(" ["))]|=counter.floating?2u:1u;
    for(size_t i=0;i<series.size();++i)if(series[i].availability&&series[i].reason.empty())names[series[i].name]=static_cast<int>(i);
    for(auto& counter:series) {
        counter.companion=-1;
        if(counter.meta&&!counter.availability) {
            auto unit=counter.name.rfind(" [");
            const auto base=counter.name.substr(0,unit);
            auto found=names.find(base+(counter.floating?"/float/available":"/uint/available"));
            // Legacy captures only have overall metric availability. If both
            // types exist, that flag cannot establish validity for either type.
            if(found==names.end()&&types[base]!=3)found=names.find(base+"/available");
            if(found!=names.end())counter.companion=found->second;
            else if(counter.reason.empty())counter.reason=types[base]==3?
                "Meta per-type availability unavailable; global flag is ambiguous; raw values retained":
                "Meta availability companion unavailable; raw values retained";
        }
        if(!counter.samples.empty()){start=std::min(start,counter.samples.front().time);end=std::max(end,counter.samples.back().time);}
    }
    if(!std::isfinite(start)){start=0;end=0;}
    ranges_.clear();std::vector<RangeIndex> prepared(series.size());
    for(size_t counter=0;counter<series.size();++counter){
        const auto& c=series[counter];if(c.samples.size()<512)continue;
        auto& index=prepared[counter];index.samples=c.samples.size();index.valid.resize(index.samples);
        size_t blocks=(index.samples+range_block_samples-1)/range_block_samples;index.base=1;while(index.base<blocks)index.base*=2;index.tree.resize(index.base*2);
        for(size_t i=0;i<index.samples;++i){
            if((i&1023)==0&&cancel&&cancel->load())throw std::runtime_error("Counter import cancelled");
            index.valid[i]=c.samples[i].valid&&Available(counter,c.samples[i].time);
            CounterStats sample;if(index.valid[i]){sample.count=1;sample.first=sample.last=sample.minimum_sample=sample.maximum_sample=i;sample.mean=sample.minimum=sample.maximum=c.samples[i].Value(c.floating);}else sample.unavailable=1;
            auto& block=index.tree[index.base+i/range_block_samples];block=Combine(counter,block,sample);
        }
        for(size_t i=index.base-1;i>0;--i)index.tree[i]=Combine(counter,index.tree[i*2],index.tree[i*2+1]);
    }ranges_=std::move(prepared);
}
bool CounterModel::Indexed(size_t counter) const {return counter<ranges_.size()&&ranges_[counter].base&&ranges_[counter].samples==series[counter].samples.size();}
bool CounterModel::SampleAvailable(size_t counter,size_t sample) const {return Indexed(counter)?ranges_[counter].valid[sample]!=0:series[counter].samples[sample].valid&&Available(counter,series[counter].samples[sample].time);}
CounterStats CounterModel::Combine(size_t counter,const CounterStats& a,const CounterStats& b) const {
    if(!a.count){auto result=b;result.unavailable+=a.unavailable;return result;}if(!b.count){auto result=a;result.unavailable+=b.unavailable;return result;}
    auto result=a;result.count+=b.count;result.unavailable+=b.unavailable;result.last=b.last;
    const auto& c=series[counter];auto less=[&](size_t x,size_t y){return c.floating?c.samples[x].real<c.samples[y].real:c.samples[x].integer<c.samples[y].integer;};
    if(less(b.minimum_sample,a.minimum_sample))result.minimum_sample=b.minimum_sample;
    if(less(a.maximum_sample,b.maximum_sample))result.maximum_sample=b.maximum_sample;
    result.minimum=c.samples[result.minimum_sample].Value(c.floating);result.maximum=c.samples[result.maximum_sample].Value(c.floating);
    const double weight=double(b.count)/double(result.count);result.mean=a.mean*(1-weight)+b.mean*weight;return result;
}
CounterStats CounterModel::Range(size_t counter,size_t begin,size_t end) const {
    CounterStats result;if(begin>=end)return result;const auto& index=ranges_[counter];const auto& c=series[counter];
    auto sample=[&](size_t i){CounterStats s;if(index.valid[i]){s.count=1;s.first=s.last=s.minimum_sample=s.maximum_sample=i;s.mean=s.minimum=s.maximum=c.samples[i].Value(c.floating);}else s.unavailable=1;result=Combine(counter,result,s);};
    auto lead=std::min(end,((begin+range_block_samples-1)/range_block_samples)*range_block_samples);for(size_t i=begin;i<lead;++i)sample(i);
    auto tail=std::max(lead,(end/range_block_samples)*range_block_samples);CounterStats left,right;
    for(size_t l=index.base+lead/range_block_samples,r=index.base+tail/range_block_samples;l<r;l/=2,r/=2){if(l&1)left=Combine(counter,left,index.tree[l++]);if(r&1)right=Combine(counter,index.tree[--r],right);}
    result=Combine(counter,result,Combine(counter,left,right));for(size_t i=tail;i<end;++i)sample(i);return result;
}
bool CounterModel::Available(size_t counter,double time) const {
    if(counter>=series.size())return false;const auto& c=series[counter];
    if(!c.meta||c.availability)return true;
    if(c.companion<0)return false;
    const auto& flag=series[static_cast<size_t>(c.companion)];
    auto after=std::upper_bound(flag.samples.begin(),flag.samples.end(),time,[](double t,const CounterSample& s){return t<s.time;});
    return after!=flag.samples.begin()&&(--after)->valid&&after->Value(flag.floating)==1;
}
const CounterSample* CounterModel::At(size_t counter,double time) const {
    if(counter>=series.size()||!Available(counter,time))return nullptr;const auto& c=series[counter];
    auto after=std::upper_bound(c.samples.begin(),c.samples.end(),time,[](double t,const CounterSample& s){return t<s.time;});
    if(after==c.samples.begin())return nullptr;--after;
    if(!after->valid||!Available(counter,after->time))return nullptr;
    // Recovery does not refresh stale values; wait for a new measured value.
    if(c.meta&&!c.availability&&c.companion>=0) {
        const auto& flag=series[static_cast<size_t>(c.companion)];
        for(size_t i=flag.Lower(after->time);i<flag.samples.size()&&flag.samples[i].time<=time;++i)
            if(!flag.samples[i].valid||flag.samples[i].Value(flag.floating)!=1)return nullptr;
    }
    return &*after;
}
CounterStats CounterModel::Stats(size_t counter,double a,double b) const {
    CounterStats result;if(counter>=series.size()||!std::isfinite(a)||!std::isfinite(b)||a>b)return result;
    if(Indexed(counter)){const auto& c=series[counter];auto end=std::upper_bound(c.samples.begin(),c.samples.end(),b,[](double t,const CounterSample& s){return t<s.time;})-c.samples.begin();return Range(counter,c.Lower(a),static_cast<size_t>(end));}
    const auto& c=series[counter];double mean=0;
    for(size_t i=c.Lower(a);i<c.samples.size()&&c.samples[i].time<=b;++i) {
        const auto& s=c.samples[i];if(!s.valid||!Available(counter,s.time)){++result.unavailable;continue;}
        const double value=s.Value(c.floating);
        if(!result.count){result.minimum=result.maximum=value;result.first=result.minimum_sample=result.maximum_sample=i;}
        if(c.floating?value<c.samples[result.minimum_sample].real:s.integer<c.samples[result.minimum_sample].integer)result.minimum_sample=i;
        if(c.floating?value>c.samples[result.maximum_sample].real:s.integer>c.samples[result.maximum_sample].integer)result.maximum_sample=i;
        result.minimum=std::min(result.minimum,value);result.maximum=std::max(result.maximum,value);
        ++result.count;const double n=static_cast<double>(result.count);
        mean=mean*(1-1/n)+value/n;result.last=i;
    }
    result.mean=mean;return result;
}
std::vector<CounterBucket> CounterModel::Plot(size_t counter,double a,double b,int width) const {
    std::vector<CounterBucket> result;if(counter>=series.size()||width<=0||!std::isfinite(a)||!std::isfinite(b)||a>=b)return result;
    const auto& c=series[counter];
    auto first=c.Lower(a),end=static_cast<size_t>(std::upper_bound(c.samples.begin(),c.samples.end(),b,[](double t,const CounterSample& s){return t<s.time;})-c.samples.begin());
    if(Indexed(counter)&&end-first>static_cast<size_t>(width)*range_block_samples*2){
        auto pixel=[&](double t){return std::clamp(static_cast<int>((t-a)/(b-a)*width),0,width-1);};
        while(first<end){int x=pixel(c.samples[first].time);auto next=static_cast<size_t>(std::upper_bound(c.samples.begin()+first,c.samples.begin()+end,x,[&](int column,const CounterSample& sample){return column<pixel(sample.time);})-c.samples.begin());
            auto summary=Range(counter,first,next);if(summary.count)result.push_back({summary.first,summary.last,summary.minimum_sample,summary.maximum_sample,x});first=next;
        }return result;
    }
    for(size_t i=c.Lower(a);i<c.samples.size()&&c.samples[i].time<=b;++i) {
        const auto& s=c.samples[i];if(!SampleAvailable(counter,i))continue;
        int x=std::clamp(static_cast<int>((s.time-a)/(b-a)*width),0,width-1);
        if(result.empty()||result.back().pixel!=x)result.push_back({i,i,i,i,x});
        else {auto& bucket=result.back();bucket.last=i;
            if(c.floating?s.real<c.samples[bucket.minimum].real:s.integer<c.samples[bucket.minimum].integer)bucket.minimum=i;
            if(c.floating?s.real>c.samples[bucket.maximum].real:s.integer>c.samples[bucket.maximum].integer)bucket.maximum=i;}
    }
    return result;
}
}
