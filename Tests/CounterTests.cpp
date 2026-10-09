// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "CounterModel.hpp"
#include "CounterTrace.hpp"
#include "NativeTrace.hpp"
#include <chrono>
#include <cmath>
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
    std::error_code ec;fs::remove_all(dir,ec);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
