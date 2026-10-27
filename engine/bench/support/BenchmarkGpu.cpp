#include "support/BenchmarkGpu.hpp"

#include <cstdio>
#include <exception>
#include <format>
#include <string_view>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/OffscreenSwapchain.hpp"

namespace haylen::bench {

BenchmarkGpu::BenchmarkGpu(int argc, char** argv) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument != "--graphics-api" || index + 1 == argc) {
            throw std::invalid_argument(std::format("The benchmark takes only \"--graphics-api <name>\", not \"{}\".", argument));
        }
        const std::string_view name = argv[++index];
        api = gpu::DeviceFactory::findApi(name);
        if (!api || *api == gpu::Device::Api::None) {
            throw std::invalid_argument(std::format("The name \"{}\" is not a graphics API of \"gpu::Device::kApiNames\".", name));
        }
    }
}

BenchmarkGpu::~BenchmarkGpu() = default;

// The output goes out unbuffered, so a benchmark whose output goes to a file or a pipe keeps every row it printed even when it stops early.
int BenchmarkGpu::run(int argc, char** argv, const std::function<int(BenchmarkGpu&)>& benchmark) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    try {
        BenchmarkGpu gpu(argc, argv);
        return benchmark(gpu);
    } catch (const std::invalid_argument& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return kInvalidArguments;
    } catch (const Unavailable& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return kUnavailable;
    }
}

void BenchmarkGpu::install(platform::HeadlessHost& host) {
    if (!api) {
        return;
    }
    width = static_cast<int>(host.getFramebufferSize().x);
    height = static_cast<int>(host.getFramebufferSize().y);
    host.setGpuDeviceMaker([this] { return makeDevice(); });
}

// The device draws without validation, as an app of the Release configuration does, and the surface of an API that draws through a context of OpenGL outlives it.
std::unique_ptr<gpu::Device> BenchmarkGpu::makeDevice() {
    try {
        surface = std::make_unique<test::OffscreenSurface>(*api);
        return std::make_unique<OffscreenSwapchain>(gpu::DeviceFactory::create(*api, surface->getSurface(), {}), width, height);
    } catch (const std::runtime_error& error) {
        throw Unavailable(std::format("The device of \"{}\" cannot start on this machine: {}", gpu::DeviceFactory::getApiName(*api), error.what()));
    }
}

void BenchmarkGpu::printDevice(core::Engine& engine) const {
    const graphics::Device& graphics = engine.getGraphics();
    if (!api) {
        std::printf("Graphics API \"%.*s\": the null device of the headless host, so the frames measure the frame thread alone.\n", static_cast<int>(graphics.getApiName().size()), graphics.getApiName().data());
        return;
    }
    const std::string adapter = graphics.getAdapterName();
    std::printf("Graphics API \"%.*s\" on \"%s\", drawing every frame offscreen at %d by %d pixels.\n", static_cast<int>(graphics.getApiName().size()), graphics.getApiName().data(), adapter.c_str(), width, height);
}

void BenchmarkGpu::resetFrames() noexcept {
    frameMilliseconds = 0.0;
    frames = 0;
    gpuSeconds = 0.0;
    gpuFrames = 0;
}

void BenchmarkGpu::addFrame(core::Engine& engine, double milliseconds) noexcept {
    frameMilliseconds += milliseconds;
    ++frames;
    if (const std::optional<double> seconds = engine.getGraphics().getGpuFrameSeconds()) {
        gpuSeconds += *seconds;
        ++gpuFrames;
    }
}

std::string BenchmarkGpu::formatFrameTime() const {
    return frames == 0 ? std::string("-") : std::format("{:.3f}", frameMilliseconds / frames);
}

std::string BenchmarkGpu::formatGpuTime() const {
    return gpuFrames == 0 ? std::string("-") : std::format("{:.3f}", gpuSeconds * 1000.0 / gpuFrames);
}

} // namespace haylen::bench
