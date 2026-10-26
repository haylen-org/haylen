#include "gpu/support/GpuFixture.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "core/EmbeddedFiles.hpp"
#include "gpu/DeviceFactory.hpp"
#include "support/EventLoop.hpp"

namespace haylen::test {

math::Color GpuFixture::Pixels::at(int x, int y) const {
    const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
    return math::Color::fromRgba8(bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]);
}

// A name that is not a graphics API runs once as the API of none, which fails with the name.
std::vector<gpu::Device::Api> GpuFixture::getTestedApis() {
    const char* requested = std::getenv("HAYLEN_TEST_GRAPHICS_API");
    if (requested == nullptr || *requested == '\0') {
        const std::span<const gpu::Device::Api> compiled = gpu::DeviceFactory::getCompiledApis();
        return {compiled.begin(), compiled.end()};
    }
    return {gpu::DeviceFactory::findApi(requested).value_or(gpu::Device::Api::None)};
}

std::string GpuFixture::nameApi(const ::testing::TestParamInfo<gpu::Device::Api>& info) {
    return std::string(gpu::DeviceFactory::getApiName(info.param));
}

gpu::DeviceFactory::Options GpuFixture::getDeviceOptions() {
    return {.validation = true, .software = isEnabled("HAYLEN_TEST_GPU_SOFTWARE"), .lowPower = isEnabled("HAYLEN_TEST_GPU_LOW_POWER")};
}

bool GpuFixture::isEnabled(const char* variable) {
    const char* value = std::getenv(variable);
    return value != nullptr && std::string_view(value) == "1";
}

::testing::AssertionResult GpuFixture::isNear(math::Color actual, math::Color expected, int tolerance) {
    const std::array<float, 4> got{actual.r, actual.g, actual.b, actual.a};
    const std::array<float, 4> wanted{expected.r, expected.g, expected.b, expected.a};
    for (std::size_t channel = 0; channel < got.size(); ++channel) {
        if (std::abs(got[channel] - wanted[channel]) * 255.0F > static_cast<float>(tolerance) + 0.5F) {
            return ::testing::AssertionFailure() << "The color " << actual.toHex() << " is not within " << tolerance << " of " << expected.toHex() << ".";
        }
    }
    return ::testing::AssertionSuccess();
}

// The tests wait for an API whose device arrives in a later turn of the event loop, such as WebGPU, before they make its device.
std::unique_ptr<gpu::Device> GpuFixture::makeDevice(gpu::Device::Api api, const std::optional<gpu::Surface>& surface) {
    while (!gpu::DeviceFactory::prepare(api, getDeviceOptions())) {
        EventLoop::yield();
    }
    return gpu::DeviceFactory::create(api, surface, getDeviceOptions());
}

void GpuFixture::SetUp() {
    if (GetParam() == gpu::Device::Api::None) {
        FAIL() << "The variable \"HAYLEN_TEST_GRAPHICS_API\" names \"" << std::getenv("HAYLEN_TEST_GRAPHICS_API") << "\", which is not a graphics API of \"gpu::Device::kApiNames\".";
    }

    errors = std::make_unique<LoggedErrors>();

    try {
        surface = std::make_unique<OffscreenSurface>(GetParam());
        made = makeDevice(GetParam(), surface->getSurface());
    } catch (const std::runtime_error& error) {
        GTEST_SKIP() << "The device of \"" << gpu::DeviceFactory::getApiName(GetParam()) << "\" cannot start on this machine: " << error.what();
    }
    shaders = graphics::Shader::parse(core::EmbeddedFiles::getConformanceShader());
    shaders.getResource()->graveyard = graveyard;
}

// The device goes before the surface it draws into, and the errors it logged until then belong to the test, unless the device never started.
void GpuFixture::TearDown() {
    if (made) {
        shaders = {};
        for (const gpu::Device::Pipeline pipeline : planes) {
            made->destroy(pipeline);
        }
        for (const gpu::Device::Image image : images) {
            made->destroy(image);
        }
        graveyard->collect(*made);
        made->commit();
        made.reset();
    }
    surface.reset();
    if (errors && !IsSkipped()) {
        errors->reportAsFailures();
    }
}

