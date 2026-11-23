#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/platform/SystemAnswer.hpp"
#include "platform/SystemState.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

class SystemTest : public ::testing::Test {
  protected:
    // Records the theme and battery events of the engine with their data.
    static void recordChanges(haylen::core::Engine& engine, std::vector<std::string>& log) {
        for (const std::string_view name : {haylen::core::LifecycleEvent::kSystemThemeChanged, haylen::core::LifecycleEvent::kBatteryChanged}) {
            engine.getEvents().on(name, [&log, name](haylen::core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
        }
    }

    // An iPad that tells everything about itself.
    [[nodiscard]] static haylen::platform::SystemInfo makeTablet() {
        return {.os = haylen::platform::SystemInfo::Os::IpadOs, .osVersion = "17.5", .deviceModel = "iPad14,1", .manufacturer = "Apple", .deviceKind = haylen::platform::SystemInfo::DeviceKind::Tablet, .cpuName = "Apple M2", .cpuCores = 8, .architecture = haylen::platform::SystemInfo::Architecture::Arm, .bits = 32, .memoryBytes = 8589934592, .gpuName = "Reported by the platform"};
    }
};

TEST_F(SystemTest, ReportsThePlatformWithTheGpuOfTheGraphicsDevice) {
    test::LuaFixture fixture;
    const haylen::platform::SystemInfo& info = fixture.engine().getSystem().getInfo();
#if defined(__APPLE__)
    EXPECT_EQ(info.os, haylen::platform::SystemInfo::Os::MacOs);
#elif defined(_WIN32)
    EXPECT_EQ(info.os, haylen::platform::SystemInfo::Os::Windows);
#else
    EXPECT_EQ(info.os, haylen::platform::SystemInfo::Os::Linux);
#endif
    EXPECT_EQ(info.deviceKind, haylen::platform::SystemInfo::DeviceKind::Desktop);
    EXPECT_EQ(info.cpuCores, static_cast<int>(std::thread::hardware_concurrency()));

    // The dummy backend of the headless host names no adapter, and the GPU is always the one of the graphics device.
    EXPECT_EQ(fixture.engine().getGraphics().getAdapterName(), "");
    fixture.host().setSystemInfo(makeTablet());
    fixture.restart();
    const haylen::platform::SystemInfo& tablet = fixture.engine().getSystem().getInfo();
    EXPECT_EQ(tablet.deviceModel, "iPad14,1");
    EXPECT_EQ(tablet.gpuName, "");
}

TEST_F(SystemTest, WritesOnlyWhatThePlatformReportsAsJson) {
    // The architecture and the width of pointers are those of the build that runs, which every platform reports without asking the system.
    const std::string_view architecture = haylen::platform::SystemInfo::architectureName(haylen::platform::SystemInfo::processArchitecture());
    const int bits = static_cast<int>(sizeof(void*) * 8);
    EXPECT_EQ(haylen::platform::SystemInfo{}.toJson(), (haylen::core::Json{{"os", "macOs"}, {"deviceKind", "desktop"}, {"architecture", architecture}, {"bits", bits}}));

    haylen::platform::SystemInfo tablet = makeTablet();
    EXPECT_EQ(tablet.toJson(), (haylen::core::Json{{"os", "ipadOs"}, {"osVersion", "17.5"}, {"deviceModel", "iPad14,1"}, {"manufacturer", "Apple"}, {"deviceKind", "tablet"}, {"cpuName", "Apple M2"}, {"cpuCores", 8}, {"architecture", "arm"}, {"bits", 32}, {"memoryBytes", 8589934592}, {"gpuName", "Reported by the platform"}}));
    EXPECT_EQ(haylen::platform::SystemInfo::osName(haylen::platform::SystemInfo::Os::TvOs), "tvOs");
    EXPECT_EQ(haylen::platform::SystemInfo::deviceKindName(haylen::platform::SystemInfo::DeviceKind::Tv), "tv");
    EXPECT_EQ(haylen::platform::SystemInfo::architectureName(haylen::platform::SystemInfo::Architecture::X64), "x64");

    EXPECT_EQ(haylen::platform::Battery{}.toJson(), (haylen::core::Json{{"charging", false}, {"state", "unknown"}}));
    EXPECT_EQ((haylen::platform::Battery{.level = 0.6F, .charging = true, .state = haylen::platform::Battery::State::Charging}.toJson()), (haylen::core::Json{{"level", 0.6}, {"charging", true}, {"state", "charging"}}));
    EXPECT_EQ(haylen::platform::Battery::stateName(haylen::platform::Battery::State::None), "none");
}

TEST_F(SystemTest, PublishesEveryChangeOfThemeAndBatteryOnce) {
    test::LuaFixture fixture;
    std::vector<std::string> log;
    recordChanges(fixture.engine(), log);
    haylen::platform::System& system = fixture.engine().getSystem();
    EXPECT_EQ(system.getTheme(), haylen::platform::Theme::Light);
    EXPECT_EQ(system.getBattery(), haylen::platform::Battery{});

    // Platform services report from their own threads, and the engine hears the last report at its next frame.
    // clang-format off
    std::thread([&fixture] {
        fixture.host().setTheme(haylen::platform::Theme::Dark);
        fixture.host().setBattery({.level = 0.8F, .state = haylen::platform::Battery::State::Discharging});
        fixture.host().setBattery({.level = 0.75F, .state = haylen::platform::Battery::State::Discharging});
    }).join();
    // clang-format on
    EXPECT_EQ(system.getTheme(), haylen::platform::Theme::Light);
    fixture.frames(1);
    fixture.host().setTheme(haylen::platform::Theme::Dark);
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"(systemThemeChanged {"theme":"dark"})", R"(batteryChanged {"charging":false,"level":0.75,"state":"discharging"})"}));
    EXPECT_EQ(system.getTheme(), haylen::platform::Theme::Dark);
    EXPECT_EQ(system.getBattery().level, 0.75F);

    // A new app starts with what the platform reported last, without an event.
    fixture.host().setBattery({.charging = true, .state = haylen::platform::Battery::State::Charging});
    fixture.restart();
    std::vector<std::string> restarted;
    recordChanges(fixture.engine(), restarted);
    fixture.frames(1);
    EXPECT_TRUE(restarted.empty());
    EXPECT_EQ(fixture.engine().getSystem().getBattery(), (haylen::platform::Battery{.charging = true, .state = haylen::platform::Battery::State::Charging}));
}

