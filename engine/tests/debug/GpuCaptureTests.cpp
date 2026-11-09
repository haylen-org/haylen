#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/debug/Console.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/DevelopmentSession.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::debug {

class GpuCaptureTest : public ::testing::Test {
  protected:
    // An engine whose GPU device reports RenderDoc as its capture tool, which records the frames of each capture without writing a file.
    [[nodiscard]] test::EngineFixture::Options withCaptureTool(bool development = false) {
        // clang-format off
        return {.development = development, .gpuDevice = [this] {
            auto made = std::make_unique<gpu::NullDevice>(gpu::Device::Language::None, gpu::NullDevice::getDefaultCapabilities(), gpu::Device::Api::None, gpu::Device::CaptureTool::RenderDoc);
            device = made.get();
            return made;
        }};
        // clang-format on
    }

    gpu::NullDevice* device = nullptr;
};

TEST_F(GpuCaptureTest, ResolvesWithTheFileOfTheFramesItCaptured) {
    test::EngineFixture fixture({}, nullptr, withCaptureTool());
    EXPECT_EQ(fixture.lua("return require('haylen.debug').gpuCaptureTool()"), "renderDoc");

    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        require('async').spawn(function()
            captured = debugging.captureGpuAsync({frames = 3}):await()
        end)
        require('async').spawn(function()
            local _, message, failure = debugging.captureGpuAsync():await()
            busy = failure.code .. ' ' .. message
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(captured ~= nil)") == "true"; }));
    EXPECT_EQ(fixture.lua("return busy"), "busy A GPU capture is already recording. Wait for it to end before capturing again.");
    EXPECT_EQ(fixture.lua("return captured.tool .. ' ' .. captured.frames"), "renderDoc 3");
    const std::filesystem::path path = std::filesystem::path(fixture.lua("return captured.path"));
    EXPECT_EQ(path.parent_path(), fixture.engine().getStorage().getRoot() / plugins::DebugPlugin::kGpuCaptureFolder);
    EXPECT_EQ(path.extension(), ".capture");
    EXPECT_TRUE(std::filesystem::is_directory(path.parent_path()));
    EXPECT_EQ(device->getCapturedFrames(), std::vector<std::uint64_t>{3});

    EXPECT_NE(fixture.lua("debugging.captureGpuAsync({frames = 0})").find("A GPU capture records at least 1 frame."), std::string::npos);
    EXPECT_NE(fixture.lua("debugging.captureGpuAsync({seconds = 2})").find("Unknown option \"seconds\"."), std::string::npos);
}

TEST_F(GpuCaptureTest, RejectsAsUnsupportedWithWhatTheDeviceLacks) {
    test::EngineFixture fixture;
    EXPECT_EQ(fixture.lua("return require('haylen.debug').gpuCaptureTool()"), "none");

    // clang-format off
    fixture.runLua(R"(
        require('async').spawn(function()
            local _, message, failure = require('haylen.debug').captureGpuAsync():await()
            refused = failure.code .. ' ' .. message
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(refused ~= nil)") == "true"; }));
    EXPECT_EQ(fixture.lua("return refused"), "unsupported " + std::string(gpu::NullDevice::kMissingCaptureTool));
    EXPECT_FALSE(fixture.engine().getGraphics().isCapturing());
}

TEST_F(GpuCaptureTest, TheConsoleCapturesAndLogsTheFileOnceTheFramesWereSubmitted) {
    test::EngineFixture fixture({}, nullptr, withCaptureTool());
    plugins::DebugPlugin& plugin = fixture.engine().getPlugin<plugins::DebugPlugin>();
    Console& console = plugin.getConsole();

    const Console::Result started = console.run("gpuCapture 2");
    EXPECT_TRUE(started.ok);
    EXPECT_EQ(started.output, "Capturing the next 2 frames with \"renderDoc\".");
    EXPECT_EQ(console.run("gpuCapture").output, "A GPU capture is already recording. Wait for it to end before capturing again.");
    fixture.frames(2);
    EXPECT_FALSE(fixture.engine().getGraphics().isCapturing());
    EXPECT_NE(plugin.getRecentLog().back().text.find("The GPU capture of 2 frames is in \""), std::string::npos);
    EXPECT_EQ(device->getCapturedFrames(), std::vector<std::uint64_t>{2});
}

TEST_F(GpuCaptureTest, TheConsoleSaysWhatADeviceWithoutACaptureToolLacks) {
    test::EngineFixture fixture;
    const Console::Result refused = fixture.engine().getPlugin<plugins::DebugPlugin>().getConsole().run("gpuCapture");
    EXPECT_FALSE(refused.ok);
    EXPECT_EQ(refused.output, gpu::NullDevice::kMissingCaptureTool);
}

TEST_F(GpuCaptureTest, ADevelopmentRunReportsTheCaptureToTheDevelopmentServer) {
    test::EngineFixture fixture({}, nullptr, withCaptureTool(true));
    std::vector<core::Json> reports;
    fixture.host().getDevelopmentSession()->addReporter([&reports](const core::Json& report) { reports.push_back(report); });

    ASSERT_TRUE(fixture.engine().getPlugin<plugins::DebugPlugin>().getConsole().run("gpuCapture").ok);
    fixture.frames(1);
    ASSERT_EQ(reports.size(), 1U);
    const core::Json& capture = reports.front().at("gpuCapture");
    EXPECT_TRUE(capture.at("ok").get<bool>());
    EXPECT_EQ(capture.at("tool"), "renderDoc");
    EXPECT_EQ(capture.at("frames"), 1);
    EXPECT_TRUE(capture.at("path").get<std::string>().ends_with(".capture"));
}

} // namespace haylen::debug
