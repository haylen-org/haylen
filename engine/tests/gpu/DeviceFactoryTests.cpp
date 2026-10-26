#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "gpu/DeviceFactory.hpp"

namespace haylen::gpu {

// The order tries the preferred APIs the build compiled first, once each, then every other compiled API in the default order, and leaves out what the build did not compile.
TEST(DeviceFactoryTest, OrdersThePreferredCompiledApisFirst) {
    const std::span<const Device::Api> compiled = DeviceFactory::getCompiledApis();
    EXPECT_EQ(DeviceFactory::order({}), std::vector<Device::Api>(compiled.begin(), compiled.end()));

    std::vector<Device::Api> preferred(compiled.rbegin(), compiled.rend());
    preferred.push_back(Device::Api::None);
    if (!compiled.empty()) {
        preferred.push_back(compiled.back());
    }
    const std::vector<Device::Api> ordered = DeviceFactory::order(preferred);
    EXPECT_EQ(ordered, std::vector<Device::Api>(compiled.rbegin(), compiled.rend()));
}

// Only OpenGL, OpenGL ES and WebGL2 draw through a context that the platform creates with the window.
TEST(DeviceFactoryTest, TellsTheApisThatDrawThroughAGlContext) {
    for (const auto& [name, api] : Device::kApiNames) {
        const bool gl = api == Device::Api::OpenGl || api == Device::Api::OpenGlEs || api == Device::Api::WebGl2;
        EXPECT_EQ(DeviceFactory::usesGlContext(api), gl) << name;
    }
}

// The names of the APIs go both ways, and the name that means none names no API to create.
TEST(DeviceFactoryTest, ReadsAndWritesTheNamesOfTheApis) {
    for (const auto& [name, api] : Device::kApiNames) {
        EXPECT_EQ(DeviceFactory::getApiName(api), name);
        EXPECT_EQ(DeviceFactory::findApi(name), api == Device::Api::None ? std::nullopt : std::optional<Device::Api>(api)) << name;
    }
    EXPECT_FALSE(DeviceFactory::findApi("Vulkan").has_value());
}

// Only WebGPU waits for the browser before its device can be made, and every other API is ready at once.
TEST(DeviceFactoryTest, PreparesTheApisThatStartAtOnce) {
    for (const auto& [name, api] : Device::kApiNames) {
        if (api != Device::Api::WebGpu) {
            EXPECT_TRUE(DeviceFactory::prepare(api, {})) << name;
        }
    }
}

// An API that the build did not compile fails with a sentence that names it, so the platform tries the next one.
TEST(DeviceFactoryTest, RefusesApisTheBuildDidNotCompile) {
    for (const auto& [name, api] : Device::kApiNames) {
        if (std::ranges::find(DeviceFactory::getCompiledApis(), api) != DeviceFactory::getCompiledApis().end()) {
            continue;
        }
        try {
            (void)DeviceFactory::create(api, std::nullopt, {});
            ADD_FAILURE() << "The factory made a device of \"" << name << "\".";
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(std::string(error.what()), "This build has no device of the graphics API \"" + std::string(name) + "\". Add it to \"HAYLEN_GRAPHICS_APIS\" when the engine is built.");
        }
    }
}

} // namespace haylen::gpu
