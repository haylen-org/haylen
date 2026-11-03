#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/platform/Permission.hpp"
#include "haylen/platform/System.hpp"
#include "platform/SystemState.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class SystemTest : public ::testing::Test {
  protected:
    // Records the theme and battery events of the engine with their data.
    static void recordChanges(core::Engine& engine, std::vector<std::string>& log) {
        for (const std::string_view name : {core::LifecycleEvent::kSystemThemeChanged, core::LifecycleEvent::kBatteryChanged}) {
            engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
        }
    }

    // An iPad that tells everything about itself.
    [[nodiscard]] static SystemInfo makeTablet() {
        return {.os = SystemInfo::Os::IpadOs, .osVersion = "17.5", .deviceModel = "iPad14,1", .manufacturer = "Apple", .deviceKind = SystemInfo::DeviceKind::Tablet, .cpuName = "Apple M2", .cpuCores = 8, .architecture = SystemInfo::Architecture::Arm, .bits = 32, .memoryBytes = 8589934592, .gpuName = "Reported by the platform", .locale = "pt-BR", .languages = {"pt-BR", "en"}, .timeZone = "America/Sao_Paulo"};
    }
};

TEST_F(SystemTest, ReportsThePlatformWithTheGpuOfTheGraphicsDevice) {
    test::EngineFixture fixture;
    const SystemInfo& info = fixture.engine().getSystem().getInfo();
#if defined(__APPLE__)
    EXPECT_EQ(info.os, SystemInfo::Os::MacOs);
#elif defined(_WIN32)
    EXPECT_EQ(info.os, SystemInfo::Os::Windows);
#else
    EXPECT_EQ(info.os, SystemInfo::Os::Linux);
#endif
    EXPECT_EQ(info.deviceKind, SystemInfo::DeviceKind::Desktop);
    EXPECT_EQ(info.cpuCores, static_cast<int>(std::thread::hardware_concurrency()));

    // The dummy backend of the headless host names no adapter, and the GPU is always the one of the graphics device.
    EXPECT_EQ(fixture.engine().getGraphics().getAdapterName(), "");
    fixture.host().setSystemInfo(makeTablet());
    fixture.restart();
    const SystemInfo& tablet = fixture.engine().getSystem().getInfo();
    EXPECT_EQ(tablet.deviceModel, "iPad14,1");
    EXPECT_EQ(tablet.gpuName, "");
    EXPECT_EQ(tablet.languages, (std::vector<std::string>{"pt-BR", "en"}));
}

TEST_F(SystemTest, WritesOnlyWhatThePlatformReportsAsJson) {
    // The architecture and the width of pointers are those of the build that runs, which every platform reports without asking the system.
    const std::string_view architecture = SystemInfo::architectureName(SystemInfo::processArchitecture());
    const int bits = static_cast<int>(sizeof(void*) * 8);
    EXPECT_EQ(SystemInfo{}.toJson(), (core::Json{{"os", "macOs"}, {"deviceKind", "desktop"}, {"architecture", architecture}, {"bits", bits}, {"languages", core::Json::array()}}));

    SystemInfo tablet = makeTablet();
    EXPECT_EQ(tablet.toJson(), (core::Json{{"os", "ipadOs"}, {"osVersion", "17.5"}, {"deviceModel", "iPad14,1"}, {"manufacturer", "Apple"}, {"deviceKind", "tablet"}, {"cpuName", "Apple M2"}, {"cpuCores", 8}, {"architecture", "arm"}, {"bits", 32}, {"memoryBytes", 8589934592}, {"gpuName", "Reported by the platform"}, {"locale", "pt-BR"}, {"languages", {"pt-BR", "en"}}, {"timeZone", "America/Sao_Paulo"}}));
    EXPECT_EQ(SystemInfo::osName(SystemInfo::Os::TvOs), "tvOs");
    EXPECT_EQ(SystemInfo::deviceKindName(SystemInfo::DeviceKind::Tv), "tv");
    EXPECT_EQ(SystemInfo::architectureName(SystemInfo::Architecture::X64), "x64");

    EXPECT_EQ(Battery{}.toJson(), (core::Json{{"charging", false}, {"state", "unknown"}}));
    EXPECT_EQ((Battery{.level = 0.6F, .charging = true, .state = Battery::State::Charging}.toJson()), (core::Json{{"level", 0.6}, {"charging", true}, {"state", "charging"}}));
    EXPECT_EQ(Battery::stateName(Battery::State::None), "none");
}

