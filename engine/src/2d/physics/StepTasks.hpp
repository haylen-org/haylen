#pragma once

#include <box2d/box2d.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics2d {

// Runs the tasks of a Box2D step on the job system. Each task splits into at most one chunk per worker, and the chunk index is the worker index Box2D receives, so no two chunks of a task share per-worker memory. The stepping thread finishes every task by running the chunks no worker took yet, so a step never waits behind other work queued on the pool. Waking workers costs more than a small step saves, so a step of a small world, and every task of one chunk, runs on the stepping thread alone.
class StepTasks final {
  public:
    StepTasks(core::JobSystem& jobSystem, int workerCount) noexcept;

    StepTasks(const StepTasks&) = delete;
    StepTasks& operator=(const StepTasks&) = delete;

    // Decides before a step whether its tasks go to the workers.
    void setParallel(bool value) noexcept {
        parallel = value;
    }

    static void* enqueue(b2TaskCallback* callback, int itemCount, int minRange, void* taskContext, void* userContext);
    static void finish(void* userTask, void* userContext);

  private:
    // A worker touches the callback only after it claimed a chunk, and the stepping thread waits for every claimed chunk, so a worker that starts after the task finished finds nothing to claim.
    struct Task {
        b2TaskCallback* callback = nullptr;
        void* context = nullptr;
        int itemCount = 0;
        int chunkSize = 0;
        int chunkCount = 0;
        std::atomic<int> nextChunk = 0;
        std::atomic<int> finishedChunks = 0;
        std::mutex mutex;
        std::condition_variable finished;

        void runChunks();
    };

    core::JobSystem& jobs;
    int workers;
    bool parallel = false;

    // Box2D enqueues and finishes every task on the stepping thread, so only that thread touches this list.
    std::vector<std::shared_ptr<Task>> running;
};

} // namespace haylen::physics2d
