#pragma once

#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace varn::runtime {
class Scheduler;
}

namespace haylen::debug {
class TraceRecorder;
}

namespace haylen::core {

// Owns the scheduler of Varn, with its worker pools and its event loop, runs background work on the pools and hands results back to the frame thread through the event loop. Exceptions that escape posted work reach the error handler on the frame thread.
class JobSystem final {
  public:
    template <typename T> struct Result {
        std::optional<T> value{};
        std::string error;

        [[nodiscard]] bool isOk() const noexcept {
            return value.has_value();
        }
    };

    // What a poll of the event loop did: whether any job, timer or socket callback ran, whether work remains, and the milliseconds the loop can wait before it has work of its own, which is -1 when only work from outside can bring any.
    struct Poll {
        bool ran = false;
        bool pending = false;
        long long idleMilliseconds = -1;
    };

    using ErrorHandler = std::function<void(const std::string&)>;

    explicit JobSystem(ErrorHandler errorHandler);
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    // Runs what the event loop holds on the calling thread, the frame thread, such as the results of jobs, timers and the callbacks of sockets and HTTP, in passes that repeat while they make progress and the budget lasts.
    Poll poll(std::chrono::nanoseconds budget);

    // The scheduler that runs the pools and the event loop, which the HTTP client of the content and the runtimes of scripts run on.
    [[nodiscard]] varn::runtime::Scheduler& getScheduler() noexcept {
        return *scheduler;
    }

    void post(std::function<void()> work);
    void postIo(std::function<void()> work);
    void postToFrame(std::function<void()> work);

    // Runs work on the task pool, or blocking work such as reading files on the I/O pool, and calls `completion` on the frame thread with the value or the error message.
    template <typename Work, typename Completion> void run(Work&& work, Completion&& completion) {
        post(wrap(std::forward<Work>(work), std::forward<Completion>(completion)));
    }
    template <typename Work, typename Completion> void runIo(Work&& work, Completion&& completion) {
        postIo(wrap(std::forward<Work>(work), std::forward<Completion>(completion)));
    }

    // Splits `[begin, end)` into chunks of at least `grainSize` items and runs them in parallel, including on the calling thread. Workers and the caller take chunks one at a time, so the caller runs every chunk no worker started and never waits behind other work queued on the task pool. Only the frame thread may call it.
    void parallelFor(std::size_t begin, std::size_t end, std::size_t grainSize, const std::function<void(std::size_t, std::size_t)>& body);

    // Stops the pools and the event loop, drops the posted work that no worker started, and from then on drops what is posted at once, without running it and without reaching the pools or the loop. The engine calls it once the services that post work let go, before the runtimes of scripts close, so what that work holds, such as the references of a script, goes while the runtime that owns them is still open.
    void stop() noexcept;

    [[nodiscard]] std::size_t getWorkerCount() const noexcept {
        return workerCount;
    }

    // Records every job that a worker runs from now on into the recorder as a span named `job` or `ioJob` on the lane of its thread, with the trace spans of the job inside it, until it is set to null.
    void setTraceRecorder(std::shared_ptr<debug::TraceRecorder> value);

  private:
    // The job that runs work, catches its failure and hands the result to the frame thread.
    template <typename Work, typename Completion> std::function<void()> wrap(Work&& work, Completion&& completion) {
        using Value = std::invoke_result_t<Work>;

        // clang-format off
        return [this, work = std::forward<Work>(work), completion = std::forward<Completion>(completion)]() mutable {
            auto result = std::make_shared<Result<Value>>();
            try {
                result->value.emplace(work());
            } catch (const std::exception& exception) {
                result->error = exception.what();
            } catch (...) {
                result->error = kUnknownFailure;
            }

            postToFrame([completion = std::move(completion), result]() mutable { completion(std::move(*result)); });
        };
        // clang-format on
    }

    struct ParallelRange;
    struct Queue;

    static constexpr std::string_view kUnknownFailure = "A job failed with an unknown exception.";

    // Chunks per worker, so threads that start late or run slow chunks still share the range evenly.
    static constexpr std::size_t kChunksPerWorker = 4;

    [[nodiscard]] static std::string describe(const std::exception_ptr& error);
    [[nodiscard]] std::function<void()> guardWorker(std::function<void()> work, std::string_view name, std::shared_ptr<debug::TraceRecorder> recorder);
    [[nodiscard]] std::function<void()> enqueue(std::function<void()> work, std::string_view name);
    [[nodiscard]] bool isStopped() const;

    std::unique_ptr<varn::runtime::Scheduler> scheduler;
    ErrorHandler onError;
    std::size_t workerCount;
    std::shared_ptr<Queue> queue;
    std::shared_ptr<debug::TraceRecorder> tracer;
};

} // namespace haylen::core
