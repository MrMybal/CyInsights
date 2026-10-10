// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
namespace cyinsights {
enum ActivityFlags : uint32_t {ActivityPoint=1,ActivityIncomplete=2,ActivityFailed=4,ActivityContentBits=8,ActivityGauge=16,ActivityBucketCount=32,ActivitySignedGauge=64};
struct ActivityRecord {double start=0,end=0;uint64_t value=0,identity=0;uint32_t thread=UINT32_MAX,flags=0,name=0;};
struct ActivityLane {std::string kind,label,unit;uint64_t count=0;};
struct ActivityBin {uint64_t count=0;double minimum=0,maximum=0;bool valid=false;};
struct ActivityLaneWindow {
 uint32_t lane=0;uint64_t matched=0,complete=0,points=0,content=0,value_sum=0,minimum=0,maximum=0;
 int64_t signed_minimum=0,signed_maximum=0;bool signed_values=false;
 double inclusive_seconds=0;std::vector<ActivityBin> bins;std::vector<ActivityRecord> rows;
};
struct ActivityWindow {double start=0,end=0;std::vector<ActivityLaneWindow> lanes;uint64_t blocks_read=0;};
// Append-only disk cache; block metadata and dictionaries are the resident data.
class ActivityStore {
public:
 static std::shared_ptr<ActivityStore> Build(const std::filesystem::path& journal,const std::filesystem::path& binary,const std::atomic<bool>* cancel=nullptr);
 ActivityWindow Query(double a,double b,const std::vector<uint32_t>& lanes,unsigned pixels,const std::string& search={},uint64_t page=0,const std::atomic<bool>* cancel=nullptr) const;
 std::vector<ActivityLane> lanes;std::vector<std::string> names;uint64_t count=0;double start=0,end=0;
private:
 struct Block {uint64_t offset;uint32_t count;double start,end;};
 struct Index {std::vector<Block> blocks;std::vector<double> prefix_end;};
 std::vector<Index> indexes_;std::filesystem::path file_;
};
}
