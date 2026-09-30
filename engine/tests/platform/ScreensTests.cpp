#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Screens.hpp"
#include "haylen/platform/native/HaylenNative.h"
#include "platform/ScreenRelay.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "platform/native/NativeApi.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// The screen that shows belongs to the process, so every test ends the one it left open.
class ScreensTest : public ::testing::Test {
  protected:
    // The screens that a native library opened, by the JSON of their parameters.
    struct LibraryScreens {
        std::vector<std::string> opened;

        static void open(void* user, std::uint64_t, const char* paramsJson, const HaylenNativeBuffer*, std::size_t) {
            static_cast<LibraryScreens*>(user)->opened.emplace_back(paramsJson);
        }
    };

    void TearDown() override {
        if (const std::optional<ScreenRelay::Screen> showing = ScreenRelay::getShowing()) {
            ScreenRelay::finish(showing->id, false, "null");
        }
        static_cast<void>(ScreenRelay::take());
    }

    // An app with the plugin shop, whose Lua module opens its paywall and keeps what the screen answered and what the listener of screenRestored heard.
    static std::map<std::string, std::string> shopFiles() {
        // clang-format off
        return {
            {"app.json", R"({"name": "Test App", "identifier": "dev.haylen.tests", "plugins": {"shop": {}}})"},
            {"plugins/shop/plugin.json", R"({"id": "shop", "version": "1.0.0"})"},
            {"source/main.lua", R"(
                async = require('async')
                haylen = require('haylen')
                platform = require('haylen.platform')
                shop = platform.plugin('shop')
                function open(params, options)
                    answer, failure = nil, nil
                    call = shop:openScreen('paywall', params, options)
                    async.spawn(function() answer, failure = call:await() end)
                    return call
                end
            )"},
        };
        // clang-format on
    }
};

TEST_F(ScreensTest, OpensTheScreenOnceTheCoverHoldsAndAnswersWithItsResult) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    Screens& screens = engine.getScreens();
    std::optional<Bridge::Result> answer;
    const std::uint64_t id = screens.open("shop", "paywall", {.json = {{"offer", "gold"}}}, {.state = {{"level", 3}}}, [&answer](Bridge::Result result) { answer = std::move(result); });
    EXPECT_TRUE(screens.isShowing());
    EXPECT_EQ(screens.getPendingCount(), 1U);

    // The platform receives the screen at the start of the next frame, when the app is covered already.
    EXPECT_TRUE(fixture.host().getScreenRequests().empty());
    EXPECT_FALSE(engine.isAppCovered());
    fixture.frames(1);
    EXPECT_TRUE(engine.isAppCovered());
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    EXPECT_TRUE(engine.isHalted());
    EXPECT_TRUE(engine.getAudio().isBusMuted("master"));
    ASSERT_EQ(fixture.host().getScreenRequests().size(), 1U);
    const ScreenRequest& request = fixture.host().getScreenRequests()[0];
    EXPECT_EQ(request.id, id);
    EXPECT_EQ(request.plugin, "shop");
    EXPECT_EQ(request.screen, "paywall");
    EXPECT_EQ(request.params.json.dump(), R"({"offer":"gold"})");
    EXPECT_EQ(request.state.dump(), R"({"level":3})");
    EXPECT_TRUE(request.opaque);

    // The end of the screen uncovers the app and answers the call in the same frame, and a second end changes nothing.
    fixture.frames(2);
    EXPECT_FALSE(answer);
    ScreenRelay::finish(id, true, R"({"bought": true, "receipt": {"$bytes": 0}})", {{std::byte{7}}});
    ScreenRelay::finish(id, false, R"("again")");
    fixture.frames(1);
    ASSERT_TRUE(answer);
    EXPECT_TRUE(answer->ok);
    EXPECT_EQ(answer->value.json.dump(), R"({"bought":true,"receipt":{"$bytes":0}})");
    EXPECT_EQ(answer->value.buffers, (std::vector<std::vector<std::byte>>{{std::byte{7}}}));
    EXPECT_FALSE(engine.isAppCovered());
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
    EXPECT_FALSE(engine.getAudio().isBusMuted("master"));
    EXPECT_FALSE(screens.isShowing());
    EXPECT_EQ(screens.getPendingCount(), 0U);

    // Failures of the platform reach the call with their code.
    answer.reset();
    const std::uint64_t closed = screens.open("shop", "paywall", {}, {}, [&answer](Bridge::Result result) { answer = std::move(result); });
    fixture.frames(1);
    ScreenRelay::finish(closed, false, R"({"message": "The user closed the paywall.", "code": "cancelled"})");
    fixture.frames(1);
    ASSERT_TRUE(answer);
    EXPECT_EQ(answer->error.message, "The user closed the paywall.");
    EXPECT_EQ(answer->error.code, "cancelled");

    // A screen that nobody waits for ends all the same, and so does one that could not open.
    const std::uint64_t unheard = screens.open("shop", "paywall", {}, {}, nullptr);
    screens.open("shop", "offer", {}, {}, nullptr);
    fixture.frames(1);
    ScreenRelay::finish(unheard, true, "null");
    fixture.frames(1);
    EXPECT_FALSE(screens.isShowing());
    EXPECT_EQ(screens.getPendingCount(), 0U);
}