TEST_F(SystemTest, PublishesEveryChangeOfThemeAndBatteryOnce) {
    test::EngineFixture fixture;
    std::vector<std::string> log;
    recordChanges(fixture.engine(), log);
    System& system = fixture.engine().getSystem();
    EXPECT_EQ(system.getTheme(), Theme::Light);
    EXPECT_EQ(system.getBattery(), Battery{});

    // Platform services report from their own threads, and the engine hears the last report at its next frame.
    // clang-format off
    std::thread([&fixture] {
        fixture.host().setTheme(Theme::Dark);
        fixture.host().setBattery({.level = 0.8F, .state = Battery::State::Discharging});
        fixture.host().setBattery({.level = 0.75F, .state = Battery::State::Discharging});
    }).join();
    // clang-format on
    EXPECT_EQ(system.getTheme(), Theme::Light);
    fixture.frames(1);
    fixture.host().setTheme(Theme::Dark);
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"(systemThemeChanged {"theme":"dark"})", R"(batteryChanged {"charging":false,"level":0.75,"state":"discharging"})"}));
    EXPECT_EQ(system.getTheme(), Theme::Dark);
    EXPECT_EQ(system.getBattery().level, 0.75F);

    // A new app starts with what the platform reported last, without an event.
    fixture.host().setBattery({.charging = true, .state = Battery::State::Charging});
    fixture.restart();
    std::vector<std::string> restarted;
    recordChanges(fixture.engine(), restarted);
    fixture.frames(1);
    EXPECT_TRUE(restarted.empty());
    EXPECT_EQ(fixture.engine().getSystem().getBattery(), (Battery{.charging = true, .state = Battery::State::Charging}));
}

TEST_F(SystemTest, KeepsTheLastReportOfEveryThread) {
    SystemState state;
    std::vector<std::thread> reporters;
    for (int index = 0; index < 4; ++index) {
        // clang-format off
        reporters.emplace_back([&state, index] {
            for (int step = 0; step < 100; ++step) {
                state.setTheme(step % 2 == 0 ? Theme::Dark : Theme::Light);
                state.setBattery({.level = static_cast<float>(index) / 4.0F});
            }
        });
        // clang-format on
    }
    for (std::thread& reporter : reporters) {
        reporter.join();
    }
    EXPECT_EQ(state.getTheme(), Theme::Light);
    EXPECT_TRUE(state.getBattery().level.has_value());
}

TEST_F(SystemTest, OpensUrlsWithAnAnswerOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    std::vector<bool> answers;
    system.openUrl("https://example.com", [&answers](bool opened) { answers.push_back(opened); });
    fixture.host().setOpensUrls(false);
    system.openUrl("mailto:island@example.com", [&answers](bool opened) { answers.push_back(opened); });
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<bool>{true, false}));
    EXPECT_EQ(fixture.host().getOpenedUrls(), (std::vector<std::string>{"https://example.com", "mailto:island@example.com"}));
    EXPECT_THROW(system.openUrl("", [](bool) {}), std::invalid_argument);
}

TEST_F(SystemTest, WritesTheClipboardWithAnAnswerOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    std::vector<bool> answers;
    system.setClipboard("Island seed: 184467", [&answers](bool written) { answers.push_back(written); });
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");

    // A refused write leaves the clipboard as it was, the way a browser refuses a page without a recent input of the person.
    fixture.host().setWritesClipboard(false);
    system.setClipboard("Refused", [&answers](bool written) { answers.push_back(written); });
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<bool>{true, false}));
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");
}

TEST_F(SystemTest, AsksForPermissionsWithAnAnswerOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    EXPECT_EQ(system.getPermission(Permission::Kind::LocalNetwork), Permission::State::Granted);
    fixture.host().setPermission(Permission::State::Prompt);
    EXPECT_EQ(system.getPermission(Permission::Kind::LocalNetwork), Permission::State::Prompt);

    std::vector<Permission::Answer> answers;
    fixture.host().setPermissionAnswer({.state = Permission::State::Denied});
    system.requestPermission(Permission::Kind::LocalNetwork, [&answers](Permission::Answer answer) { answers.push_back(std::move(answer)); });
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers.front().state, Permission::State::Denied);
    EXPECT_EQ(fixture.host().getPermissionRequests(), (std::vector<Permission::Kind>{Permission::Kind::LocalNetwork}));
}

