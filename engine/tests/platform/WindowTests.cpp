#include <gtest/gtest.h>

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class WindowEngineTest : public ::testing::Test {
  protected:
    // The passes of the last frame that reached the swapchain, with the alpha they cleared to and the channels their pipelines wrote.
    [[nodiscard]] static std::vector<gpu::NullDevice::PassRecord> getSwapchainPasses(test::EngineFixture& fixture) {
        std::vector<gpu::NullDevice::PassRecord> passes;
        for (const gpu::NullDevice::PassRecord& pass : fixture.host().getGpu().getLastFrame().passes) {
            if (pass.swapchain) {
                passes.push_back(pass);
            }
        }

        return passes;
    }

    [[nodiscard]] static std::vector<gpu::Device::ColorMask> getMasks(const std::vector<gpu::NullDevice::PassRecord>& passes) {
        std::vector<gpu::Device::ColorMask> masks;
        for (const gpu::NullDevice::PassRecord& pass : passes) {
            masks.insert(masks.end(), pass.masks.begin(), pass.masks.end());
        }

        return masks;
    }
};

TEST_F(WindowEngineTest, AppliesTheWindowOptionsOfHaylenJson) {
    test::EngineFixture fixture({{"haylen.json", R"({"window": {"decorated": false, "alwaysOnTop": true, "showInTaskbar": false, "focusable": false, "mousePassthrough": true, "resizable": false}})"}});
    const HeadlessHost& host = fixture.host();
    EXPECT_FALSE(host.isDecorated());
    EXPECT_TRUE(host.isAlwaysOnTop());
    EXPECT_FALSE(host.isShownInTaskbar());
    EXPECT_FALSE(host.isFocusable());
    EXPECT_FALSE(host.isResizable());
    EXPECT_EQ(host.getMousePassthrough(), Window::Passthrough::Whole);
    EXPECT_TRUE(host.getPassthroughRegions().empty());
}

TEST_F(WindowEngineTest, TakesCallsWhoseAnswerNobodyWants) {
    test::EngineFixture fixture;
    HeadlessHost& host = fixture.host();
    Window& window = host;
    window.setMouseLocked(true, {});
    window.setKeepScreenOn(true, {});
    window.enterFullscreen({}, {});
    EXPECT_TRUE(host.isMouseLocked());
    EXPECT_TRUE(host.isScreenKeptOn());
    EXPECT_TRUE(host.isFullscreen());

    window.setFullscreen(false, {});
    EXPECT_FALSE(host.isFullscreen());
}

TEST_F(WindowEngineTest, OpensAnOrdinaryWindowByDefault) {
    test::EngineFixture fixture;
    const HeadlessHost& host = fixture.host();
    EXPECT_TRUE(host.isDecorated());
    EXPECT_FALSE(host.isAlwaysOnTop());
    EXPECT_TRUE(host.isShownInTaskbar());
    EXPECT_TRUE(host.isFocusable());
    EXPECT_EQ(host.getMousePassthrough(), Window::Passthrough::Off);
}

// The engine paints the color behind the status bar on a canvas of its own, only while the status bar shows over the visible area.
TEST_F(WindowEngineTest, ShowsTheStatusBarAndPaintsItsColor) {
    test::EngineFixture fixture({{"haylen.json", R"({"window": {"statusBar": "light", "statusBarColor": "#102030"}})"}});
    HeadlessHost& host = fixture.host();
    EXPECT_EQ(host.getStatusBar(), Window::StatusBar::Light);
    EXPECT_EQ(fixture.engine().getStatusBarColor(), math::Color::fromHex(0x102030FFU));
    fixture.frames(1);
    const std::size_t canvases = fixture.engine().getRenderer2D().getStats().canvases;

    host.setStatusBarHeight(90.0F);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().canvases, canvases + 1);

    host.setStatusBar(Window::StatusBar::Hidden);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().canvases, canvases);

    host.setStatusBar(Window::StatusBar::Dark);
    fixture.engine().setStatusBarColor(std::nullopt);
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().canvases, canvases);
}