TEST_F(ScreensTest, DrawsNothingUnderAnOpaqueScreen) {
    test::EngineFixture fixture({{"source/main.lua", "renders = 0 require('haylen.scene').push({render = function() renders = renders + 1 end})"}});
    Screens& screens = fixture.engine().getScreens();
    const auto rendered = [&fixture] { return std::stoi(fixture.lua("return renders")); };
    fixture.frames(5);
    int before = rendered();
    EXPECT_GT(before, 0);

    // A halted app keeps drawing under a screen that lets it show through, and draws nothing under an opaque one.
    const std::uint64_t translucent = screens.open("shop", "offer", {}, {.opaque = false}, [](const Bridge::Result&) {});
    fixture.frames(3);
    EXPECT_TRUE(fixture.engine().isHalted());
    EXPECT_EQ(rendered() - before, 3);
    ScreenRelay::finish(translucent, true, "null");
    fixture.frames(1);

    before = rendered();
    const std::uint64_t opaque = screens.open("shop", "paywall", {}, {}, [](const Bridge::Result&) {});
    fixture.frames(3);
    EXPECT_EQ(rendered() - before, 0);
    ScreenRelay::finish(opaque, true, "null");
    fixture.frames(1);
    EXPECT_EQ(rendered() - before, 1);
}

TEST_F(ScreensTest, OpensOneScreenAtATimeAndOnlyWhileTheAppIsActive) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    Screens& screens = engine.getScreens();
    std::vector<std::string> codes;
    const auto record = [&codes](const Bridge::Result& result) { codes.push_back(result.ok ? "ok" : result.error.code.get<std::string>()); };

    // A second screen fails while the first shows, whichever app opened it, and never reaches the platform.
    const std::uint64_t first = screens.open("shop", "paywall", {}, {}, record);
    screens.open("shop", "offer", {}, {}, record);
    EXPECT_TRUE(codes.empty()) << "A failure arrives at the next pump, like every answer.";
    fixture.frames(1);
    screens.open("shop", "offer", {}, {}, record);
    fixture.frames(1);
    EXPECT_EQ(codes, (std::vector<std::string>{"busy", "busy"}));
    EXPECT_EQ(fixture.host().getScreenRequests().size(), 1U);
    ScreenRelay::finish(first, true, "null");
    fixture.frames(1);

    // An app without the focus, in the background or covered by other native UI opens no screen.
    engine.handleEvent({.type = Event::Type::FocusLost});
    screens.open("shop", "paywall", {}, {}, record);
    engine.handleEvent({.type = Event::Type::FocusGained});
    engine.handleEvent({.type = Event::Type::Suspended});
    screens.open("shop", "paywall", {}, {}, record);
    engine.handleEvent({.type = Event::Type::Resumed});
    fixture.host().getNativeViews().coverApp();
    fixture.frames(1);
    screens.open("shop", "paywall", {}, {}, record);
    fixture.host().getNativeViews().uncoverApp();
    fixture.frames(1);
    EXPECT_EQ(codes, (std::vector<std::string>{"busy", "busy", "ok", "notActive", "notActive", "notActive"}));
    EXPECT_FALSE(screens.isShowing());
    EXPECT_EQ(fixture.host().getScreenRequests().size(), 1U);

    // Screens need a plugin, a name, parameters whose bytes came with them and a positive timeout.
    EXPECT_THROW(screens.open("", "paywall", {}, {}, record), std::invalid_argument);
    EXPECT_THROW(screens.open("shop", "", {}, {}, record), std::invalid_argument);
    EXPECT_THROW(screens.open("shop", "paywall", {.json = {{"image", {{"$bytes", 0}}}}}, {}, record), std::invalid_argument);
    EXPECT_THROW(screens.open("shop", "paywall", {}, {.timeout = std::chrono::seconds(0)}, record), std::invalid_argument);
    EXPECT_FALSE(screens.isShowing());
}

