#pragma once

#include <box2d/box2d.h>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <vector>

namespace haylen::core {
class JobSystem;
}

namespace haylen::physics2d {

// Runs the tasks of a Box2D step on the job system. Box2D splits its work into one task per worker itself, so each task goes to the pool whole. The stepping thread finishes a task by running it when no worker took it yet, so a step never waits behind other work queued on the pool. Waking workers costs more than a small step saves, so the tasks of a step of a small world wait for the stepping thread to finish them. They never run inside the call that enqueues them, because the solver tasks of the workers wait for the one the stepping thread runs.
class StepTasks final {
  public:
    explicit StepTasks(core::JobSystem& jobSystem) noexcept;

    StepTasks(const StepTasks&) = delete;
    StepTasks& operator=(const StepTasks&) = delete;

    // Decides before a step whether its tasks go to the workers.
    void setParallel(bool value) noexcept {
        parallel = value;
    }

    static void* enqueue(b2TaskCallbackFcn* callback, void* taskContext, void* userContext);
    static void finish(void* userTask, void* userContext);

  private:
    // Whoever claims a task first runs it, a worker or the stepping thread, and the stepping thread waits for a task a worker claimed. A task that never went to the pool serves the next steps again, so a step of a small world allocates no task once the first ones exist.
    struct Task {
        b2TaskCallbackFcn* callback = nullptr;
        void* context = nullptr;
        bool posted = false;
        std::atomic<bool> claimed = false;
        bool done = false;
        std::mutex mutex;
        std::condition_variable finished;

        void run();
    };

    [[nodiscard]] std::shared_ptr<Task> takeTask();

    core::JobSystem& jobs;
    bool parallel = false;

    // Box2D enqueues and finishes every task on the stepping thread, so only that thread touches these lists.
    std::vector<std::shared_ptr<Task>> running;
    std::vector<std::shared_ptr<Task>> idle;
};

} // namespace haylen::physics2d
