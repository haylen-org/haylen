#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace haylen::debug {

// Records spans of time from any thread into a bounded buffer and writes them in the trace event JSON format that the profilers of browsers and trace viewers open, with one lane per thread. The profiler records its scopes and frames into it on the frame thread, the job system records the jobs of its workers, and the renderer records how long the GPU took for each frame where the backend measures it. Recording stops taking spans once the buffer is full, and counts the spans it dropped.
class TraceRecorder final {
  public:
    static constexpr std::size_t kDefaultCapacity = 262144;

    // A span in microseconds since the recorder started, on the lane of a thread, or on the lane of the GPU.
    struct Span {
        std::uint32_t name = 0;
        std::uint32_t lane = 0;
        double start = 0.0;
        double duration = 0.0;
    };

    explicit TraceRecorder(std::size_t spanCount = kDefaultCapacity);

    // Records a span of the calling thread between two times of `now`.
    void record(std::string_view name, double startSeconds, double endSeconds);

    // Records a span on the lane of the GPU, which ends at the time the GPU reported it.
    void recordGpu(std::string_view name, double endSeconds, double durationSeconds);

    // Names the lane of the calling thread, such as `frame`, which every span of the thread shows under.
    void nameThread(std::string_view name);

    // The seconds of the steady clock that every span measures with.
    [[nodiscard]] static double now() noexcept;

    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::size_t getDropped() const;

    // Writes the spans as `{"traceEvents": [...], "displayTimeUnit": "ms"}`, with a complete event of phase `X` for each span and the names of the lanes as metadata events.
    [[nodiscard]] std::string toJson() const;

  private:
    struct NameHash {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    static constexpr std::uint32_t kGpuLane = 0;

    [[nodiscard]] std::uint32_t intern(std::string_view name);
    [[nodiscard]] std::uint32_t laneOfThisThread();
    void append(std::string_view name, std::uint32_t lane, double startSeconds, double durationSeconds);

    mutable std::mutex mutex;
    std::vector<Span> spans;
    std::vector<std::string> names;
    std::unordered_map<std::string, std::uint32_t, NameHash, std::equal_to<>> nameIndices;
    std::vector<std::thread::id> threads;
    std::vector<std::string> laneNames;
    std::size_t capacity;
    std::size_t dropped = 0;
    double origin;
};

} // namespace haylen::debug
