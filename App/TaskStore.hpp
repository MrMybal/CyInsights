// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <array>
#include <atomic>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <functional>
namespace cyinsights {
enum class TaskPhase {Created,Launched,Scheduled,Started,Finished,Completed,Destroyed};
struct TaskRecord {
    uint64_t id=0,size=0;
    std::array<double,7> time{{NAN,NAN,NAN,NAN,NAN,NAN,NAN}};
    std::array<uint32_t,7> thread{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}};
    uint32_t name=0,flags=0;int32_t target=0;
    double Duration(TaskPhase first,TaskPhase last) const;
};
struct TaskLink {uint64_t from=0,to=0;double time=0;};
struct TaskPage {uint64_t matched=0;std::vector<TaskRecord> rows;};
class TaskStore {
public:
    static std::shared_ptr<TaskStore> Build(const std::filesystem::path& journal,const std::filesystem::path& binary,
        double capture_end,const std::atomic<bool>* cancel=nullptr);
    TaskPage Query(double a,double b,int64_t thread,const std::string& search,uint64_t page,const std::atomic<bool>* cancel=nullptr) const;
    TaskPage ScopeCandidates(const class PagedTimingStore& store,double a,double b,int64_t thread,const std::string& search,uint64_t page,const std::atomic<bool>* cancel=nullptr) const;
    std::optional<TaskRecord> Get(uint64_t id) const;
    void Visit(const std::function<void(const TaskRecord&)>& sink,const std::atomic<bool>* cancel=nullptr) const;
    std::vector<TaskLink> Links(uint64_t id,const std::atomic<bool>* cancel=nullptr) const;
    std::vector<std::string> names{"Unnamed task"};
    std::map<uint32_t,std::string> threads;
    uint64_t count=0,events=0,relations=0;double start=NAN,end=0;
private:
    std::filesystem::path file_,links_;
    std::vector<std::pair<uint64_t,uint64_t>> ids_;
};
bool IsTaskScope(const std::string& name);
}