gpu::Device::Program GpuFixture::program(std::string_view name) {
    return shaders.getResource()->getProgram(*made, name).program;
}

int GpuFixture::attribute(std::string_view programName, std::string_view input) const {
    const graphics::ShaderResource::Attribute* found = shaders.getResource()->getInfo(programName).findAttribute(input);
    if (found == nullptr) {
        throw std::invalid_argument(std::format("The program \"{}\" has no input \"{}\".", programName, input));
    }
    return found->location;
}

gpu::Device::Image GpuFixture::makeImage(const gpu::Device::ImageDesc& desc) {
    const gpu::Device::Image image = made->makeImage(desc);
    images.push_back(image);
    return image;
}

gpu::Device::Image GpuFixture::makeTarget(int width, int height, gpu::Device::PixelFormat format, int sampleCount) {
    return makeImage({.width = width, .height = height, .sampleCount = sampleCount, .format = format, .kind = gpu::Device::ImageKind::RenderTarget, .label = "haylen-test-target"});
}

gpu::Device::Pipeline GpuFixture::makePlanePipeline(std::span<const gpu::Device::ColorTarget> colors, const gpu::Device::DepthStencil& depthStencil, int sampleCount) {
    const gpu::Device::Pipeline pipeline = made->makePipeline({.program = program("plane"), .colors = colors, .depthStencil = depthStencil, .sampleCount = sampleCount, .label = "haylen-test-plane"});
    planes.push_back(pipeline);
    return pipeline;
}

void GpuFixture::drawPlane(math::Color color, float depth) {
    const std::array<float, 8> params{color.r, color.g, color.b, color.a, depth, 0.0F, 0.0F, 0.0F};
    made->applyBindings({});
    made->applyUniforms(0, std::as_bytes(std::span(params)));
    made->draw(0, 3, 1);
}

GpuFixture::Pixels GpuFixture::read(gpu::Device::Image image, int width, int height) {
    const std::unique_ptr<gpu::Device::Readback> copy = made->readImage(image, width, height);
    const auto deadline = std::chrono::steady_clock::now() + kReadTimeout;
    while (std::chrono::steady_clock::now() < deadline) {
        std::optional<std::vector<std::uint8_t>> pixels = copy->poll();
        if (!pixels) {
            made->commit();
            EventLoop::yield();
            continue;
        }

        // A device whose render targets start at their bottom row hands that row first.
        Pixels read{.width = width, .height = height, .bytes = std::move(*pixels)};
        if (!made->getCapabilities().originTopLeft) {
            const std::size_t row = static_cast<std::size_t>(width) * 4;
            for (int top = 0, bottom = height - 1; top < bottom; ++top, --bottom) {
                std::swap_ranges(read.bytes.begin() + static_cast<std::ptrdiff_t>(row * static_cast<std::size_t>(top)), read.bytes.begin() + static_cast<std::ptrdiff_t>(row * static_cast<std::size_t>(top + 1)), read.bytes.begin() + static_cast<std::ptrdiff_t>(row * static_cast<std::size_t>(bottom)));
            }
        }
        return read;
    }
    throw std::runtime_error(std::format("The copy of a render target did not finish within {} seconds.", kReadTimeout.count()));
}

GpuFixture::Pixels GpuFixture::clearAndRead(math::Color color, int width, int height) {
    const gpu::Device::Image target = makeTarget(width, height);
    const std::array<gpu::Device::ColorAttachment, 1> colors{{{.image = target, .clear = color}}};
    made->beginPass({.colors = colors, .label = "haylen-test-clear"});
    made->endPass();
    made->commit();
    return read(target, width, height);
}

} // namespace haylen::test
