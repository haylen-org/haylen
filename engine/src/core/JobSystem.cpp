#include "haylen/core/JobSystem.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "varn/runtime/Runtime.h"

namespace haylen::core {

// The chunks of one `parallelFor`. A thread touches the body only after it claimed a chunk, and the caller waits for every claimed chunk, so a worker that starts after the range ended finds nothing to claim and returns.
struct JobSystem::ParallelRange {
    const std::function<void(std::size_t, std::size_t)>* body = nullptr;
    std::size_t begin = 0;
    std::size_t end = 0;
    std::size_t chunkSize = 0;
    std::size_t chunkCount = 0;
    std::atomic<std::size_t> nextChunk = 0;
    std::atomic<std::size_t> finishedChunks = 0;
    std::mutex mutex;
    std::condition_variable finished;
    std::exception_ptr error;

    // Claims and runs chunks until none is left.
    void runChunks() {
        for (std::size_t chunk = nextChunk.fetch_add(1); chunk < chunkCount; chunk = nextChunk.fetch_add(1)) {
            const std::size_t chunkBegin = begin + chunk * chunkSize;
            try {
                (*body)(chunkBegin, std::min(end, chunkBegin + chunkSize));
            } catch (...) {
                const std::scoped_lock lock(mutex);
                if (!error) {
                    error = std::current_exception();
                }
            }
            if (finishedChunks.fetch_add(1) + 1 == chunkCount) {
                const std::scoped_lock lock(mutex);
                finished.notify_all();
            }
        }
    }
};

// The work posted to the pools, kept here rather than in the queues of the pools, which outlive the Lua state when the runtime shuts down.
struct JobSystem::Queue {
    std::mutex mutex;
    std::unordered_map<std::uint64_t, std::function<void()>> jobs;
    std::uint64_t next = 0;
};

JobSystem::JobSystem(varn::runtime::Runtime& scriptRuntime, ErrorHandler errorHandler) : runtime(scriptRuntime), onError(std::move(errorHandler)), workerCount(std::max(1U, std::thread::hardware_concurrency())), queue(std::make_shared<Queue>()) {}

std::string JobSystem::describe(const std::exception_ptr& error) {
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& exception) {
        return exception.what();
    } catch (...) {
        return "A job failed with an unknown exception.";
    }
}

// Varn drops exceptions that escape a job, so every job is wrapped to hand its failure to the frame thread instead.
std::function<void()> JobSystem::guardWorker(std::function<void()> work) {
    // clang-format off
    return [this, work = std::move(work)] {
        try {
            work();
        } catch (...) {
            postToFrame([this, message = describe(std::current_exception())] { onError(message); });
        }
    };
    // clang-format on
}

// The pool job only names the work, so the work stays with the job system until a worker takes it.
std::function<void()> JobSystem::enqueue(std::function<void()> work) {
    std::uint64_t id = 0;
    {
        const std::scoped_lock lock(queue->mutex);
        id = queue->next++;
        queue->jobs.emplace(id, guardWorker(std::move(work)));
    }

    // clang-format off
    return [target = queue, id] {
        std::function<void()> job;
        {
            const std::scoped_lock lock(target->mutex);
            const auto found = target->jobs.find(id);
            if (found == target->jobs.end()) {
                return;
            }
            job = std::move(found->second);
            target->jobs.erase(found);
        }
        job();
    };
    // clang-format on
}

void JobSystem::post(std::function<void()> work) {
    runtime.taskPool().post(enqueue(std::move(work)));
}

void JobSystem::postIo(std::function<void()> work) {
    runtime.ioPool().post(enqueue(std::move(work)));
}

void JobSystem::discardQueued() noexcept {
    std::unordered_map<std::uint64_t, std::function<void()>> dropped;
    {
        const std::scoped_lock lock(queue->mutex);
        dropped.swap(queue->jobs);
    }
}

void JobSystem::postToFrame(std::function<void()> work) {
    // clang-format off
    runtime.mainLoop().post([this, work = std::move(work)] {
        try {
            work();
        } catch (...) {
            onError(describe(std::current_exception()));
        }
    });
    // clang-format on
}

void JobSystem::parallelFor(std::size_t begin, std::size_t end, [[maybe_unused]] std::size_t grainSize, const std::function<void(std::size_t, std::size_t)>& body) {
    if (end <= begin) {
        return;
    }

#if defined(__EMSCRIPTEN__)
    // The browser build has no worker threads, so the whole range runs on the frame thread.
    body(begin, end);
#else
    const std::size_t count = end - begin;
    const std::size_t grain = std::max<std::size_t>(1, grainSize);
    const std::size_t chunkLimit = std::min(workerCount * kChunksPerWorker, (count + grain - 1) / grain);
    const std::size_t chunkSize = (count + chunkLimit - 1) / chunkLimit;
    const std::size_t chunkCount = (count + chunkSize - 1) / chunkSize;
    if (chunkCount <= 1) {
        body(begin, end);
        return;
    }

    auto range = std::make_shared<ParallelRange>();
    range->body = &body;
    range->begin = begin;
    range->end = end;
    range->chunkSize = chunkSize;
    range->chunkCount = chunkCount;

    // Workers help with the chunks while the caller takes them too, and a worker that only starts once the caller took the last chunk does nothing.
    const std::size_t helpers = std::min(workerCount, chunkCount) - 1;
    for (std::size_t helper = 0; helper < helpers; ++helper) {
        post([range] { range->runChunks(); });
    }
    range->runChunks();

    // Chunks that workers claimed may still run, and they reference `body`, so the caller waits for them even when a chunk failed.
    std::unique_lock lock(range->mutex);
    range->finished.wait(lock, [&range] { return range->finishedChunks.load() == range->chunkCount; });
    if (range->error) {
        std::rethrow_exception(range->error);
    }
#endif
}

} // namespace haylen::core
