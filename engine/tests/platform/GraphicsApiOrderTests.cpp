#include <gtest/gtest.h>

#include <array>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "platform/GraphicsApiOrder.hpp"

namespace haylen::platform {

// The command line overrides the APIs of `app.json`, and both only move the APIs of the build to the front of the order of the platform.
TEST(GraphicsApiOrderTest, PutsTheCommandLineBeforeTheAppAndThePlatform) {
    const std::array<gpu::Device::Api, 2> configured{gpu::Device::Api::OpenGl, gpu::Device::Api::Vulkan};
    EXPECT_EQ(GraphicsApiOrder::resolve("", configured), gpu::DeviceFactory::order(configured));
    const std::array<gpu::Device::Api, 1> metal{gpu::Device::Api::Metal};
    EXPECT_EQ(GraphicsApiOrder::resolve("metal", configured), gpu::DeviceFactory::order(metal));
    const std::span<const gpu::Device::Api> compiled = gpu::DeviceFactory::getCompiledApis();
    EXPECT_EQ(GraphicsApiOrder::resolve("", {}), std::vector<gpu::Device::Api>(compiled.begin(), compiled.end()));
}

TEST(GraphicsApiOrderTest, RefusesANameThatIsNotAGraphicsApi) {
    try {
        (void)GraphicsApiOrder::resolve("glide", {});
        ADD_FAILURE() << "An unknown graphics API was taken.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The graphics API \"glide\" of \"--graphics-api\" is unknown. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".");
    }
    EXPECT_THROW((void)GraphicsApiOrder::resolve("none", {}), std::invalid_argument);
}

} // namespace haylen::platform