TEST_F(ScreensTest, KeepsTheAppCoveredUntilACancelledScreenIsGone) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    Screens& screens = engine.getScreens();
    std::vector<std::string> codes;
    const auto record = [&codes](const Bridge::Result& result) { codes.push_back(result.ok ? "ok" : result.error.code.get<std::string>()); };

    // The call fails at once, while the platform dismisses the screen and the cover lasts until it reports the screen gone.
    const std::uint64_t id = screens.open("shop", "paywall", {}, {}, record);
    fixture.frames(1);
    EXPECT_TRUE(screens.cancel(id));
    EXPECT_FALSE(screens.cancel(id));
    EXPECT_EQ(fixture.host().getCancelledScreens(), (std::vector<std::uint64_t>{id}));
    fixture.frames(1);
    EXPECT_EQ(codes, (std::vector<std::string>{"cancelled"}));
    EXPECT_TRUE(engine.isAppCovered());
    std::vector<std::string> restored;
    engine.getPlatform().on("shop.screenRestored", [&restored](const Bridge::Payload& payload) { restored.push_back(payload.json.dump()); });
    ScreenRelay::finish(id, false, R"({"message": "Dismissed.", "code": "cancelled"})");
    fixture.frames(1);
    EXPECT_FALSE(engine.isAppCovered());
    EXPECT_TRUE(restored.empty()) << "The app gave the screen up, so its end reaches nobody.";

    // A screen cancelled before it reached the platform never reaches it.
    const std::uint64_t early = screens.open("shop", "paywall", {}, {}, record);
    EXPECT_TRUE(screens.cancel(early));
    EXPECT_FALSE(screens.isShowing());
    fixture.frames(1);
    EXPECT_EQ(fixture.host().getScreenRequests().size(), 1U);
    EXPECT_FALSE(engine.isAppCovered());
    EXPECT_EQ(codes, (std::vector<std::string>{"cancelled", "cancelled"}));
}

TEST_F(ScreensTest, GivesUpAScreenWhoseTimeoutPassed) {
    test::EngineFixture fixture;
    Screens& screens = fixture.engine().getScreens();
    std::optional<Bridge::Result> answer;
    const std::uint64_t id = screens.open("shop", "paywall", {}, {.timeout = std::chrono::milliseconds(20)}, [&answer](Bridge::Result result) { answer = std::move(result); });
    ASSERT_TRUE(fixture.frameUntil([&answer] { return answer.has_value(); }));
    EXPECT_EQ(answer->error.code, "timeout");
    EXPECT_EQ(answer->error.message, "The screen \"paywall\" of \"shop\" timed out.");
    EXPECT_EQ(fixture.host().getCancelledScreens(), (std::vector<std::uint64_t>{id}));
    EXPECT_TRUE(screens.isShowing());
    ScreenRelay::finish(id, false, R"({"message": "Dismissed.", "code": "cancelled"})");
    fixture.frames(1);
    EXPECT_FALSE(screens.isShowing());
}

