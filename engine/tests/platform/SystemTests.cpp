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
    std::vector<Permission::State> answers;
    const auto record = [&answers](Permission::Answer answer) { answers.push_back(answer.state); };
    system.openUrl("https://example.com", record);
    fixture.host().setUrlAnswer({.state = Permission::State::Unsupported, .message = "No app opens the url."});
    system.openUrl("mailto:island@example.com", record);
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<Permission::State>{Permission::State::Granted, Permission::State::Unsupported}));
    EXPECT_EQ(fixture.host().getOpenedUrls(), (std::vector<std::string>{"https://example.com", "mailto:island@example.com"}));
    EXPECT_THROW(system.openUrl("", record), std::invalid_argument);
}

TEST_F(SystemTest, WritesAndReadsTheClipboardWithAnswersOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    std::vector<std::string> answers;
    system.setClipboard("Island seed: 184467", [&answers](Permission::Answer answer) { answers.emplace_back(Permission::stateName(answer.state)); });
    system.readClipboard([&answers](Permission::Answer answer, std::string text) { answers.push_back(std::string(Permission::stateName(answer.state)) + " " + text); });
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");

    // A refused write leaves the clipboard as it was, the way a browser refuses a page without a recent input of the person, and a refused read brings no text.
    fixture.host().setClipboardAnswer({.state = Permission::State::Denied, .message = Permission::refusal(Permission::Kind::Clipboard)});
    system.setClipboard("Refused", [&answers](Permission::Answer answer) { answers.emplace_back(Permission::stateName(answer.state)); });
    system.readClipboard([&answers](Permission::Answer answer, std::string text) { answers.push_back(std::string(Permission::stateName(answer.state)) + " " + text); });
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<std::string>{"granted", "granted Island seed: 184467", "denied", "denied "}));
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");
}

TEST_F(SystemTest, ChecksAndAsksForPermissionsWithAnswersOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    std::vector<Permission::Answer> answers;
    const auto record = [&answers](Permission::Answer answer) { answers.push_back(std::move(answer)); };
    fixture.host().setPermission({.state = Permission::State::Prompt});
    system.checkPermission(Permission::Kind::Camera, record);
    fixture.host().setPermissionAnswer({.state = Permission::State::Denied});
    system.requestPermission(Permission::Kind::LocalNetwork, record);
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    ASSERT_EQ(answers.size(), 2U);
    EXPECT_EQ(answers[0].state, Permission::State::Prompt);
    EXPECT_EQ(answers[1].state, Permission::State::Denied);
    EXPECT_EQ(fixture.host().getPermissionRequests(), (std::vector<Permission::Kind>{Permission::Kind::LocalNetwork}));
}

TEST_F(SystemTest, NamesEveryPermissionAndHowToAskAgain) {
    for (const std::string_view name : {"camera", "microphone", "location", "preciseLocation", "backgroundLocation", "notifications", "photos", "clipboard", "motion", "screens", "localNetwork"}) {
        const std::optional<Permission::Kind> kind = Permission::kindFromName(name);
        ASSERT_TRUE(kind.has_value()) << name;
        EXPECT_EQ(Permission::kindName(*kind), name);
    }
    EXPECT_FALSE(Permission::kindFromName("bluetooth").has_value());
    EXPECT_EQ(Permission::stateFromName("unsupported"), Permission::State::Unsupported);
    EXPECT_EQ(Permission::refusal(Permission::Kind::PreciseLocation), "The person has not allowed the precise location. Ask for it with \"system.requestPermission('preciseLocation')\", which shows the prompt of the system while the system still asks, or the person allows it in the settings of the system.");
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
    EXPECT_EQ(Permission::Answer::fromJson(core::Json::array()).state, Permission::State::Unsupported);
    EXPECT_FALSE(Permission::Answer::fromJson(core::Json::array()).message.empty());
}

