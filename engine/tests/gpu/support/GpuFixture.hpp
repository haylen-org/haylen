#pragma once

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "gpu/support/LoggedErrors.hpp"
#include "gpu/support/OffscreenSurface.hpp"
#include "graphics/ResourceGraveyard.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::test {

// Runs a test on the device of each graphics API that the build compiled, or of the one that the environment variable `HAYLEN_TEST_GRAPHICS_API` names, drawing offscreen with the validation of the API on and the GPU that `getDeviceOptions` picks. A device that cannot start on this machine skips the test with the reason, or fails it when the variable names its API, and an error that the device logs, such as a message of the validation of its API, fails it. The programs come from `engine/tests/gpu/shaders/conformance.glsl`.
class GpuFixture : public ::testing::TestWithParam<gpu::Device::Api> {
  public:
    // The pixels that a test reads back, four bytes a pixel with the top row first.
    struct Pixels {
        int width = 0;
        int height = 0;
        std::vector<std::uint8_t> bytes;

        [[nodiscard]] math::Color at(int x, int y) const;
    };

    // How long a copy of a render target, or the first measure of the GPU, may take before a test gives up on it.
    static constexpr std::chrono::seconds kReadTimeout{10};

    // The APIs the tests run on, which `INSTANTIATE_TEST_SUITE_P` takes, and the name each run of a test takes after its API.
    [[nodiscard]] static std::vector<gpu::Device::Api> getTestedApis();
    [[nodiscard]] static std::string nameApi(const ::testing::TestParamInfo<gpu::Device::Api>& info);

    // Whether the environment variable `HAYLEN_TEST_GRAPHICS_API` names the API the tests run on, so a device that cannot start fails them instead of skipping them, as a run of continuous integration that counts on that device needs.
    [[nodiscard]] static bool isApiRequired();

    // The options the tests make devices with: validation on, the software rasterizer of the API when the environment variable `HAYLEN_TEST_GPU_SOFTWARE` is `1`, and the GPU that uses the least power when `HAYLEN_TEST_GPU_LOW_POWER` is `1`.
    [[nodiscard]] static gpu::DeviceFactory::Options getDeviceOptions();

    // Makes what the API draws into and the device of the API with its validation on, once the API is ready to make it. The API prepares before the surface opens, since the drivers of a laptop with two GPUs pick the GPU of OpenGL when its first context opens.
    [[nodiscard]] static std::unique_ptr<gpu::Device> makeDevice(gpu::Device::Api api, std::unique_ptr<OffscreenSurface>& surface);

    // Whether the channels of a color lie within the tolerance in 255ths of the expected ones.
    [[nodiscard]] static ::testing::AssertionResult isNear(math::Color actual, math::Color expected, int tolerance = 2);

  protected:
    void SetUp() override;
    void TearDown() override;

    [[nodiscard]] gpu::Device& device() noexcept {
        return *made;
    }
    [[nodiscard]] const gpu::Device::Capabilities& capabilities() const noexcept {
        return made->getCapabilities();
    }

    [[nodiscard]] gpu::Device::Program program(std::string_view name);
    [[nodiscard]] int attribute(std::string_view programName, std::string_view input) const;

    // Makes an image that the fixture destroys when the test ends, and a render target of the size, format and sample count.
    [[nodiscard]] gpu::Device::Image makeImage(const gpu::Device::ImageDesc& desc);
    [[nodiscard]] gpu::Device::Image makeTarget(int width, int height, gpu::Device::PixelFormat format = gpu::Device::PixelFormat::Rgba8, int sampleCount = 1);

    // A pipeline of the plane program, which fills the viewport with the color and depth of its uniforms, into targets of the formats.
    [[nodiscard]] gpu::Device::Pipeline makePlanePipeline(std::span<const gpu::Device::ColorTarget> colors, const gpu::Device::DepthStencil& depthStencil = {}, int sampleCount = 1);

    // Draws the plane program, whose pipeline the pass applied, in the color at the depth.
    void drawPlane(math::Color color, float depth = 0.0F);

    // Commits empty frames until the copy of the render target, which a committed frame rendered, finishes on the GPU.
    [[nodiscard]] Pixels read(gpu::Device::Image image, int width, int height);

    // Draws one pass that clears a render target of one color and the size, and reads it back.
    [[nodiscard]] Pixels clearAndRead(math::Color color, int width, int height);

  private:
    [[nodiscard]] static bool isEnabled(const char* variable);

    std::unique_ptr<OffscreenSurface> surface;
    std::unique_ptr<gpu::Device> made;
    graphics::Shader shaders;
    std::shared_ptr<graphics::ResourceGraveyard> graveyard = std::make_shared<graphics::ResourceGraveyard>();
    std::vector<gpu::Device::Image> images;
    std::vector<gpu::Device::Pipeline> planes;
    std::unique_ptr<LoggedErrors> errors;
};

} // namespace haylen::test
