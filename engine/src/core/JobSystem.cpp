#include "haylen/core/JobSystem.hpp"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "varn/runtime/Runtime.h"

namespace haylen::core {

struct JobSystem::ChunkCompletion {
    std::mutex mutex;
    std::condition_variable finished;
    std::size_t remaining = 0;
    std::exception_ptr error;
};

JobSystem::JobSystem(varn::runtime::Runtime& scriptRuntime, ErrorHandler errorHandler) : runtime(scriptRuntime), onError(std::move(errorHandler)), workerCount(std::max(1U, std::thread::hardware_concurrency())) {}

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

void JobSystem::post(std::function<void()> work) {
    runtime.taskPool().post(guardWorker(std::move(work)));
}

void JobSystem::postIo(std::function<void()> work) {
    runtime.ioPool().post(guardWorker(std::move(work)));
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
    const std::size_t chunkLimit = std::min(workerCount, (count + grain - 1) / grain);
    const std::size_t chunkSize = (count + chunkLimit - 1) / chunkLimit;
    const std::size_t chunkCount = (count + chunkSize - 1) / chunkSize;
    if (chunkCount <= 1) {
        body(begin, end);
        return;
    }

    // Every chunk after the first goes to the task pool while the caller runs the first chunk itself.
    auto completion = std::make_shared<ChunkCompletion>();
    completion->remaining = chunkCount - 1;
    for (std::size_t chunk = 1; chunk < chunkCount; ++chunk) {
        const std::size_t chunkBegin = begin + chunk * chunkSize;
        const std::size_t chunkEnd = std::min(end, chunkBegin + chunkSize);

        // clang-format off
        post([&body, chunkBegin, chunkEnd, completion] {
            std::exception_ptr error;
            try {
                body(chunkBegin, chunkEnd);
            } catch (...) {
                error = std::current_exception();
            }

            std::scoped_lock lock(completion->mutex);
            if (error && !completion->error) {
                completion->error = error;
            }
            if (--completion->remaining == 0) {
                completion->finished.notify_all();
            }
        });
        // clang-format on
    }

    std::exception_ptr callerError;
    try {
        body(begin, begin + chunkSize);
    } catch (...) {
        callerError = std::current_exception();
    }

    // The pool chunks reference body, so the caller waits for all of them even when its own chunk failed.
    std::unique_lock lock(completion->mutex);
    completion->finished.wait(lock, [&completion] { return completion->remaining == 0; });

    if (callerError) {
        std::rethrow_exception(callerError);
    }
    if (completion->error) {
        std::rethrow_exception(completion->error);
    }
#endif
}

} // namespace haylen::core