TEST_F(SystemTest, VibratesForAPositiveNumberOfSecondsWithAnAnswerOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    std::vector<Permission::State> answers;
    system.vibrate(0.25F, [&answers](Permission::Answer answer) { answers.push_back(answer.state); });
    EXPECT_EQ(fixture.host().getVibrations(), (std::vector<float>{0.25F}));
    EXPECT_TRUE(answers.empty());
    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<Permission::State>{Permission::State::Granted}));

    for (const float seconds : {0.0F, -1.0F, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        EXPECT_THROW(system.vibrate(seconds, {}), std::invalid_argument);
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

TEST(SystemLuaTest, OpensUrlsAndVibratesOrSaysWhyNot) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        async = require('async')
        answers = {}
        function remember(promise)
            async.spawn(function()
                local _, message, failure = promise:await()
                answers[#answers + 1] = failure and (failure.code .. ': ' .. message) or 'done'
            end)
        end
    )");
    // clang-format on
    fixture.runLua("remember(system.openUrl('https://example.com'))");
    fixture.host().setUrlAnswer({.state = Permission::State::Denied, .message = "The browser blocked the new tab."});
    fixture.runLua("remember(system.openUrl('island://settings'))");
    fixture.runLua("remember(system.vibrate(0.25))");
    fixture.host().setVibrationAnswer({.state = Permission::State::Unsupported, .message = "This device has no vibration motor."});
    fixture.runLua("remember(system.vibrate(0.5)) system.vibrate(0.5)");
    EXPECT_EQ(fixture.lua("return #answers"), "0");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "4"; }));
    EXPECT_EQ(fixture.lua("return table.concat(answers, ' | ')"), "done | denied: The browser blocked the new tab. | done | unsupported: This device has no vibration motor.");
    EXPECT_EQ(fixture.host().getOpenedUrls(), (std::vector<std::string>{"https://example.com", "island://settings"}));
    EXPECT_EQ(fixture.host().getVibrations(), (std::vector<float>{0.25F, 0.5F, 0.5F}));

    // A refusal that nothing awaits never stops the app.
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_NE(fixture.lua("system.openUrl('')").find("Opening a url needs a url."), std::string::npos);
    EXPECT_NE(fixture.lua("system.openUrl()").find("string expected"), std::string::npos);
    EXPECT_NE(fixture.lua("system.vibrate(0)").find("A vibration lasts a positive number of seconds."), std::string::npos);
    EXPECT_NE(fixture.lua("system.vibrate('long')").find("number expected"), std::string::npos);
}

TEST(SystemLuaTest, WritesAndReadsTheClipboardOrSaysWhyNot) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        async = require('async')
        answers = {}
        function remember(promise)
            async.spawn(function()
                local text, message, failure = promise:await()
                answers[#answers + 1] = failure and (failure.code .. ': ' .. message) or tostring(text)
            end)
        end
    )");
    // clang-format on
    fixture.runLua("remember(system.setClipboard('Island seed: 184467')) remember(system.clipboard())");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "2"; }));
    fixture.host().setClipboardAnswer({.state = Permission::State::Denied, .message = Permission::refusal(Permission::Kind::Clipboard)});
    fixture.runLua("remember(system.setClipboard('Refused')) remember(system.clipboard())");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "4"; }));
    EXPECT_EQ(fixture.lua("return answers[1] .. ' | ' .. answers[2]"), "nil | Island seed: 184467");
    EXPECT_EQ(fixture.lua("return answers[4]"), "denied: " + Permission::refusal(Permission::Kind::Clipboard));
    EXPECT_NE(fixture.lua("system.setClipboard()").find("string expected"), std::string::npos);
}

TEST(SystemLuaTest, ChecksAndAsksForPermissions) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        async = require('async')
        answers = {}
        function remember(name, promise)
            async.spawn(function()
                local state, message, failure = promise:await()
                answers[name] = state or (failure.code .. ' ' .. message .. ' ' .. (failure.data and failure.data.missing[1].name or 'none'))
            end)
        end
    )");
    // clang-format on
    fixture.host().setPermission({.state = Permission::State::Prompt});
    fixture.runLua("remember('checked', system.permission('camera'))");
    fixture.host().setPermissionAnswer({.state = Permission::State::Granted});
    fixture.runLua("remember('asked', system.requestPermission('localNetwork'))");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(answers.asked)") == "granted"; }));
    EXPECT_EQ(fixture.lua("return answers.checked"), "prompt");
    EXPECT_EQ(fixture.host().getPermissionRequests(), (std::vector<Permission::Kind>{Permission::Kind::LocalNetwork}));
    EXPECT_NE(fixture.lua("system.permission('bluetooth')").find("bluetooth"), std::string::npos);

    // A platform without the capability and a project that lacks a requirement reject with the code "unsupported", the reason and the line the project lacks.
    fixture.host().setPermission({.state = Permission::State::Unsupported, .message = "Apple TV has no camera."});
    fixture.runLua("remember('tv', system.permission('camera'))");
    fixture.host().setPermissionAnswer({.state = Permission::State::Unsupported, .missing = {{.kind = "permission", .name = "android.permission.CAMERA", .file = "app/src/main/AndroidManifest.xml", .snippet = "<uses-permission android:name=\"android.permission.CAMERA\" />"}}, .message = "The engine needs the permission, which the app lacks."});
    fixture.runLua("remember('missing', system.requestPermission('camera'))");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(answers.missing)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return answers.tv"), "unsupported Apple TV has no camera. none");
    EXPECT_EQ(fixture.lua("return answers.missing"), "unsupported The engine needs the permission, which the app lacks. android.permission.CAMERA");

    // The app asked, so an answer that nothing awaits never stops it.
    fixture.runLua("system.requestPermission('camera')");
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

} // namespace haylen::platform
