// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "VulkanPassDetails.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace cyinsights {
namespace {void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}void Cancel(const std::atomic<bool>* c){if(c&&c->load())throw std::runtime_error("Cancelled");}}
VulkanPassDetails::~VulkanPassDetails(){if(bytes_)UnmapViewOfFile(bytes_);if(map_)CloseHandle(map_);if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}
cyvpd::Record VulkanPassDetails::Record(uint64_t index)const{cyvpd::Record r{};std::memcpy(&r,bytes_+8+index*sizeof(r),sizeof(r));return r;}
std::shared_ptr<VulkanPassDetails> VulkanPassDetails::Open(const std::filesystem::path& path,std::string& error,const std::atomic<bool>* cancel){
    error.clear();std::error_code ec;if(!std::filesystem::exists(path,ec))return {};
    try{auto out=std::make_shared<VulkanPassDetails>();out->file_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);Check(out->file_!=INVALID_HANDLE_VALUE,"Vulkan pass contents unavailable");
        LARGE_INTEGER size{};Check(GetFileSizeEx(out->file_,&size)&&size.QuadPart>=24,"Truncated Vulkan pass contents");const auto length=uint64_t(size.QuadPart);Check((length-24)%sizeof(cyvpd::Record)==0,"Invalid Vulkan content record size");out->count=(length-24)/sizeof(cyvpd::Record);
        out->map_=CreateFileMappingW(out->file_,nullptr,PAGE_READONLY,0,0,nullptr);Check(out->map_!=nullptr,"Cannot map Vulkan pass contents");out->bytes_=static_cast<const unsigned char*>(MapViewOfFile(out->map_,FILE_MAP_READ,0,0,0));Check(out->bytes_!=nullptr,"Cannot map Vulkan pass contents");
        Check(!std::memcmp(out->bytes_,cyvpd::magic,8)&&!std::memcmp(out->bytes_+length-16,cyvpd::footer,8),"Unknown or incomplete Vulkan pass content format");uint64_t count=0;std::memcpy(&count,out->bytes_+length-8,8);Check(count==out->count,"Incomplete Vulkan pass content count");
        for(uint64_t i=0;i<count;++i){if(i%4096==0){Cancel(cancel);out->blocks_.push_back({i,0,INFINITY,-INFINITY});}auto r=out->Record(i);Check(std::isfinite(r.begin)&&std::isfinite(r.end)&&r.begin>=0&&r.end>=r.begin&&r.pass&&r.submission&&r.ordinal&&r.width&&r.height&&r.subpasses&&r.subpass<r.subpasses&&!(r.flags&~3u),"Invalid Vulkan pass content record");auto& b=out->blocks_.back();++b.count;b.begin=std::min(b.begin,r.begin);b.end=std::max(b.end,r.end);}
        return out;
    }catch(const std::exception& e){error=e.what();return {};}
}
std::vector<cyvpd::Record> VulkanPassDetails::Query(double begin,double end,size_t limit,uint64_t& matched,const std::atomic<bool>* cancel,uint64_t pass,uint64_t offset)const{
    matched=0;std::vector<cyvpd::Record> rows;for(const auto& block:blocks_){Cancel(cancel);if(block.end<begin||block.begin>end)continue;for(uint64_t i=block.first;i<block.first+block.count;++i){if((i&255)==0)Cancel(cancel);auto r=Record(i);if((pass&&r.pass!=pass)||r.end<=begin||r.begin>=end)continue;++matched;if(matched>offset&&rows.size()<limit)rows.push_back(r);}}
    std::stable_sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){if(a.begin!=b.begin)return a.begin<b.begin;if(a.pass!=b.pass)return a.pass<b.pass;return a.subpass<b.subpass;});return rows;
}
std::vector<cyvpd::Record> VulkanPassDetails::Find(const TimingEvent& event,bool& ambiguous)const{
    ambiguous=false;std::vector<cyvpd::Record> rows;if(event.display_aggregate||event.represented!=1||event.thread!="GPU / Vulkan render passes (approximate alignment)")return rows;
    uint64_t pass=0;for(const auto& block:blocks_){if(block.end<event.start-1e-9||block.begin>event.end+1e-9)continue;for(uint64_t i=block.first;i<block.first+block.count;++i){auto r=Record(i);if(std::abs(r.begin-event.start)>1e-9||std::abs(r.end-event.end)>1e-9)continue;
        auto name="Vulkan pass "+std::to_string(r.ordinal)+" ["+std::to_string(r.width)+"x"+std::to_string(r.height)+", "+std::to_string(r.subpasses)+" subpasses]";if(name!=event.name)continue;if(pass&&r.pass!=pass){ambiguous=true;return {};}pass=r.pass;rows.push_back(r);}}
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.subpass<b.subpass;});return rows;
}
}
