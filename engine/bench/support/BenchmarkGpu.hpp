#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "gpu/support/OffscreenSurface.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::core {
class Engine;
}

namespace haylen::platform {
class HeadlessHost;
}

namespace haylen::bench {

// The GPU device that a benchmark of the renderer draws through: the null device of the headless host, which measures what the frame thread records without a GPU, or the device of the graphics API that the option `--graphics-api` names, which draws every frame offscreen on the GPU of this machine at the size of the framebuffer of the host.
class BenchmarkGpu final {
  public:
    // The exit status of a benchmark whose arguments it does not take, of one whose device cannot start on this machine, and of one whose device the GPU lost while it measured.
    static constexpr int kInvalidArguments = 2;
    static constexpr int kUnavailable = 3;
    static constexpr int kLost = 4;

    // Thrown when the device of the API cannot start on this machine.
    class Unavailable final : public std::runtime_error {
      public:
        using std::runtime_error::runtime_error;
    };

    // Thrown when the GPU lost the device, such as in a reset of the GPU, since the frames that follow measure nothing.
    class Lost final : public std::runtime_error {
      public:
        using std::runtime_error::runtime_error;
    };

    // Reads `--graphics-api <name>` from the arguments of the benchmark. Throws `std::invalid_argument` with the reason for any other argument or a name that is not an API of the build.
    BenchmarkGpu(int argc, char** argv);
    BenchmarkGpu(const BenchmarkGpu&) = delete;
    BenchmarkGpu& operator=(const BenchmarkGpu&) = delete;
    ~BenchmarkGpu();

    // Runs a benchmark with the device that its arguments name and returns its exit status, or `kInvalidArguments`, `kUnavailable` or `kLost` with the reason on the standard error.
    static int run(int argc, char** argv, const std::function<int(BenchmarkGpu&)>& benchmark);

    // Makes the engines that the host runs draw through the device of the API, when the arguments named one, and keeps the null device otherwise.
    void install(platform::HeadlessHost& host);

    // Prints the line that names the graphics API and the GPU that the frames of the engine draw through.
    void printDevice(core::Engine& engine) const;

    // Starts a measurement of whole frames, adds each frame once it ended with the CPU milliseconds it took and the time the GPU took for the last frame it finished, and formats the average milliseconds per frame for the columns of the output, a dash where the measurement had no frame or nothing measures the GPU. Adding a frame throws `Lost` once the GPU lost the device.
    void resetFrames() noexcept;
    void addFrame(core::Engine& engine, double milliseconds);
    [[nodiscard]] std::string formatFrameTime() const;
    [[nodiscard]] std::string formatGpuTime() const;

  private:
    [[nodiscard]] std::unique_ptr<gpu::Device> makeDevice();

    std::optional<gpu::Device::Api> api;
    std::unique_ptr<test::OffscreenSurface> surface;
    gpu::Device* device = nullptr;
    int width = 0;
    int height = 0;
    double frameMilliseconds = 0.0;
    int frames = 0;
    double gpuSeconds = 0.0;
    int gpuFrames = 0;
};

} // namespace haylen::bench
