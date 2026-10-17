// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "VulkanPassDetails.hpp"
#include "../Plugins/CyInsights/Source/CyInsights/Private/QuestPassContents.h"
#include <fstream>
#include <iostream>
using namespace cyinsights;
namespace fs=std::filesystem;
int main(){int checks=0,failures=0;auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<'\n';}};
    cyinsights_quest::PassWork primary,secondary;primary.Draw(3,2,false);secondary.Draw(12,4,true);secondary.Indirect(5);primary.Merge(secondary);primary.Merge(secondary);
    check(primary.direct==1&&primary.indexed==2&&primary.indirect==2&&primary.indirect_commands==10,"Repeated secondary execution counts each invocation");
    check(primary.vertices==6&&primary.indices==96&&primary.instances==10,"Instanced direct arguments are preserved without guessing shader invocations");
    check(primary.flags==cyinsights_quest::IndirectExecutionUnknown,"Indirect argument values remain unavailable without reading GPU buffers");
    auto dir=fs::temp_directory_path()/("cy-pass-details-"+std::to_string(GetCurrentProcessId()));fs::create_directory(dir);
    std::vector<cyvpd::Record> records;
    for(uint32_t i=0;i<3;++i){cyvpd::Record r{};r.begin=10;r.end=10.014;r.pass=1;r.submission=5;r.ordinal=1;r.width=2016;r.height=1760;r.subpass=i;r.subpasses=3;r.direct=3-i;r.indices=(1ull<<54)+i;records.push_back(r);}
    auto write=[&](const fs::path& file,const auto& rows,uint64_t footerCount){std::ofstream out(file,std::ios::binary);out.write(cyvpd::magic,8);for(const auto& r:rows)out.write(reinterpret_cast<const char*>(&r),sizeof(r));out.write(cyvpd::footer,8);out.write(reinterpret_cast<const char*>(&footerCount),8);};
    std::string error;write(dir/"a.cyvp",records,3);auto details=VulkanPassDetails::Open(dir/"a.cyvp",error);check(details&&details->count==3,"Actual detail format opens through read-only mapping");
    if(details){uint64_t matched=0;auto rows=details->Query(9,11,2,matched);check(rows.size()==2&&matched==3&&rows[1].indices==(1ull<<54)+1,"Response limit does not discard source records or large integer counters");
        auto page=details->Query(9,11,1,matched,nullptr,1,2);check(page.size()==1&&page[0].subpass==2&&matched==3,"SDK offset accesses records beyond the response limit");
        check(details->Query(11,12,10,matched).empty()&&matched==0,"Timeline range excludes unrelated passes");
        TimingEvent event;event.thread="GPU / Vulkan render passes (approximate alignment)";event.name="Vulkan pass 1 [2016x1760, 3 subpasses]";event.start=10;event.end=10.014;bool ambiguous=false;
        check(details->Find(event,ambiguous).size()==3&&!ambiguous,"Selected parent resolves its actual subpass records");event.represented=2;check(details->Find(event,ambiguous).empty(),"Aggregated glyphs never masquerade as a unique pass");
        std::atomic<bool> cancel{true};bool stopped=false;try{details->Query(9,11,10,matched,&cancel);}catch(...){stopped=true;}check(stopped,"Queries respect SDK cancellation");
    }
    write(dir/"bad.cyvp",records,2);check(!VulkanPassDetails::Open(dir/"bad.cyvp",error),"Incomplete footer rejected");
    records[0].subpass=9;write(dir/"bad2.cyvp",records,3);check(!VulkanPassDetails::Open(dir/"bad2.cyvp",error),"Malformed subpass identity rejected");records[0].subpass=0;
    auto duplicate=records[0];duplicate.pass=2;records.push_back(duplicate);write(dir/"ambiguous.cyvp",records,4);auto ambiguousStore=VulkanPassDetails::Open(dir/"ambiguous.cyvp",error);
    if(ambiguousStore){TimingEvent event;event.thread="GPU / Vulkan render passes (approximate alignment)";event.name="Vulkan pass 1 [2016x1760, 3 subpasses]";event.start=10;event.end=10.014;bool ambiguous=false;check(ambiguousStore->Find(event,ambiguous).empty()&&ambiguous,"Coincident pass intervals with different identities remain ambiguous");}
    details.reset();ambiguousStore.reset();fs::remove_all(dir);std::cout<<checks<<" pass-content checks, "<<failures<<" failures\n";return failures?1:0;
}
