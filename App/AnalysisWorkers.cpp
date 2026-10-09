// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AnalysisWorkers.hpp"
#include <algorithm>
#include <condition_variable>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <stdexcept>
namespace cyinsights {
namespace {
std::atomic<unsigned> configured{0};
thread_local bool in_worker=false;
void Check(const std::atomic<bool>* cancel){if(cancel&&cancel->load())throw std::runtime_error("Analysis cancelled");}
class WorkerPool {
public:
    WorkerPool() {
        try {
            for(unsigned i=1;i<AnalysisWorkerLimit();++i)threads_.emplace_back([this] {
                in_worker=true;
                for(;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> guard(mutex_);
                        ready_.wait(guard,[&]{return stopping_||!tasks_.empty();});
                        if(stopping_&&tasks_.empty())return;
                        task=std::move(tasks_.front());tasks_.pop();
                    }
                    task();
                }
            });
        } catch(...) {Stop();throw;}
    }
    ~WorkerPool(){Stop();}
    std::future<void> Submit(std::function<void()> work) {
        auto task=std::make_shared<std::packaged_task<void()>>(std::move(work));auto result=task->get_future();
        {std::lock_guard<std::mutex> guard(mutex_);tasks_.emplace([task]{(*task)();});}
        ready_.notify_one();return result;
    }
private:
    void Stop(){
        {std::lock_guard<std::mutex> guard(mutex_);stopping_=true;}
        ready_.notify_all();for(auto& thread:threads_)if(thread.joinable())thread.join();
    }
    std::mutex mutex_;std::condition_variable ready_;
    std::queue<std::function<void()>> tasks_;std::vector<std::thread> threads_;bool stopping_=false;
};
WorkerPool& Pool(){static WorkerPool pool;return pool;}
}
unsigned AnalysisWorkerLimit() {
    // Avoid oversubscribing small CPUs; measurements choose at most eight
    // analysis participants, leaving cores for the UI and native analyzer.
    const auto cores=std::thread::hardware_concurrency();
    return std::max(1u,std::min(8u,cores>2?cores-2:1u));
}
unsigned AnalysisWorkerCount(){auto requested=configured.load();return requested?std::clamp(requested,1u,AnalysisWorkerLimit()):AnalysisWorkerLimit();}
unsigned AnalysisWorkerSetting(){return configured.load();}
void SetAnalysisWorkerCount(unsigned count){configured.store(count?std::clamp(count,1u,AnalysisWorkerLimit()):0u);}
void ParallelAnalysis(size_t jobs,unsigned workers,const std::function<void(size_t)>& work,const std::atomic<bool>* cancel) {
    Check(cancel);if(!jobs)return;
    workers=static_cast<unsigned>(std::min<size_t>(jobs,workers?std::min(workers,AnalysisWorkerLimit()):AnalysisWorkerCount()));
    if(workers<2||in_worker){for(size_t i=0;i<jobs;++i){Check(cancel);work(i);}return;}
    std::atomic<size_t> next{0};std::atomic<bool> failed{false};
    auto run=[&] {
        try {for(;;){Check(cancel);if(failed.load())return;auto i=next.fetch_add(1);if(i>=jobs)return;work(i);}}
        catch(...){failed.store(true);throw;}
    };
    std::vector<std::future<void>> tasks;tasks.reserve(workers-1);std::exception_ptr error;
    // Always drain queued tasks, including allocation/submit failures, before
    // releasing references captured by this batch or propagating an error.
    try {for(unsigned i=1;i<workers;++i)tasks.push_back(Pool().Submit(run));run();}
    catch(...){failed.store(true);error=std::current_exception();}
    for(auto& task:tasks)try{task.get();}catch(...){if(!error)error=std::current_exception();}
    if(error)std::rethrow_exception(error);Check(cancel);
}
}
