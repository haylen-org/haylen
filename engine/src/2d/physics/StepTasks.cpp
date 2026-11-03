#include "2d/physics/StepTasks.hpp"

#include <algorithm>

#include "haylen/core/JobSystem.hpp"

namespace haylen::physics2d {

StepTasks::StepTasks(core::JobSystem& jobSystem) noexcept : jobs(jobSystem) {}

void StepTasks::Task::run() {
    if (claimed.exchange(true)) {
        return;
    }
    callback(context);
    const std::scoped_lock lock(mutex);
    done = true;
    finished.notify_all();
}

std::shared_ptr<StepTasks::Task> StepTasks::takeTask() {
    if (idle.empty()) {
        return std::make_shared<Task>();
    }
    std::shared_ptr<Task> task = std::move(idle.back());
    idle.pop_back();
    task->claimed = false;
    task->done = false;
    return task;
}

void* StepTasks::enqueue(b2TaskCallbackFcn* callback, void* taskContext, void* userContext) {
    auto& self = *static_cast<StepTasks*>(userContext);
    std::shared_ptr<Task> task = self.takeTask();
    task->callback = callback;
    task->context = taskContext;
    task->posted = self.parallel;
    if (task->posted) {
        self.jobs.post([task] { task->run(); });
    }
    return self.running.emplace_back(std::move(task)).get();
}

void StepTasks::finish(void* userTask, void* userContext) {
    auto& self = *static_cast<StepTasks*>(userContext);
    auto* task = static_cast<Task*>(userTask);
    task->run();
    {
        std::unique_lock lock(task->mutex);
        task->finished.wait(lock, [task] { return task->done; });
    }

    const auto found = std::ranges::find_if(self.running, [task](const std::shared_ptr<Task>& item) { return item.get() == task; });
    if (!task->posted) {
        self.idle.push_back(std::move(*found));
    }
    self.running.erase(found);
}

} // namespace haylen::physics2d