// Native code answers with JSON, whose broken parts read as missing so the request fails instead of the engine.
TEST_F(SystemTest, ReadsTheAnswersOfNativeCode) {
    const Permission::Answer answer = Permission::Answer::fromJson(core::Json::parse(R"({"state": "denied", "message": "The engine needs a line.", "missing": [{"kind": "permission", "name": "android.permission.ACCESS_LOCAL_NETWORK", "file": "app/src/main/AndroidManifest.xml", "snippet": "<uses-permission />"}, 3]})"));
    EXPECT_EQ(answer.state, Permission::State::Denied);
    EXPECT_EQ(answer.message, "The engine needs a line.");
    ASSERT_EQ(answer.missing.size(), 1U);
    EXPECT_EQ(answer.missing.front().name, "android.permission.ACCESS_LOCAL_NETWORK");
    EXPECT_EQ(answer.missingToJson().at(0).at("snippet"), "<uses-permission />");

    EXPECT_EQ(Permission::Answer::fromJson(core::Json::parse(R"({"state": 5})")).state, Permission::State::Unknown);
    EXPECT_FALSE(Permission::Answer::fromJson(core::Json::array()).message.empty());
}

TEST_F(SystemTest, VibratesForAPositiveNumberOfSeconds) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    system.vibrate(0.25F);
    EXPECT_EQ(fixture.host().getVibrations(), (std::vector<float>{0.25F}));
    for (const float seconds : {0.0F, -1.0F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        EXPECT_THROW(system.vibrate(seconds), std::invalid_argument);
    }
    EXPECT_EQ(fixture.host().getVibrations().size(), 1U);
}

TEST(SystemLuaTest, ReportsTheInfoThemeAndBattery) {
    test::EngineFixture fixture;
    fixture.runLua("system = require('haylen.system')");
    EXPECT_EQ(fixture.lua("local info = system.info() return info.deviceKind .. ' ' .. tostring(info.cpuCores > 0) .. ' ' .. tostring(info.gpuName) .. ' ' .. tostring(info.locale) .. ' ' .. #info.languages"), "desktop true nil nil 0");
    EXPECT_EQ(fixture.lua("return system.theme()"), "light");
    EXPECT_EQ(fixture.lua("local battery = system.battery() return tostring(battery.level) .. ' ' .. tostring(battery.charging) .. ' ' .. battery.state"), "nil false unknown");

    fixture.host().setSystemInfo({.os = SystemInfo::Os::Android, .osVersion = "16", .deviceModel = "Phone 9", .manufacturer = "Maker", .deviceKind = SystemInfo::DeviceKind::Phone, .cpuName = "Chip G4", .cpuCores = 8, .architecture = SystemInfo::Architecture::Arm64, .memoryBytes = 12884901888, .locale = "pt-BR", .languages = {"pt-BR", "en-US"}, .timeZone = "America/Sao_Paulo"});
    fixture.restart();
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        events = require('haylen.events')
        heard = {}
        events.on('systemThemeChanged', function(data) heard[#heard + 1] = data.theme end)
        events.on('batteryChanged', function(data) heard[#heard + 1] = data.state .. ' ' .. tostring(data.level) end)
        local info = system.info()
        summary = table.concat({info.os, info.osVersion, info.deviceModel, info.manufacturer, info.deviceKind, info.cpuName, info.cpuCores, info.architecture, info.bits, info.memoryBytes, info.locale, table.concat(info.languages, ','), info.timeZone}, '|')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return summary"), "android|16|Phone 9|Maker|phone|Chip G4|8|arm64|64|12884901888|pt-BR|pt-BR,en-US|America/Sao_Paulo");

    fixture.host().setTheme(Theme::Dark);
    fixture.host().setBattery({.level = 0.5F, .charging = true, .state = Battery::State::Charging});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "dark, charging 0.5");
    EXPECT_EQ(fixture.lua("local battery = system.battery() return system.theme() .. ' ' .. battery.level .. ' ' .. tostring(battery.charging) .. ' ' .. battery.state"), "dark 0.5 true charging");
}

TEST(SystemLuaTest, OpensUrlsAndVibrates) {
    test::EngineFixture fixture;
    fixture.runLua("system = require('haylen.system') async = require('async') answers = {}");
    fixture.runLua("opening = system.openUrl('https://example.com') async.spawn(function() local opened = opening:await() answers[#answers + 1] = tostring(opened) end)");
    fixture.host().setOpensUrls(false);
    fixture.runLua("refusing = system.openUrl('island://settings') async.spawn(function() local opened = refusing:await() answers[#answers + 1] = tostring(opened) end)");
    EXPECT_EQ(fixture.lua("return #answers"), "0");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "2"; }));
    EXPECT_EQ(fixture.lua("return table.concat(answers, ' ')"), "true false");
    EXPECT_EQ(fixture.host().getOpenedUrls(), (std::vector<std::string>{"https://example.com", "island://settings"}));

    EXPECT_NE(fixture.lua("system.openUrl('')").find("Opening a url needs a url."), std::string::npos);
    EXPECT_NE(fixture.lua("system.openUrl()").find("string expected"), std::string::npos);

    fixture.runLua("system.vibrate(0.25)");
    EXPECT_EQ(fixture.host().getVibrations(), (std::vector<float>{0.25F}));
    EXPECT_NE(fixture.lua("system.vibrate(0)").find("A vibration lasts a positive number of seconds."), std::string::npos);
    EXPECT_NE(fixture.lua("system.vibrate('long')").find("number expected"), std::string::npos);
}

