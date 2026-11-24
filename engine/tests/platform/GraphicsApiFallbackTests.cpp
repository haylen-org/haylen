#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Log.hpp"
#include "platform/GraphicsApiFallback.hpp"

namespace haylen::platform {

class GraphicsApiFallbackTest : public ::testing::Test {
  protected:
    // A window that records what the fallback asks of it, whose graphics APIs prepare, open and start as the test says.
    class RecordingWindow final : public GraphicsApiFallback::Window {
      public:
        [[nodiscard]] bool prepare(gpu::Device::Api api) override {
            calls.push_back("prepare " + name(api));
            return !preparing.contains(api);
        }

        [[nodiscard]] gpu::Device::Api getApi() const noexcept override {
            return openApi;
        }

        void open(gpu::Device::Api api) override {
            calls.push_back("open " + name(api));
            if (refusedWindows.contains(api)) {
                throw std::runtime_error("The window refused this API.");
            }

            openApi = api;
        }

        [[nodiscard]] bool hasArea() override {
            return area;
        }

        [[nodiscard]] std::unique_ptr<gpu::Device> createDevice(gpu::Device::Api api) override {
            calls.push_back("create " + name(api));
            if (refusedDevices.contains(api)) {
                throw std::runtime_error("The device refused to start.");
            }

            return std::make_unique<gpu::NullDevice>(gpu::Device::Language::None, gpu::NullDevice::getDefaultCapabilities(), api);
        }

        void close() noexcept override {
            calls.push_back("close");
            openApi = gpu::Device::Api::None;
        }

        std::set<gpu::Device::Api> preparing;
        std::set<gpu::Device::Api> refusedWindows;
        std::set<gpu::Device::Api> refusedDevices;
        bool area = true;
        gpu::Device::Api openApi = gpu::Device::Api::None;
        std::vector<std::string> calls;

      private:
        [[nodiscard]] static std::string name(gpu::Device::Api api) {
            return api == gpu::Device::Api::Vulkan ? "vulkan" : api == gpu::Device::Api::OpenGl ? "openGl" : "webGpu";
        }
    };

    GraphicsApiFallbackTest() : listener(core::Log::addListener([this](core::Log::Level level, std::string_view line) { record(level, line); })) {}

    ~GraphicsApiFallbackTest() override {
        core::Log::removeListener(listener);
    }

    GraphicsApiFallbackTest(const GraphicsApiFallbackTest&) = delete;
    GraphicsApiFallbackTest& operator=(const GraphicsApiFallbackTest&) = delete;

    void record(core::Log::Level level, std::string_view line) {
        if (level == core::Log::Level::Warning) {
            warnings.emplace_back(line);
        }
    }

    static constexpr std::string_view kFailure = "No graphics API could start on this machine, so the app cannot open its window.";

    GraphicsApiFallback fallback{{gpu::Device::Api::Vulkan, gpu::Device::Api::OpenGl, gpu::Device::Api::WebGpu}, std::string(kFailure)};
    RecordingWindow window;
    std::vector<std::string> warnings;
    std::uint64_t listener = 0;
};

// An API whose window or device does not start closes the window and gives way to the next API in the order, and the log says why each one failed.
TEST_F(GraphicsApiFallbackTest, FallsBackToTheNextApiWhenAWindowOrADeviceDoesNotStart) {
    window.refusedWindows.insert(gpu::Device::Api::Vulkan);
    window.refusedDevices.insert(gpu::Device::Api::OpenGl);
    const std::unique_ptr<gpu::Device> device = fallback.open(window);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getApi(), gpu::Device::Api::WebGpu);
    EXPECT_EQ(window.calls, (std::vector<std::string>{"prepare vulkan", "open vulkan", "close", "prepare openGl", "open openGl", "create openGl", "close", "prepare webGpu", "open webGpu", "create webGpu"}));
    ASSERT_EQ(warnings.size(), 2U);
    EXPECT_NE(warnings[0].find("The graphics API \"vulkan\" could not start. The window refused this API."), std::string::npos);
    EXPECT_NE(warnings[1].find("The graphics API \"openGl\" could not start. The device refused to start."), std::string::npos);
}

// An API that still prepares, or a window without area, holds the start, and the next call goes on from the same API with the window it opened.
TEST_F(GraphicsApiFallbackTest, WaitsForAnApiThatPreparesAndForAWindowWithAnArea) {
    window.preparing.insert(gpu::Device::Api::Vulkan);
    EXPECT_EQ(fallback.open(window), nullptr);
    EXPECT_EQ(window.calls, (std::vector<std::string>{"prepare vulkan"}));

    window.preparing.clear();
    window.area = false;
    EXPECT_EQ(fallback.open(window), nullptr);
    EXPECT_EQ(window.openApi, gpu::Device::Api::Vulkan);

    window.area = true;
    window.calls.clear();
    const std::unique_ptr<gpu::Device> device = fallback.open(window);
    ASSERT_NE(device, nullptr);
    EXPECT_EQ(device->getApi(), gpu::Device::Api::Vulkan);
    EXPECT_EQ(window.calls, (std::vector<std::string>{"prepare vulkan", "create vulkan"}));
    EXPECT_TRUE(warnings.empty());
}

TEST_F(GraphicsApiFallbackTest, FailsWithItsSentenceOnceNoApiStarts) {
    window.refusedDevices = {gpu::Device::Api::Vulkan, gpu::Device::Api::OpenGl, gpu::Device::Api::WebGpu};
    try {
        (void)fallback.open(window);
        ADD_FAILURE() << "A device started although every API refused.";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(error.what(), kFailure);
    }

    EXPECT_EQ(warnings.size(), 3U);
    EXPECT_EQ(window.openApi, gpu::Device::Api::None);
}

} // namespace haylen::platform