TEST_F(SystemTest, PublishesEveryChangeOfPowerSavingAndTimeZoneOnce) {
    test::LuaFixture fixture;
    std::vector<std::string> log;
    for (const std::string_view name : {haylen::core::LifecycleEvent::kPowerSavingChanged, haylen::core::LifecycleEvent::kTimeZoneChanged}) {
        fixture.engine().getEvents().on(name, [&log, name](haylen::core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
    }

    haylen::platform::System& system = fixture.engine().getSystem();
    EXPECT_EQ(system.getPowerSaving(), std::nullopt);
    EXPECT_EQ(system.getTimeZone(), "");

    fixture.host().setPowerSaving(true);
    fixture.host().setTimeZone("Europe/Lisbon");
    fixture.frames(2);
    fixture.host().setPowerSaving(false);
    fixture.host().setTimeZone("Europe/Lisbon");
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"(powerSavingChanged {"enabled":true})", R"(timeZoneChanged {"timeZone":"Europe/Lisbon"})", R"(powerSavingChanged {"enabled":false})"}));
    EXPECT_EQ(system.getPowerSaving(), false);
    EXPECT_EQ(system.getTimeZone(), "Europe/Lisbon");

    // A new app starts with what the platform reported last, without an event.
    fixture.restart();
    EXPECT_EQ(fixture.engine().getSystem().getTimeZone(), "Europe/Lisbon");
    EXPECT_EQ(fixture.engine().getSystem().getPowerSaving(), false);
}

TEST_F(SystemTest, KeepsTheLastReportOfEveryThread) {
    haylen::platform::SystemState state;
    std::vector<std::thread> reporters;
    for (int index = 0; index < 4; ++index) {
        // clang-format off
        reporters.emplace_back([&state, index] {
            for (int step = 0; step < 100; ++step) {
                state.setTheme(step % 2 == 0 ? haylen::platform::Theme::Dark : haylen::platform::Theme::Light);
                state.setBattery({.level = static_cast<float>(index) / 4.0F});
            }
        });
        // clang-format on
    }

    for (std::thread& reporter : reporters) {
        reporter.join();
    }

    EXPECT_EQ(state.getTheme(), haylen::platform::Theme::Light);
    EXPECT_TRUE(state.getBattery().level.has_value());
}

