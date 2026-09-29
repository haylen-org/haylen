#pragma once

#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace varn::runtime {
class Runtime;
}

namespace haylen::core {

// Runs background work on the Varn worker pools and hands results back to the frame thread through the Varn event loop. Exceptions that escape posted work reach the error handler on the frame thread.
class JobSystem final {
  public:
    template <typename T> struct Result {
        std::optional<T> value;
        std::string error;

        [[nodiscard]] bool isOk() const noexcept {
            return value.has_value();
        }
    };

    using ErrorHandler = std::function<void(const std::string&)>;

    JobSystem(varn::runtime::Runtime& scriptRuntime, ErrorHandler errorHandler);

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void post(std::function<void()> work);
    void postIo(std::function<void()> work);
    void postToFrame(std::function<void()> work);

    // Runs work on the task pool and calls completion on the frame thread with the value or the error message.
    template <typename Work, typename Completion> void run(Work&& work, Completion&& completion) {
        using Value = std::invoke_result_t<Work>;

        // clang-format off
        post([this, work = std::forward<Work>(work), completion = std::forward<Completion>(completion)]() mutable {
            auto result = std::make_shared<Result<Value>>();
            try {
                result->value.emplace(work());
            } catch (const std::exception& exception) {
                result->error = exception.what();
            } catch (...) {
                result->error = "Background job failed with an unknown exception.";
            }
            postToFrame([completion = std::move(completion), result]() mutable { completion(std::move(*result)); });
        });
        // clang-format on
    }

    // Splits [begin, end) into chunks of at least grainSize items and runs them in parallel, including on the calling thread. Only the frame thread may call it.
    void parallelFor(std::size_t begin, std::size_t end, std::size_t grainSize, const std::function<void(std::size_t, std::size_t)>& body);

    [[nodiscard]] std::size_t getWorkerCount() const noexcept {
        return workerCount;
    }

  private:
    struct ChunkCompletion;

    [[nodiscard]] static std::string describe(const std::exception_ptr& error);
    [[nodiscard]] std::function<void()> guardWorker(std::function<void()> work);

    varn::runtime::Runtime& runtime;
    ErrorHandler onError;
    std::size_t workerCount;
};

} // namespace haylen::core
