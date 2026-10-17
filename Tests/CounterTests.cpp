// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "CounterModel.hpp"
#include "CounterTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>
using namespace cyinsights;
namespace fs=std::filesystem;
int main(int argc,char** argv) {
    if(argc==5&&std::string(argv[1])=="--analyze") {
        fs::path cache=fs::u8path(argv[4]);fs::create_directories(cache);
        NativeTrace native;std::string error;
        if(!native.Start(fs::u8path(argv[2]),fs::u8path(argv[3]),cache,error)){std::cerr<<error;return 1;}
        auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(2);
        while(native.Busy()&&std::chrono::steady_clock::now()<deadline){native.Poll(error);std::this_thread::sleep_for(std::chrono::milliseconds(20));}
        if(native.Busy()||!native.counters.error.empty()){std::cerr<<error<<native.counters.error;return 1;}
        std::cout<<"Timing events: "<<native.events.size()<<" / counter catalog: "<<native.counters.series.size()<<"\n";
        CounterTrace exporter;CounterModel values;
        if(!exporter.Start(fs::u8path(argv[2]),fs::u8path(argv[3]),cache,std::move(native.counters),error)){std::cerr<<error;return 1;}
        while(exporter.Busy()&&std::chrono::steady_clock::now()<deadline){exporter.Poll(values,error);std::this_thread::sleep_for(std::chrono::milliseconds(20));}
        if(exporter.Busy()||!values.error.empty()){std::cerr<<error;return 1;}
        size_t count=0,available=0;
        for(const auto& c:values.series){count+=c.samples.size();available+=c.reason.empty();}
        std::cout<<"Imported counters: "<<values.series.size()<<" / available: "<<available<<" / samples: "<<count<<"\n";
        return count?0:1;
    }
    int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<name<<"\n";}};
    auto dir=fs::temp_directory_path()/("CyCounterTests_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(dir);auto catalog=dir/"catalog.csv";std::string error;CounterModel model;
    std::ofstream(catalog)<<"\xef\xbb\xbf"<<"Id,Type,Name\n"
        "4294967295,Int64,Native integer\n"
        "2,Double,CyInsights/Meta/perfmetrics_meta/app/gpu [milliseconds]/float\n"
        "3,Int64,CyInsights/Meta/perfmetrics_meta/app/gpu/available\n"
        "4,Double|ResetEveryFrame,Native reset\n"
        "5,Double,Duplicate\n6,Int64,Duplicate\n"
        "7,Double,\"Comma, name\"\n"
        "8,Double,CyInsights/Meta/perfmetrics_meta/missing [watts]/float\n";
    check(CounterModel::ReadCatalog(catalog,model,error)&&model.series.size()==8,"catalog and BOM parse");
    check(model.series[0].id==UINT32_MAX&&!model.series[0].floating,"native uint32 identity and integer type retained");
    check(model.series[1].meta&&model.series[1].unit=="milliseconds","Meta unit retained without guessed conversion");
    check(model.series[2].availability&&model.series[3].reset_per_frame,"availability and reset metadata retained");
    check(!model.series[4].reason.empty()&&!model.series[5].reason.empty(),"duplicate names do not share guessed values");
    check(model.series[6].name=="Comma, name"&&!model.series[6].reason.empty(),"quoted comma name retained but exact query unavailable");
    std::ofstream(dir/"4294967295.csv")<<"Time,Value\n0,9007199254740992\n1,9007199254740993\n2,-9223372036854775808\n";
    std::ofstream(dir/"2.csv")<<"Time,Value\n0,0\n1.1,5\n1.5,nan\n2.5,20\n3.1,0\n";
    std::ofstream(dir/"3.csv")<<"Time,Value\n0,0\n1,1\n2,0\n3,1\n";
    std::ofstream(dir/"4.csv")<<"Time,Value\n0,1\n1,2\n";
    std::ofstream(dir/"8.csv")<<"Time,Value\n1,50\n";
    model.LoadValues(dir);
    check(model.series[0].samples[1].integer==9007199254740993ll,"integer values above 2^53 remain exact");
    check(model.series[0].Text(model.series[0].samples[1])=="9007199254740993","integer display remains exact");
    check(model.series[0].samples[2].integer==INT64_MIN,"minimum signed value retained");
    auto integer_stats=model.Stats(0,0,3);
    check(integer_stats.minimum_sample==2&&integer_stats.maximum_sample==1,"integer extrema compare integers, not rounded doubles");
    auto extrema=model.Plot(0,0,3,1);
    check(extrema.size()==1&&extrema[0].maximum==1&&extrema[0].minimum==2,"pixel aggregation preserves exact integer extrema");
    check(!model.At(1,.5),"initial Meta default zero is unavailable");
    check(model.At(1,1.2)&&model.At(1,1.2)->real==5,"valid Meta observation loaded");
    check(!model.At(1,1.7),"nonfinite Meta reading is unavailable");
    check(!model.At(1,2.7),"availability gap cannot display stale value");
    check(!model.At(1,3.05),"availability recovery requires a fresh measured value");
    check(model.At(1,3.2)&&model.At(1,3.2)->real==0,"measured zero remains valid");
    auto stats=model.Stats(1,0,4);
    check(stats.count==2&&stats.unavailable==3&&stats.mean==2.5,"statistics exclude invalid observations and synthetic default");
    check(model.Stats(1,1.1,1.1).count==1,"inclusive interval boundaries");
    check(model.Stats(1,4,1).count==0,"inverted interval has no statistics");
    check(model.Plot(1,0,4,1).size()==1&&model.Plot(1,0,4,1)[0].minimum==4,"dense plot retains peaks and valid zero");
    check(model.Plot(1,0,4,0).empty(),"invalid plot width rejected");
    check(!model.series[7].reason.empty()&&!model.At(7,2)&&model.series[7].samples.size()==1,"missing availability is explicit; raw values retained");
    check(!CounterModel::CanExport("a*\nquit")&&!CounterModel::CanExport("a?b")&&!CounterModel::CanExport("a\\b")&&
          !CounterModel::CanExport("a\tquit")&&!CounterModel::CanExport(std::string("a\0quit",6)),"counter names cannot inject export commands or ambiguous wildcards");
    check(CounterModel::CanExport("CyInsights/Meta/path [ms]/float"),"exact Meta query supported");
    {
        std::ofstream(catalog)<<"Id,Type,Name\n"
            "1,Double,CyInsights/Meta/path [ms]/float\n2,Int64,CyInsights/Meta/path [ms]/uint\n"
            "3,Int64,CyInsights/Meta/path/available\n4,Int64,CyInsights/Meta/path/float/available\n"
            "5,Int64,CyInsights/Meta/path/uint/available\n";
        CounterModel typed;CounterModel::ReadCatalog(catalog,typed,error);
        std::ofstream(dir/"1.csv")<<"Time,Value\n1.1,5\n";
        std::ofstream(dir/"2.csv")<<"Time,Value\n1.1,10\n2.1,20\n";
        std::ofstream(dir/"3.csv")<<"Time,Value\n1,1\n2,1\n";
        std::ofstream(dir/"4.csv")<<"Time,Value\n1,1\n2,0\n";
        std::ofstream(dir/"5.csv")<<"Time,Value\n1,1\n2,1\n";
        typed.LoadValues(dir);
        check(typed.At(0,1.2)&&typed.At(0,1.2)->real==5,"type-specific float availability loaded");
        check(!typed.At(0,2.2),"invalid float cannot inherit valid integer availability");
        check(typed.At(1,2.2)&&typed.At(1,2.2)->integer==20,"valid integer remains available independently");
        typed.series.resize(3);typed.Index();
        check(!typed.At(0,1.2)&&!typed.At(1,1.2),"legacy mixed-type metric availability remains ambiguous");
    }
    for(const char* row:{"1,Double,First\n1,Double,Second\n","-1,Double,Name\n","4294967296,Double,Name\n","1,Float,Name\n","1,Double|Unexpected,Name\n","1,Double,\"incomplete\n"}) {
        std::ofstream(catalog)<<"Id,Type,Name\n"<<row;CounterModel bad;
        check(!CounterModel::ReadCatalog(catalog,bad,error),"malformed catalog rejected");
    }
    for(const char* row:{"2,1\n1,2\n","-1,2\n","nan,2\n","1,1bad\n","1,9223372036854775808\n"}) {
        std::ofstream(catalog)<<"Id,Type,Name\n1,Int64,Native\n";
        CounterModel bad;CounterModel::ReadCatalog(catalog,bad,error);
        std::ofstream(dir/"1.csv")<<"Time,Value\n"<<row;bad.LoadValues(dir);
        check(!bad.series[0].reason.empty()&&bad.series[0].samples.empty(),"malformed samples are unavailable, never partial published");
    }
    {
        CounterModel huge;CounterSeries c;c.floating=true;c.samples={{0,1e308},{1,-1e308}};huge.series.push_back(c);
        check(std::isfinite(huge.Stats(0,0,1).mean),"derived mean does not overflow opposite finite extremes");
    }
    {
        CounterTrace worker;CounterModel empty;
        check(worker.Start({}, {},dir/"empty",empty,error)&&worker.Busy(),"background empty catalog export starts");
        check(!worker.Start({}, {},dir/"empty",empty,error),"concurrent counter analysis refused");
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);std::string message;CounterModel result;
        while(worker.Busy()&&std::chrono::steady_clock::now()<deadline){worker.Poll(result,message);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(!worker.Busy()&&result.error.empty(),"background completion clears busy state");
        CounterModel missing;CounterSeries c;c.name="Counter";missing.series.push_back(c);
        worker.Start(dir/"missing.exe",dir/"trace",dir/"missing",missing,error);
        while(worker.Busy()&&std::chrono::steady_clock::now()<deadline){worker.Poll(result,message);std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        check(!worker.Busy()&&!result.error.empty(),"analyzer launch failure returns explicit error");
    }
    {
        CounterModel indexed,reference;CounterSeries c;c.name="Dense integer";
        for(int i=0;i<20000;++i)c.samples.push_back({(i/3)*.001,0,9007199254740992ll+i%97,i%17!=0});
        indexed.series.push_back(c);reference.series.push_back(c);indexed.Index();
        bool stats_equal=true,plots_equal=true;
        for(auto interval:{std::pair<double,double>{0,7},{.255,.512},{1.234,4.321},{3,3},{10,11}}){
            auto a=indexed.Stats(0,interval.first,interval.second),b=reference.Stats(0,interval.first,interval.second);
            stats_equal&=a.count==b.count&&a.unavailable==b.unavailable&&a.first==b.first&&a.last==b.last&&a.minimum_sample==b.minimum_sample&&a.maximum_sample==b.maximum_sample&&std::abs(a.mean-b.mean)<=std::max(1.,std::abs(b.mean))*1e-12;
            for(int width:{1,17,127}){auto x=indexed.Plot(0,interval.first,interval.second,width),y=reference.Plot(0,interval.first,interval.second,width);plots_equal&=x.size()==y.size();if(x.size()==y.size())for(size_t i=0;i<x.size();++i)plots_equal&=x[i].pixel==y[i].pixel&&x[i].first==y[i].first&&x[i].last==y[i].last&&x[i].minimum==y[i].minimum&&x[i].maximum==y[i].maximum;}
        }
        check(stats_equal,"Indexed range statistics match a full scan across block edges, equal timestamps, exact integer extrema and invalid samples");
        check(plots_equal,"Dense indexed counter plots match every first/last/min/max sample and pixel boundary of a full scan");
        for(auto& s:indexed.series[0].samples)s.valid=false;indexed.Index();auto invalid=indexed.Stats(0,0,7);check(invalid.count==0&&invalid.unavailable==20000&&indexed.Plot(0,0,7,17).empty(),"Reindexing and fully unavailable dense intervals produce no fabricated observations");
        std::atomic<bool> cancelled{true};bool stopped=false;try{indexed.Index(&cancelled);}catch(...){stopped=true;}check(stopped,"Counter range index preparation is cancellable");
    }
    {
        CounterModel indexed,reference;CounterSeries values,flags;values.name="CyInsights/Meta/dense [ms]/float";values.meta=values.floating=true;
        flags.name="CyInsights/Meta/dense/float/available";flags.meta=flags.availability=true;
        for(int i=0;i<10000;++i)values.samples.push_back({.0005+i*.001,double(i%31)-12.5,0,i%13!=0});
        for(int i=0;i<11;++i)flags.samples.push_back({double(i),0,i%2,true});indexed.series={values,flags};indexed.Index();reference.series=indexed.series;
        auto a=indexed.Stats(0,.5,9.5),b=reference.Stats(0,.5,9.5);auto x=indexed.Plot(0,.5,9.5,11),y=reference.Plot(0,.5,9.5,11);
        bool equal=a.count==b.count&&a.unavailable==b.unavailable&&a.minimum_sample==b.minimum_sample&&a.maximum_sample==b.maximum_sample&&std::abs(a.mean-b.mean)<1e-12&&x.size()==y.size();
        if(x.size()==y.size())for(size_t i=0;i<x.size();++i)equal&=x[i].pixel==y[i].pixel&&x[i].first==y[i].first&&x[i].last==y[i].last&&x[i].minimum==y[i].minimum&&x[i].maximum==y[i].maximum;
        check(equal,"Dense Meta counter index preserves per-type availability gaps, invalid float readings and exact plot extrema");
    }
    {
        const auto file=dir/"frame-stats.cystats";
        auto fixture=[&](bool bad_id=false,bool truncated=false){
            std::ofstream out(file,std::ios::binary);out.write(cyfs::magic,8);
            auto value=[&](const auto& v){out.write(reinterpret_cast<const char*>(&v),sizeof(v));};
            auto text=[&](const char* s){auto n=static_cast<uint32_t>(std::strlen(s));value(n);out.write(s,n);};
            auto spec=[&](uint32_t id,bool floating,const char* group,const char* name,const char* unit){value(uint8_t(cyfs::Spec));value(id);value(uint8_t(floating));text(group);text(name);text(unit);};
            spec(5,true,"Game","Tick inclusive","ms");spec(7,false,"UObjects","Allocated object slots","objects");
            auto sample=[&](cyfs::Sample s){value(uint8_t(cyfs::Value));value(s);};
            sample({5,cyfs::FrameBounds|cyfs::Delayed|cyfs::Aggregate|cyfs::Valid,1.2,1.3,3.4,43,21,0,0});
            sample({bad_id?99u:5u,cyfs::FrameBounds|cyfs::Delayed|cyfs::Aggregate|cyfs::Valid,1,1.1,3,42,20,0,7.5});
            sample({5,cyfs::FrameBounds|cyfs::Delayed,1.4,1.5,3.6,44,22,0,99});
            sample({7,cyfs::FrameBounds|cyfs::Valid,1,1.1,1.1,42,20,9007199254740993ll,0});
            if(!truncated){value(uint8_t(cyfs::End));value(uint64_t(4));}
        };
        fixture();CounterModel frames;CounterSeries mirror;mirror.id=UINT32_MAX;mirror.name="CyInsights/Stats/Game/Tick inclusive [ms]";frames.series.push_back(mirror);
        check(frames.ReadFrameStats(file,true,error)&&frames.series.size()==2&&frames.series[0].frame_stats&&frames.series[0].samples.empty(),"frame-stat catalogue replaces observation mirrors without loading values");
        check(frames.ReadFrameStats(file,false,error)&&frames.series.size()==2&&frames.series[0].samples.size()==3,"streamed frame stats load without duplicate catalogue entries");
        auto measured=frames.InFrame(0,1,1.1);
        check(measured&&measured->real==7.5&&frames.series[0].samples[0].time<1.1&&frames.series[0].frame_samples[0].observed==3,"delayed stats are aligned to source frame, not receive frame");
        check(frames.series[0].frame_samples[0].game_frame==42&&frames.series[0].frame_samples[0].stats_frame==20,"source game and stats IDs stay separate");
        check(!frames.InFrame(0,1.1,1.2)&&!frames.InFrame(0,3,3.1),"missing source frame and receipt frame never reuse a previous sample");
        check(frames.InFrame(0,1.2,1.3)&&frames.InFrame(0,1.2,1.3)->real==0,"valid zero recorded for a frame remains measured");
        check(!frames.InFrame(0,1.4,1.5),"invalid latest frame sample stays unavailable");
        check(frames.InFrame(1,1,1.1)&&frames.series[1].Text(*frames.InFrame(1,1,1.1))=="9007199254740993","frame integer values preserve precision beyond double");
        fixture(true);CounterModel invalid;check(!invalid.ReadFrameStats(file,true,error)&&invalid.series.empty(),"catalogue rejects unknown frame-stat identities before publishing");
        fixture(false,true);check(!invalid.ReadFrameStats(file,false,error),"truncated frame-stat cache rejected");
        fixture();std::atomic<bool> cancel{true};check(!invalid.ReadFrameStats(file,false,error,&cancel),"frame-stat import is cancellable");
        check(frames.InFrame(0,1.2,1.3)!=nullptr,"failed import leaves an unrelated loaded frame model intact");
    }
    std::error_code ec;fs::remove_all(dir,ec);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