TEST_F(SystemTest, WritesAndReadsTheClipboardWithAnswersOnALaterFrame) {
    test::LuaFixture fixture;
    haylen::platform::System& system = fixture.engine().getSystem();
    std::vector<std::string> answers;
    system.setClipboard("Island seed: 184467", [&answers](haylen::platform::SystemAnswer answer) { answers.emplace_back(haylen::platform::SystemAnswer::stateName(answer.state)); });
    system.readClipboard([&answers](haylen::platform::SystemAnswer answer, std::string text) { answers.push_back(std::string(haylen::platform::SystemAnswer::stateName(answer.state)) + " " + text); });
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");

    // A refused write leaves the clipboard as it was, the way a browser refuses a page without a recent input of the person, and a refused read brings no text.
    fixture.host().setClipboardAnswer({.state = haylen::platform::SystemAnswer::State::Denied, .message = "The person has not allowed the app to read the clipboard."});
    system.setClipboard("Refused", [&answers](haylen::platform::SystemAnswer answer) { answers.emplace_back(haylen::platform::SystemAnswer::stateName(answer.state)); });
    system.readClipboard([&answers](haylen::platform::SystemAnswer answer, std::string text) { answers.push_back(std::string(haylen::platform::SystemAnswer::stateName(answer.state)) + " " + text); });
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<std::string>{"granted", "granted Island seed: 184467", "denied", "denied "}));
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");
}

TEST_F(SystemTest, ReadsTheAnswersOfNativeCode) {
    const haylen::platform::SystemAnswer answer = haylen::platform::SystemAnswer::fromJson(haylen::core::Json::parse(R"json({"state": "unsupported", "message": "The page needs a feature.", "missing": [{"kind": "permissionsPolicy", "name": "clipboard-read", "file": "", "snippet": "Permissions-Policy: clipboard-read=(self)"}, 3]})json"));
    EXPECT_EQ(answer.state, haylen::platform::SystemAnswer::State::Unsupported);
    EXPECT_EQ(answer.message, "The page needs a feature.");
    ASSERT_EQ(answer.missing.size(), 1U);
    EXPECT_EQ(answer.missing.front().name, "clipboard-read");
    EXPECT_EQ(answer.missingToJson().at(0).at("snippet"), "Permissions-Policy: clipboard-read=(self)");
    EXPECT_EQ(haylen::platform::SystemAnswer::fromJson(haylen::core::Json::parse(R"({"state": "denied"})")).state, haylen::platform::SystemAnswer::State::Denied);

    EXPECT_EQ(haylen::platform::SystemAnswer::fromJson(haylen::core::Json::parse(R"({"state": 5})")).state, haylen::platform::SystemAnswer::State::Unsupported);
    EXPECT_EQ(haylen::platform::SystemAnswer::fromJson(haylen::core::Json::parse(R"({"state": "unknown"})")).state, haylen::platform::SystemAnswer::State::Unsupported);
    EXPECT_EQ(haylen::platform::SystemAnswer::fromJson(haylen::core::Json::array()).state, haylen::platform::SystemAnswer::State::Unsupported);
    EXPECT_FALSE(haylen::platform::SystemAnswer::fromJson(haylen::core::Json::array()).message.empty());
}

// Platforms report their tags as they write them, and the engine keeps the canonical form of each once, leaves out what is no tag and publishes only a list that changed.
TEST_F(SystemTest, PublishesTheLanguagesOfTheSystemInTheirCanonicalForm) {
    test::LuaFixture fixture;
    fixture.host().setLanguages(std::vector<std::string>{"pt_BR", "en-us"});
    fixture.restart();
    haylen::platform::System& system = fixture.engine().getSystem();
    EXPECT_EQ(system.getLanguages(), (std::vector<std::string>{"pt-BR", "en-US"}));

    std::vector<std::string> log;
    fixture.engine().getEvents().on(haylen::core::LifecycleEvent::kSystemLanguagesChanged, [&log](haylen::core::EventBus::Event& event) { log.push_back(event.getData().dump()); });
    fixture.host().setLanguages(std::vector<std::string>{"es-419", "not a tag", "ES-419", "en-US-u-mu-celsius"});
    fixture.frames(1);
    fixture.host().setLanguages(std::vector<std::string>{"es_419", "en_US"});
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"({"languages":["es-419","en-US"]})"}));
    EXPECT_EQ(system.getLanguages(), (std::vector<std::string>{"es-419", "en-US"}));

    // A list that changes and changes back between two frames publishes nothing.
    fixture.host().setLanguages(std::vector<std::string>{"fr"});
    fixture.host().setLanguages(std::vector<std::string>{"es-419", "en-US"});
    fixture.frames(1);
    EXPECT_EQ(log.size(), 1U);
}