TEST_F(WindowEngineTest, TurnsTransparencyOffAndOnAgain) {
    test::EngineFixture fixture({{"haylen.json", R"({"window": {"transparent": true}})"}});
    EXPECT_TRUE(fixture.host().canBeTransparent());
    EXPECT_TRUE(fixture.host().isTransparent());
    EXPECT_TRUE(fixture.host().isFrameTransparent());

    fixture.host().setTransparent(false);
    EXPECT_FALSE(fixture.host().isTransparent());
    EXPECT_FALSE(fixture.host().isFrameTransparent());
    fixture.host().setTransparent(true);
    EXPECT_TRUE(fixture.host().isFrameTransparent());
}

TEST_F(WindowEngineTest, KeepsAWindowThatOpenedOpaqueOpaque) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.host().canBeTransparent());
    EXPECT_FALSE(fixture.host().isTransparent());
    EXPECT_THROW(fixture.host().setTransparent(true), std::logic_error);
    EXPECT_NO_THROW(fixture.host().setTransparent(false));
}

// Whatever the clear color and the blends, an opaque window clears to alpha 1 and draws no alpha, while a transparent one clears to the clear color and blends its alpha.
TEST_F(WindowEngineTest, KeepsTheAlphaOfOpaqueWindowsAtOne) {
    test::EngineFixture fixture({
        {"haylen.json", R"({"window": {"transparent": true}, "clearColor": "#00000000"})"},
        {"source/main.lua", "local graphics2d = require('haylen.graphics2d') local camera = graphics2d.newCamera() "
                            "require('haylen.scene').push({render = function() "
                            "graphics2d.beginWorld(camera, {ambientLight = '#FF808080', postProcess = {fade = '#40FFFFFF'}}) graphics2d.drawRect({0, 0, 100, 100}, '#80FF0000') "
                            "graphics2d.beginScreen() graphics2d.drawRect({0, 0, 50, 50}, '#40FFFFFF', {blend = 'additive'}) graphics2d.drawText(nil, 'Gold', 10, 10) end})"},
    });
    fixture.frames(2);
    std::vector<gpu::NullDevice::PassRecord> passes = getSwapchainPasses(fixture);
    ASSERT_FALSE(passes.empty());
    EXPECT_EQ(passes.front().clear.a, 0.0F);
    std::vector<gpu::Device::ColorMask> masks = getMasks(passes);
    ASSERT_FALSE(masks.empty());

    for (const gpu::Device::ColorMask mask : masks) {
        EXPECT_EQ(mask, gpu::Device::ColorMask::All);
    }

    fixture.host().setTransparent(false);
    fixture.frames(2);
    passes = getSwapchainPasses(fixture);
    ASSERT_FALSE(passes.empty());
    EXPECT_EQ(passes.front().clear.a, 1.0F);
    masks = getMasks(passes);
    ASSERT_GE(masks.size(), 3U);

    for (const gpu::Device::ColorMask mask : masks) {
        EXPECT_EQ(mask, gpu::Device::ColorMask::Rgb);
    }
}

TEST_F(WindowEngineTest, PublishesMoves) {
    test::EngineFixture fixture;
    fixture.runLua("events = require('haylen.events') heard = {} "
                   "events.on('windowMoved', function(info) heard[#heard + 1] = 'moved ' .. info.x .. ' ' .. info.y end)");

    // Platforms report every step of a move, and the app hears each new position once.
    fixture.host().setFrame({120.0F, 80.0F, 1920.0F, 1080.0F});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.host().setFrame({120.0F, 80.0F, 640.0F, 480.0F});
    fixture.engine().handleEvent({.type = Event::Type::WindowMoved});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "moved 120.0 80.0");
}

} // namespace haylen::platform
