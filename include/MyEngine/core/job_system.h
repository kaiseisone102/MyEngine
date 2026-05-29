#pragma once
// =============================================================================
// JobSystem -- Foundations Audit \xc2\xa72 (\xe2\x98\x85\xe2\x98\x85\xe2\x98\x85) worker thread receptacle
// =============================================================================
// Foundations \xc2\xa72 explicitly named worker-thread pool + transfer queue (C)
// as the hidden prerequisites of Phase 2F streaming. C landed in commit
// e7b852e; U lands the worker-thread side as a thin, deliberately minimal
// receptacle:
//
//   - The API is what every Phase 2F call site will use: submit(work) ->
//     future, wait() to drain. Once activated, streaming asset upload,
//     decode, and chunk eviction all flow through this queue.
//   - The implementation is a textbook condition-variable thread pool:
//     N std::thread workers blocking on a shared queue + cv, picking up
//     std::packaged_task entries. Lock-free / work-stealing variants are
//     orthogonal optimisations that can land later without changing the
//     surface above.
//   - init(0) (or omitting init entirely) keeps the JobSystem in a "ready
//     but inert" state where submit() runs the work inline on the calling
//     thread. This is what the engine does today: no streaming load, no
//     async decode, the receptacle is here but nothing fans out yet.
//
// The single hardest part of retrofitting threading into a mature engine
// is the implicit "everything runs on main" assumption baked into call
// sites (ResourceFactory::copyBufferRegion vkQueueWaitIdle, AssetRegistry
// init order, static caches). Landing this receptacle now means new code
// can write its async path against JobSystem from day 1; the existing
// synchronous code stays single-threaded until the gradual cut-over.
// =============================================================================
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace myengine::core {

class JobSystem {
   public:
    JobSystem() = default;
    ~JobSystem() { shutdown(); }

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Spin up `workerCount` worker threads. workerCount = 0 keeps the
    // JobSystem inert -- submit() falls back to inline execution.
    void init(uint32_t workerCount) {
        if (running_.exchange(true)) return;
        for (uint32_t i = 0; i < workerCount; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    // Stop the workers (best-effort: in-flight jobs complete, queued jobs
    // are dropped). Safe to call multiple times.
    void shutdown() {
        if (!running_.exchange(false)) return;
        {
            std::lock_guard<std::mutex> lock(mu_);
            stop_ = true;
        }
        cv_.notify_all();
        for (std::thread& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
        std::queue<std::packaged_task<void()>>().swap(queue_);
    }

    // Submit a job. Returns a future the caller can block on or detach.
    // If there are no workers (workerCount == 0 or shutdown), the job runs
    // inline on the calling thread so existing single-threaded code paths
    // continue to work without a branch.
    std::future<void> submit(std::function<void()> job) {
        std::packaged_task<void()> task(std::move(job));
        std::future<void> fut = task.get_future();
        if (workers_.empty()) {
            // Inline execution -- the receptacle is here, the worker pool
            // is not yet sized up.
            task();
            return fut;
        }
        {
            std::lock_guard<std::mutex> lock(mu_);
            queue_.push(std::move(task));
        }
        cv_.notify_one();
        return fut;
    }

    // Block until the queue drains. Useful at shutdown or major scene
    // transitions so the next frame does not race a half-decoded asset.
    void wait() {
        std::unique_lock<std::mutex> lock(mu_);
        cv_drained_.wait(lock, [this] { return queue_.empty() && busy_ == 0; });
    }

    uint32_t workerCount() const noexcept {
        return static_cast<uint32_t>(workers_.size());
    }
    bool active() const noexcept { return running_.load(); }

   private:
    std::vector<std::thread> workers_;
    std::queue<std::packaged_task<void()>> queue_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::condition_variable cv_drained_;
    std::atomic<bool> running_{false};
    bool stop_ = false;
    uint32_t busy_ = 0;

    void workerLoop() {
        while (true) {
            std::packaged_task<void()> task;
            {
                std::unique_lock<std::mutex> lock(mu_);
                cv_.wait(lock, [this] { return stop_ || !queue_.empty(); });
                if (stop_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop();
                ++busy_;
            }
            task();
            {
                std::lock_guard<std::mutex> lock(mu_);
                --busy_;
                if (queue_.empty() && busy_ == 0) cv_drained_.notify_all();
            }
        }
    }
};

}  // namespace myengine::core