TEST(SystemLuaTest, ReportsTheInfoThemeAndBattery) {
    test::LuaFixture fixture;
    fixture.runLua("system = require('haylen.system')");
    EXPECT_EQ(fixture.lua("local info = system.info() return info.deviceKind .. ' ' .. tostring(info.cpuCores > 0) .. ' ' .. tostring(info.gpuName) .. ' ' .. #system.languages()"), "desktop true nil 0");
    EXPECT_EQ(fixture.lua("return system.theme()"), "light");
    EXPECT_EQ(fixture.lua("local battery = system.battery() return tostring(battery.level) .. ' ' .. tostring(battery.charging) .. ' ' .. battery.state"), "nil false unknown");

    fixture.host().setSystemInfo({.os = haylen::platform::SystemInfo::Os::Android, .osVersion = "16", .deviceModel = "Phone 9", .manufacturer = "Maker", .deviceKind = haylen::platform::SystemInfo::DeviceKind::Phone, .cpuName = "Chip G4", .cpuCores = 8, .architecture = haylen::platform::SystemInfo::Architecture::Arm64, .memoryBytes = 12884901888});
    fixture.restart();
    // clang-format off
    fixture.runLua(R"(
        system = require('haylen.system')
        events = require('haylen.events')
        heard = {}
        events.on('systemThemeChanged', function(data) heard[#heard + 1] = data.theme end)
        events.on('batteryChanged', function(data) heard[#heard + 1] = data.state .. ' ' .. tostring(data.level) end)
        events.on('systemLanguagesChanged', function(data) heard[#heard + 1] = table.concat(data.languages, ',') end)
        local info = system.info()
        summary = table.concat({info.os, info.osVersion, info.deviceModel, info.manufacturer, info.deviceKind, info.cpuName, info.cpuCores, info.architecture, info.bits, info.memoryBytes}, '|')
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return summary"), "android|16|Phone 9|Maker|phone|Chip G4|8|arm64|64|12884901888");

    fixture.host().setTheme(haylen::platform::Theme::Dark);
    fixture.host().setBattery({.level = 0.5F, .charging = true, .state = haylen::platform::Battery::State::Charging});
    fixture.host().setLanguages(std::vector<std::string>{"pt-BR", "en"});
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "dark, charging 0.5, pt-BR,en");
    EXPECT_EQ(fixture.lua("return table.concat(system.languages(), ',')"), "pt-BR,en");

    // The power saving mode and the time zone stay nil until the platform tells them.
    EXPECT_EQ(fixture.lua("return tostring(system.powerSaving()) .. ' ' .. tostring(system.timeZone())"), "nil nil");
    fixture.runLua("events.on('powerSavingChanged', function(data) heard[#heard + 1] = 'saving ' .. tostring(data.enabled) end) events.on('timeZoneChanged', function(data) heard[#heard + 1] = data.timeZone end)");
    fixture.host().setPowerSaving(true);
    fixture.host().setTimeZone("Asia/Tokyo");
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "dark, charging 0.5, pt-BR,en, saving true, Asia/Tokyo");
    EXPECT_EQ(fixture.lua("return tostring(system.powerSaving()) .. ' ' .. system.timeZone()"), "true Asia/Tokyo");
    EXPECT_EQ(fixture.lua("local battery = system.battery() return system.theme() .. ' ' .. battery.level .. ' ' .. tostring(battery.charging) .. ' ' .. battery.state"), "dark 0.5 true charging");
}

TEST(SystemLuaTest, WritesAndReadsTheClipboardOrSaysWhyNot) {
    test::LuaFixture fixture;
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
    fixture.host().setClipboardAnswer({.state = haylen::platform::SystemAnswer::State::Denied, .message = "The person has not allowed the app to read the clipboard."});
    fixture.runLua("remember(system.setClipboard('Refused')) remember(system.clipboard()) system.setClipboard('Unheard')");
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return #answers") == "4"; }));
    EXPECT_EQ(fixture.lua("return answers[1] .. ' | ' .. answers[2]"), "nil | Island seed: 184467");
    EXPECT_EQ(fixture.lua("return answers[4]"), "denied: The person has not allowed the app to read the clipboard.");

    // A refusal that nothing awaits never stops the app.
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_NE(fixture.lua("system.setClipboard()").find("string expected"), std::string::npos);
}

} // namespace haylen::lua::platform
