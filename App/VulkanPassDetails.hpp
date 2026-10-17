// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingEvent.hpp"
#include "../Tools/NativeTiming/VulkanPassDetailFormat.hpp"
#include <Windows.h>
#include <filesystem>
#include <atomic>
#include <memory>
#include <vector>
namespace cyinsights {
class VulkanPassDetails {
    struct Block{uint64_t first,count;double begin,end;};
    HANDLE file_=INVALID_HANDLE_VALUE,map_=nullptr;const unsigned char* bytes_=nullptr;
    std::vector<Block> blocks_;
    cyvpd::Record Record(uint64_t index)const;
public:
    uint64_t count=0;
    ~VulkanPassDetails();
    static std::shared_ptr<VulkanPassDetails> Open(const std::filesystem::path&,std::string&,const std::atomic<bool>* cancel=nullptr);
    std::vector<cyvpd::Record> Find(const TimingEvent&,bool& ambiguous)const;
    std::vector<cyvpd::Record> Query(double begin,double end,size_t limit,uint64_t& matched,const std::atomic<bool>* cancel=nullptr,uint64_t pass=0,uint64_t offset=0)const;
};
}