TEST_F(ScreensTest, HandsTheEndToTheNextAppWithTheStateAfterARestart) {
    test::EngineFixture fixture(shopFiles());
    fixture.runLua("open({offer = 'gold'}, {state = {level = 3, deck = {'a', 'b'}}})");
    fixture.frames(1);
    ASSERT_EQ(fixture.host().getScreenRequests().size(), 1U);
    const std::uint64_t id = fixture.host().getScreenRequests()[0].id;

    // The screen outlives the app, so the restarted app is covered from its first frame and opens no other screen.
    fixture.restart();
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return tostring(haylen.appCovered()) .. ' ' .. tostring(platform.screenShowing())"), "true true");
    fixture.runLua("open()");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return failure ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return failure.code"), "busy");

    // The end waits for the first listener of screenRestored, however late it connects, with the state the earlier app gave.
    ScreenRelay::finish(id, true, R"({"bought": true, "receipt": {"$bytes": 0}})", {{std::byte{'o'}, std::byte{'k'}}});
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return tostring(haylen.appCovered()) .. ' ' .. tostring(platform.screenShowing())"), "false false");
    fixture.runLua("restored = {} shop:on('screenRestored', function(ending) restored[#restored + 1] = ending end)");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("local ending = restored[1] return table.concat({#restored, ending.screen, tostring(ending.result.bought), ending.result.receipt, ending.state.level, ending.state.deck[2], tostring(ending.error)}, ' ')"), "1 paywall true ok 3 b nil");

    // A screen that the platform kept across the end of the process ends the same way, with its failure as the error.
    ScreenRelay::restore("shop", "paywall", R"({"level": 4})", false, R"({"message": "The user closed the paywall.", "code": "cancelled"})");
    ScreenRelay::restore("shop", "", "null", true, "null");
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("local ending = restored[2] return table.concat({#restored, ending.screen, ending.state.level, ending.error.code, ending.error.message, tostring(ending.result)}, ' ')"), "2 paywall 4 cancelled The user closed the paywall. nil");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST_F(ScreensTest, OpensScreensFromLuaWithOptions) {
    test::EngineFixture fixture(shopFiles());
    fixture.runLua("first = open({offer = 'gold', image = platform.bytes('\\1\\2')}, {state = {level = 3}, opaque = false, timeout = 30})");
    EXPECT_EQ(fixture.lua("return tostring(platform.screenShowing()) .. ' ' .. tostring(first.done) .. ' ' .. tostring(first.id > 0)"), "true false true");
    fixture.frames(1);
    ASSERT_EQ(fixture.host().getScreenRequests().size(), 1U);
    const ScreenRequest& request = fixture.host().getScreenRequests()[0];
    EXPECT_EQ(request.params.json.dump(), R"({"image":{"$bytes":0},"offer":"gold"})");
    EXPECT_EQ(request.params.buffers, (std::vector<std::vector<std::byte>>{{std::byte{1}, std::byte{2}}}));
    EXPECT_EQ(request.state.dump(), R"({"level":3})");
    EXPECT_FALSE(request.opaque);

    // The result settles the call, whose await returns it, with the bytes of the platform as a string.
    ScreenRelay::finish(request.id, true, R"({"bought": true, "receipt": {"$bytes": 0}})", {{std::byte{'o'}, std::byte{'k'}}});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return answer ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return table.concat({tostring(first.done), tostring(answer.bought), answer.receipt, tostring(failure)}, ' ')"), "true true ok nil");

    // Cancelling fails the call with the code cancelled and dismisses the screen.
    fixture.runLua("second = open() cancelled = second:cancel()");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return failure ~= nil") == "true"; }));
    EXPECT_EQ(fixture.lua("return table.concat({tostring(cancelled), tostring(second:cancel()), failure.code, failure.message}, ' ')"), "true false cancelled The screen \"paywall\" of \"shop\" was cancelled.");
    EXPECT_TRUE(fixture.host().getCancelledScreens().empty()) << "The screen never reached the platform.";

    // Options are checked like the options of every call.
    EXPECT_NE(fixture.lua("return shop:openScreen('')").find("A screen of the plugin \"shop\" needs a name."), std::string::npos);
    EXPECT_NE(fixture.lua("return shop:openScreen('paywall', nil, {modal = true})").find("modal"), std::string::npos);
    EXPECT_NE(fixture.lua("return shop:openScreen('paywall', nil, {timeout = 0})").find("The timeout of a screen is a positive number of seconds."), std::string::npos);
    EXPECT_NE(fixture.lua("return shop:openScreen('paywall', nil, {state = {image = platform.bytes('x')}})").find("The state of a screen is JSON without bytes."), std::string::npos);
    EXPECT_FALSE(fixture.engine().getScreens().isShowing());
}