TEST(SystemLuaTest, WritesAndReadsTheClipboard) {
    test::EngineFixture fixture;
    fixture.runLua("system = require('haylen.system') async = require('async') answers = {}");
    fixture.runLua("copying = system.setClipboard('Island seed: 184467') async.spawn(function() local written = copying:await() answers[#answers + 1] = tostring(written) end)");
    fixture.host().setWritesClipboard(false);
    fixture.runLua("refusing = system.setClipboard('Refused') async.spawn(function() local written = refusing:await() answers[#answers + 1] = tostring(written) end)");
    EXPECT_EQ(fixture.lua("return #answers"), "0");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "2"; }));
    EXPECT_EQ(fixture.lua("return table.concat(answers, ' ')"), "true false");
    EXPECT_EQ(fixture.lua("return system.clipboard()"), "Island seed: 184467");
    EXPECT_NE(fixture.lua("system.setClipboard()").find("string expected"), std::string::npos);
}

TEST(SystemLuaTest, ReadsAndAsksForPermissions) {
    test::EngineFixture fixture;
    fixture.runLua("system = require('haylen.system') async = require('async')");
    EXPECT_EQ(fixture.lua("return system.permission('localNetwork')"), "granted");
    fixture.host().setPermission(Permission::State::Prompt);
    EXPECT_EQ(fixture.lua("return system.permission('localNetwork')"), "prompt");
    EXPECT_NE(fixture.lua("return system.permission('camera')").find("camera"), std::string::npos);

    fixture.host().setPermissionAnswer({.state = Permission::State::Granted});
    fixture.runLua("asking = system.requestPermission('localNetwork') async.spawn(function() granted = asking:await() end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(granted)") == "granted"; }));

    // A project that lacks the permission fails the request with the code "unsupported" and the line it lacks.
    fixture.host().setPermissionAnswer({.state = Permission::State::Denied, .missing = {{.kind = "permission", .name = "android.permission.ACCESS_LOCAL_NETWORK", .file = "app/src/main/AndroidManifest.xml", .snippet = "<uses-permission android:name=\"android.permission.ACCESS_LOCAL_NETWORK\" />"}}, .message = "The engine needs the permission, which the app lacks."});
    fixture.runLua("missing = system.requestPermission('localNetwork') async.spawn(function() local _, failure = missing:await() reason = failure end)");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return type(reason)") == "table"; }));
    EXPECT_EQ(fixture.lua("return reason.code .. ' ' .. reason.data.missing[1].name .. ' ' .. reason.message"), "unsupported android.permission.ACCESS_LOCAL_NETWORK The engine needs the permission, which the app lacks.");
    EXPECT_EQ(fixture.lua("return tostring(missing:cancel())"), "false");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::platform
