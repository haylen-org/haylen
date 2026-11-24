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
#include "haylen/platform/System.hpp"
#include "haylen/platform/SystemAnswer.hpp"
#include "platform/SystemState.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class SystemTest : public ::testing::Test {
  protected:
    // Records the theme and battery events of the engine with their data.
    static void recordChanges(haylen::core::Engine& engine, std::vector<std::string>& log) {
        for (const std::string_view name : {haylen::core::LifecycleEvent::kSystemThemeChanged, haylen::core::LifecycleEvent::kBatteryChanged}) {
            engine.getEvents().on(name, [&log, name](haylen::core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
        }
    }

    // An iPad that tells everything about itself.
    [[nodiscard]] static SystemInfo makeTablet() {
        return {.os = SystemInfo::Os::IpadOs, .osVersion = "17.5", .deviceModel = "iPad14,1", .manufacturer = "Apple", .deviceKind = SystemInfo::DeviceKind::Tablet, .cpuName = "Apple M2", .cpuCores = 8, .architecture = SystemInfo::Architecture::Arm, .bits = 32, .memoryBytes = 8589934592, .gpuName = "Reported by the platform"};
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
}

TEST_F(SystemTest, WritesOnlyWhatThePlatformReportsAsJson) {
    // The architecture and the width of pointers are those of the build that runs, which every platform reports without asking the system.
    const std::string_view architecture = SystemInfo::architectureName(SystemInfo::processArchitecture());
    const int bits = static_cast<int>(sizeof(void*) * 8);
    EXPECT_EQ(SystemInfo{}.toJson(), (haylen::core::Json{{"os", "macOs"}, {"deviceKind", "desktop"}, {"architecture", architecture}, {"bits", bits}}));

    SystemInfo tablet = makeTablet();
    EXPECT_EQ(tablet.toJson(), (haylen::core::Json{{"os", "ipadOs"}, {"osVersion", "17.5"}, {"deviceModel", "iPad14,1"}, {"manufacturer", "Apple"}, {"deviceKind", "tablet"}, {"cpuName", "Apple M2"}, {"cpuCores", 8}, {"architecture", "arm"}, {"bits", 32}, {"memoryBytes", 8589934592}, {"gpuName", "Reported by the platform"}}));
    EXPECT_EQ(SystemInfo::osName(SystemInfo::Os::TvOs), "tvOs");
    EXPECT_EQ(SystemInfo::deviceKindName(SystemInfo::DeviceKind::Tv), "tv");
    EXPECT_EQ(SystemInfo::architectureName(SystemInfo::Architecture::X64), "x64");

    EXPECT_EQ(Battery{}.toJson(), (haylen::core::Json{{"charging", false}, {"state", "unknown"}}));
    EXPECT_EQ((Battery{.level = 0.6F, .charging = true, .state = Battery::State::Charging}.toJson()), (haylen::core::Json{{"level", 0.6}, {"charging", true}, {"state", "charging"}}));
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

TEST_F(SystemTest, PublishesEveryChangeOfPowerSavingAndTimeZoneOnce) {
    test::EngineFixture fixture;
    std::vector<std::string> log;
    for (const std::string_view name : {haylen::core::LifecycleEvent::kPowerSavingChanged, haylen::core::LifecycleEvent::kTimeZoneChanged}) {
        fixture.engine().getEvents().on(name, [&log, name](haylen::core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
    }

    System& system = fixture.engine().getSystem();
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

// The thermal state starts unknown, follows every state the platform reports once per change, and a new app starts with the last one.
TEST_F(SystemTest, PublishesEveryChangeOfTheThermalStateOnce) {
    test::EngineFixture fixture;
    std::vector<std::string> log;
    fixture.engine().getEvents().on(haylen::core::LifecycleEvent::kThermalStateChanged, [&log](haylen::core::EventBus::Event& event) { log.push_back(event.getData().dump()); });

    System& system = fixture.engine().getSystem();
    EXPECT_EQ(system.getThermalState(), ThermalState::Unknown);
    fixture.host().setThermalState(ThermalState::Nominal);
    fixture.frames(1);
    fixture.host().setThermalState(ThermalState::Serious);
    fixture.frames(2);
    fixture.host().setThermalState(ThermalState::Critical);
    fixture.host().setThermalState(ThermalState::Fair);
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"({"state":"nominal"})", R"({"state":"serious"})", R"({"state":"fair"})"}));
    EXPECT_EQ(system.getThermalState(), ThermalState::Fair);
    EXPECT_EQ(System::thermalStateName(ThermalState::Unknown), "unknown");

    fixture.restart();
    std::vector<std::string> restarted;
    fixture.engine().getEvents().on(haylen::core::LifecycleEvent::kThermalStateChanged, [&restarted](haylen::core::EventBus::Event& event) { restarted.push_back(event.getData().dump()); });
    fixture.frames(1);
    EXPECT_TRUE(restarted.empty());
    EXPECT_EQ(fixture.engine().getSystem().getThermalState(), ThermalState::Fair);
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

TEST_F(SystemTest, WritesAndReadsTheClipboardWithAnswersOnALaterFrame) {
    test::EngineFixture fixture;
    System& system = fixture.engine().getSystem();
    std::vector<std::string> answers;
    system.setClipboard("Island seed: 184467", [&answers](SystemAnswer answer) { answers.emplace_back(SystemAnswer::stateName(answer.state)); });
    system.readClipboard([&answers](SystemAnswer answer, std::string text) { answers.push_back(std::string(SystemAnswer::stateName(answer.state)) + " " + text); });
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");

    // A refused write leaves the clipboard as it was, the way a browser refuses a page without a recent input of the person, and a refused read brings no text.
    fixture.host().setClipboardAnswer({.state = SystemAnswer::State::Denied, .message = "The person has not allowed the app to read the clipboard."});
    system.setClipboard("Refused", [&answers](SystemAnswer answer) { answers.emplace_back(SystemAnswer::stateName(answer.state)); });
    system.readClipboard([&answers](SystemAnswer answer, std::string text) { answers.push_back(std::string(SystemAnswer::stateName(answer.state)) + " " + text); });
    EXPECT_TRUE(answers.empty());

    fixture.frames(1);
    EXPECT_EQ(answers, (std::vector<std::string>{"granted", "granted Island seed: 184467", "denied", "denied "}));
    EXPECT_EQ(system.getClipboard(), "Island seed: 184467");
}

TEST_F(SystemTest, ReadsTheAnswersOfNativeCode) {
    const SystemAnswer answer = SystemAnswer::fromJson(haylen::core::Json::parse(R"json({"state": "unsupported", "message": "The page needs a feature.", "missing": [{"kind": "permissionsPolicy", "name": "clipboard-read", "file": "", "snippet": "Permissions-Policy: clipboard-read=(self)"}, 3]})json"));
    EXPECT_EQ(answer.state, SystemAnswer::State::Unsupported);
    EXPECT_EQ(answer.message, "The page needs a feature.");
    ASSERT_EQ(answer.missing.size(), 1U);
    EXPECT_EQ(answer.missing.front().name, "clipboard-read");
    EXPECT_EQ(answer.missingToJson().at(0).at("snippet"), "Permissions-Policy: clipboard-read=(self)");
    EXPECT_EQ(SystemAnswer::fromJson(haylen::core::Json::parse(R"({"state": "denied"})")).state, SystemAnswer::State::Denied);

    EXPECT_EQ(SystemAnswer::fromJson(haylen::core::Json::parse(R"({"state": 5})")).state, SystemAnswer::State::Unsupported);
    EXPECT_EQ(SystemAnswer::fromJson(haylen::core::Json::parse(R"({"state": "unknown"})")).state, SystemAnswer::State::Unsupported);
    EXPECT_EQ(SystemAnswer::fromJson(haylen::core::Json::array()).state, SystemAnswer::State::Unsupported);
    EXPECT_FALSE(SystemAnswer::fromJson(haylen::core::Json::array()).message.empty());
}

// Platforms report their tags as they write them, and the engine keeps the canonical form of each once, leaves out what is no tag and publishes only a list that changed.
TEST_F(SystemTest, PublishesTheLanguagesOfTheSystemInTheirCanonicalForm) {
    test::EngineFixture fixture;
    fixture.host().setLanguages(std::vector<std::string>{"pt_BR", "en-us"});
    fixture.restart();
    System& system = fixture.engine().getSystem();
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

} // namespace haylen::platform
