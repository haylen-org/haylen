#include "2d/physics/StepTasks.hpp"

#include <algorithm>

#include "haylen/core/JobSystem.hpp"

namespace haylen::physics2d {

StepTasks::StepTasks(core::JobSystem& jobSystem, int workerCount) noexcept : jobs(jobSystem), workers(workerCount) {}

void StepTasks::Task::runChunks() {
    for (int chunk = nextChunk.fetch_add(1); chunk < chunkCount; chunk = nextChunk.fetch_add(1)) {
        const int begin = chunk * chunkSize;
        callback(begin, std::min(itemCount, begin + chunkSize), static_cast<std::uint32_t>(chunk), context);
        if (finishedChunks.fetch_add(1) + 1 == chunkCount) {
            const std::scoped_lock lock(mutex);
            finished.notify_all();
        }
    }
}

void* StepTasks::enqueue(b2TaskCallback* callback, int itemCount, int minRange, void* taskContext, void* userContext) {
    auto& self = *static_cast<StepTasks*>(userContext);
    const int range = std::max(1, minRange);

    // Box2D runs a task that returns nothing as finished. The solver workers past the first then find the solve done and return at once.
    if (!self.parallel || itemCount <= range) {
        callback(0, itemCount, 0, taskContext);
        return nullptr;
    }
    auto task = std::make_shared<Task>();
    task->callback = callback;
    task->context = taskContext;
    task->itemCount = itemCount;
    task->chunkCount = std::clamp((itemCount + range - 1) / range, 1, self.workers);
    task->chunkSize = (itemCount + task->chunkCount - 1) / task->chunkCount;

    // Box2D enqueues the solver workers one task at a time before it finishes any, so every chunk goes to the pool, and the stepping thread takes what is left when it finishes the task.
    for (int chunk = 0; chunk < task->chunkCount; ++chunk) {
        self.jobs.post([task] { task->runChunks(); });
    }
    return self.running.emplace_back(std::move(task)).get();
}

void StepTasks::finish(void* userTask, void* userContext) {
    auto& self = *static_cast<StepTasks*>(userContext);
    auto* task = static_cast<Task*>(userTask);
    task->runChunks();
    {
        std::unique_lock lock(task->mutex);
        task->finished.wait(lock, [task] { return task->finishedChunks.load() == task->chunkCount; });
    }
    std::erase_if(self.running, [task](const std::shared_ptr<Task>& item) { return item.get() == task; });
}

} // namespace haylen::physics2d
