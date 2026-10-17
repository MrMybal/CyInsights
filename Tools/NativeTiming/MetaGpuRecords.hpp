// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "VulkanPassRecords.hpp"
#include <string>
#include <cmath>
namespace cymeta {
inline bool MonoAnchorFromCycles(uint64_t ticks,double frequency,uint64_t& nanoseconds){
    if(!ticks||!std::isfinite(frequency)||frequency<=0)return false;
    if(frequency==1000000){if(ticks>UINT64_MAX/1000)return false;nanoseconds=ticks*1000;return true;}
    const long double value=static_cast<long double>(ticks)*1000000000.L/frequency;
    if(value<0||value>=static_cast<long double>(UINT64_MAX))return false;
    nanoseconds=uint64_t(value);return true;
}
struct Record {double begin,end;uint32_t timer;};
inline void ExportTrack(FILE* out,FILE* spool,const std::string& name,uint64_t lane,uint64_t count,uint32_t timerBase){
    if(fflush(spool)||_fseeki64(spool,0,SEEK_SET))throw std::runtime_error("Meta GPU spool seek failed");
    cyvp::WriteValue(out,(1ull<<63)|0x43594d000000ull|lane);
    cyvp::WriteValue(out,uint32_t(name.size()));
    if(fwrite(name.data(),1,name.size(),out)!=name.size())throw std::runtime_error("Meta GPU track write failed");
    cyvp::WriteValue(out,count);Record record{};uint64_t written=0;
    while(fread(&record,sizeof(record),1,spool)==1){cyvp::WriteValue(out,cynt::Event{record.begin,record.end,timerBase+record.timer,0,0,0});++written;}
    if(ferror(spool)||written!=count)throw std::runtime_error("Incomplete Meta GPU spool");
}
}
