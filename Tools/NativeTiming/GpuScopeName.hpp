// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cynt {
// New Unreal GPU profilers store formatted RDG names as one CBOR text value.
// Decode only that contract. Never execute a format string supplied by a trace.
inline std::string_view GpuScopeName(const uint8_t* bytes,size_t size) {
    if(!bytes||!size||(bytes[0]>>5)!=3)return {};
    uint64_t length=bytes[0]&31;size_t header=1;
    if(length>=24) {
        if(length>27)return {}; // No indefinite strings or reserved lengths.
        const size_t n=size_t(1)<<(length-24);
        if(size<1+n)return {};
        length=0;for(size_t i=0;i<n;++i)length=(length<<8)|bytes[1+i];
        header+=n;
    }
    if(!length||length!=size-header)return {};
    // UTF-8 validation: reject NUL, overlong encodings and invalid Unicode.
    for(size_t i=header;i<size;) {
        const uint8_t c=bytes[i++];if(!c)return {};
        if(c<0x80)continue;
        unsigned n;uint32_t value,minimum;
        if(c>=0xc2&&c<=0xdf){n=1;value=c&31;minimum=0x80;}
        else if(c>=0xe0&&c<=0xef){n=2;value=c&15;minimum=0x800;}
        else if(c>=0xf0&&c<=0xf4){n=3;value=c&7;minimum=0x10000;}
        else return {};
        if(size-i<n)return {};
        for(unsigned j=0;j<n;++j){const auto next=bytes[i++];if((next&0xc0)!=0x80)return {};value=(value<<6)|(next&63);}
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return {};
    }
    return {reinterpret_cast<const char*>(bytes+header),static_cast<size_t>(length)};
}
}