TEST_F(ScreensTest, OpensTheScreensThatNativeLibrariesRegister) {
    test::EngineFixture fixture;
    Screens& screens = fixture.engine().getScreens();
    const HaylenNativeApi& api = NativeApi::get();
    LibraryScreens library;
    api.registerScreen("", "paywall", &LibraryScreens::open, nullptr, &library);
    api.registerScreen("shop", "paywall", &LibraryScreens::open, nullptr, &library);

    // The library opens the screen before the platform is asked, and a screen without a cancel function keeps showing until the library ends it.
    std::optional<Bridge::Result> answer;
    const std::uint64_t id = screens.open("shop", "paywall", {.json = {{"offer", "gold"}}}, {}, [&answer](Bridge::Result result) { answer = std::move(result); });
    fixture.frames(1);
    EXPECT_EQ(library.opened, (std::vector<std::string>{R"({"offer":"gold"})"}));
    EXPECT_TRUE(fixture.host().getScreenRequests().empty());
    EXPECT_TRUE(screens.cancel(id));
    fixture.frames(1);
    EXPECT_TRUE(screens.isShowing());
    api.finishScreen(id, 0, nullptr, nullptr, 0);
    fixture.frames(1);
    EXPECT_FALSE(screens.isShowing());
    EXPECT_EQ(answer->error.code, "cancelled");

    // A null opener removes the screen, which then goes to the platform.
    api.registerScreen("shop", "paywall", nullptr, nullptr, nullptr);
    const std::uint64_t platformScreen = screens.open("shop", "paywall", {}, {}, nullptr);
    fixture.frames(1);
    EXPECT_EQ(library.opened.size(), 1U);
    ASSERT_EQ(fixture.host().getScreenRequests().size(), 1U);
    ScreenRelay::finish(platformScreen, true, "null");

    // Covers of libraries nest, an uncover without a cover changes nothing, and a window reaches no null target.
    api.uncoverApp();
    api.coverApp();
    fixture.frames(1);
    EXPECT_TRUE(fixture.engine().isAppCovered());
    api.uncoverApp();
    fixture.frames(1);
    EXPECT_FALSE(fixture.engine().isAppCovered());
    EXPECT_EQ(api.getWindow(nullptr), 0);
}

TEST_F(ScreensTest, RestartsTheAppFromLua) {
    test::EngineFixture fixture;
    EXPECT_FALSE(fixture.engine().isRestartRequested());
    fixture.runLua("require('haylen').requestRestart()");
    EXPECT_TRUE(fixture.engine().isRestartRequested());
}

TEST_F(ScreensTest, TakesTheEndsOfScreensFromOtherThreads) {
    test::EngineFixture fixture;
    Screens& screens = fixture.engine().getScreens();
    int answers = 0;
    for (int round = 0; round < 20; ++round) {
        const std::uint64_t id = screens.open("shop", "paywall", {}, {}, [&answers](const Bridge::Result& result) { answers += result.ok ? 1 : 0; });
        fixture.frames(1);
        std::atomic<bool> done = false;
        // clang-format off
        std::thread native([id, &done] {
            ScreenRelay::finish(id, true, R"({"round": true})");
            ScreenRelay::restore("shop", "paywall", "null", true, "null");
            done.store(true);
        });
        // clang-format on
        while (!done.load()) {
            fixture.frames(1);
        }
        native.join();
        fixture.frames(1);
    }
    EXPECT_EQ(answers, 20);
    EXPECT_FALSE(screens.isShowing());
    EXPECT_FALSE(fixture.engine().isAppCovered());
}

} // namespace haylen::platform
