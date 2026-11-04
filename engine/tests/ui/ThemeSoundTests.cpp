#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/input/Key.hpp"
#include "haylen/platform/AccessibilitySettings.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/TestFiles.hpp"
#include "support/UiFixture.hpp"

namespace haylen::ui {

// A UI whose interactions play through buses of their own, so each bus counts the voices of one interaction. The sounds load before the test goes on, since the library of the UI finds them in the cache of the assets.
class ThemeSoundTest : public ::testing::Test, public test::UiFixture {
  protected:
    static constexpr std::uint32_t kRate = 48000;

    ThemeSoundTest() : UiFixture(makeFiles()) {
        getFixture().runLua(R"(
            audio = require('haylen.audio')
            ui = require('haylen.ui')
            held = {require('haylen.assets').load('ui/long.wav'), require('haylen.assets').load('ui/short.wav')}
            for _, bus in ipairs({'press', 'focus', 'toggle', 'step', 'typing', 'other'}) do
                audio.createBus(bus, 'ui')
            end
        )");
    }

    // A WAV file of a steady tone that lasts the frames.
    [[nodiscard]] static std::vector<std::uint8_t> makeTone(std::size_t frames) {
        return test::TestFiles::wav(kRate, 1, std::vector<float>(frames, 0.25F));
    }

    [[nodiscard]] static std::map<std::string, std::string> makeFiles() {
        const std::vector<std::uint8_t> longTone = makeTone(kRate * 2);
        const std::vector<std::uint8_t> shortTone = makeTone(kRate / 10);
        return {{"content/ui/long.wav", std::string(longTone.begin(), longTone.end())}, {"content/ui/short.wav", std::string(shortTone.begin(), shortTone.end())}, {"content/ui/broken.wav", "not audio"}};
    }

    // Registers a theme from a Lua table and switches to it, and gives its sounds the frames they take to arrive.
    void useTheme(const std::string& definition) {
        getFixture().runLua("ui.setTheme(ui.addTheme(" + definition + "))");
        frames(2);
    }

    [[nodiscard]] std::size_t playing(std::string_view bus) {
        for (const audio::Mixer::BusStats& stats : getEngine().getAudio().getBusStats()) {
            if (stats.name == bus) {
                return stats.playing;
            }
        }
        ADD_FAILURE() << "No bus is named " << bus << ".";
        return 0;
    }

    // Lets every voice of the short sound end.
    void settle() {
        frames(20);
    }
};

TEST_F(ThemeSoundTest, PlaysTheSoundOfEachInteractionThroughItsBus) {
    useTheme(R"({name = 'sounding', sounds = {
        press = {sound = 'ui/short.wav', bus = 'press'},
        toggle = {sound = 'ui/short.wav', bus = 'toggle', volume = 0.5},
        step = {sound = 'ui/short.wav', bus = 'step'},
        type = {sound = 'ui/short.wav', bus = 'typing', interval = 0},
    }})");
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "play", "text": "Play"},
        {"kind": "checkbox", "id": "music", "text": "Music"},
        {"kind": "slider", "id": "volume", "value": 0, "showValue": false},
        {"kind": "textField", "id": "name"}
    ]})");
    // clang-format on

    click(*gui, "play");
    EXPECT_EQ(playing("press"), 1U);
    EXPECT_EQ(playing("toggle"), 0U);
    settle();
    click(*gui, "music");
    EXPECT_EQ(playing("toggle"), 1U);
    EXPECT_EQ(playing("press"), 0U);
    settle();
    click(*gui, "volume");
    EXPECT_EQ(playing("step"), 1U);
    settle();
    click(*gui, "name");
    type(U"a");
    EXPECT_EQ(playing("typing"), 1U);
    EXPECT_EQ(getFixture().lua("local cue = ui.themeSound('toggle') return string.format('%s %s %g %g', cue.sound, cue.bus, cue.volume, cue.interval)"), "ui/short.wav toggle 0.5 0.05");
}

TEST_F(ThemeSoundTest, PlaysTheFocusSoundOnlyWhenThePlayerMovesTheFocus) {
    useTheme(R"({name = 'sounding', sounds = {focus = {sound = 'ui/short.wav', bus = 'focus'}}})");
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "a", "text": "A", "autofocus": true}, {"kind": "button", "id": "b", "text": "B"}, {"kind": "button", "id": "c", "text": "C"}
    ]})");
    // clang-format on
    frames(2);
    EXPECT_EQ(playing("focus"), 0U);

    // The first press only shows where the focus is, and the next one moves it.
    key(input::Key::Down);
    frames(2);
    EXPECT_EQ(playing("focus"), 0U);
    key(input::Key::Down);
    frames(2);
    ASSERT_TRUE(isFocused(*gui, "b"));
    EXPECT_EQ(playing("focus"), 1U);
    settle();
    key(input::Key::Tab);
    frames(2);
    ASSERT_TRUE(isFocused(*gui, "c"));
    EXPECT_EQ(playing("focus"), 1U);
    settle();

    // A pointer and a script move the focus without its sound.
    click(*gui, "a");
    gui->command(getUi().getContext(), "b", "focus", core::Json::object());
    frames(2);
    EXPECT_EQ(playing("focus"), 0U);
}

TEST_F(ThemeSoundTest, CutsTheLastSoundOfAnInteractionAndSkipsPlaysWithinItsInterval) {
    useTheme(R"({name = 'sounding', sounds = {press = {sound = 'ui/long.wav', bus = 'press', interval = 0}}})");
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "a", "text": "A"},
        {"kind": "button", "id": "b", "text": "B"},
        {"kind": "button", "id": "slow", "text": "Slow", "style": {"sounds": {"press": {"sound": "ui/short.wav", "bus": "other", "interval": 10}}}}
    ]})");
    // clang-format on

    // A press sooner than the interval of its sound after the last press plays nothing.
    click(*gui, "slow");
    EXPECT_EQ(playing("other"), 1U);
    settle();
    EXPECT_EQ(playing("other"), 0U);
    click(*gui, "slow");
    EXPECT_EQ(playing("other"), 0U);

    // A press cuts the long sound of the press before it, so presses never pile up.
    click(*gui, "a");
    click(*gui, "b");
    click(*gui, "a");
    EXPECT_EQ(playing("press"), 1U);
}

TEST_F(ThemeSoundTest, StylesReplaceOrSilenceTheSoundsOfTheirNodes) {
    useTheme(R"({name = 'sounding', sounds = {press = {sound = 'ui/short.wav', bus = 'press'}}})");
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "plain", "text": "Plain"},
        {"kind": "button", "id": "quiet", "text": "Quiet", "style": {"sounds": {"press": false}}},
        {"kind": "row", "style": {"sounds": {"press": {"sound": "ui/short.wav", "bus": "other"}}}, "children": [{"kind": "button", "id": "inner", "text": "Inner"}]}
    ]})");
    // clang-format on
    frames(2);

    click(*gui, "plain");
    EXPECT_EQ(playing("press"), 1U);
    settle();
    click(*gui, "quiet");
    EXPECT_EQ(playing("press"), 0U);
    EXPECT_EQ(playing("other"), 0U);
    click(*gui, "inner");
    EXPECT_EQ(playing("other"), 1U);
    EXPECT_EQ(playing("press"), 0U);
}

TEST_F(ThemeSoundTest, KeepsTheSoundsOfTheChosenThemeWhileTheSystemAsksForHigherContrast) {
    useTheme(R"({name = 'sounding', sounds = {press = {sound = 'ui/short.wav', bus = 'press'}}})");
    auto gui = mount(R"({"kind": "button", "id": "play", "text": "Play"})");
    getFixture().host().setAccessibilitySettings({.highContrast = true});
    frames(2);
    ASSERT_EQ(getUi().getTheme().getName(), "darkHighContrast");

    click(*gui, "play");
    EXPECT_EQ(playing("press"), 1U);
    EXPECT_EQ(getFixture().lua("return ui.themeSound('press').bus"), "press");
}

TEST_F(ThemeSoundTest, ReportsASoundThatFailedToLoadWhenItPlays) {
    useTheme(R"({name = 'broken', sounds = {press = {sound = 'ui/broken.wav'}}})");
    auto gui = mount(R"({"kind": "button", "id": "play", "text": "Play"})");

    // The UI asked for the sound first, so its load has failed once this one has.
    getFixture().runLua("failed = false require('async').spawn(function() failed = require('haylen.assets').loadAsync('ui/broken.wav'):await() == nil end)");
    ASSERT_TRUE(getFixture().frameUntil([this] { return getFixture().lua("return failed") == "true"; }));
    ASSERT_EQ(getEngine().getError(), nullptr);

    click(*gui, "play");
    ASSERT_NE(getEngine().getError(), nullptr);
    EXPECT_NE(std::string_view(getEngine().getError()->what()).find("The UI sound \"ui/broken.wav\" could not be loaded."), std::string::npos);
}

TEST_F(ThemeSoundTest, PlaysTheNewSoundOfAChangedFile) {
    useTheme(R"({name = 'sounding', sounds = {press = {sound = 'ui/short.wav', bus = 'press'}}})");
    auto gui = mount(R"({"kind": "button", "id": "play", "text": "Play"})");

    click(*gui, "play");
    settle();
    ASSERT_EQ(playing("press"), 0U);

    // The file now holds a long sound, which still plays once the short one would have ended, as soon as it arrived.
    getFixture().package().setFile("content/ui/short.wav", makeTone(kRate * 2));
    getEngine().getAssets().reload("ui/short.wav");
    const std::vector<std::string> changed{"content/ui/short.wav"};
    getUi().packageChanged(getEngine(), changed);
    // clang-format off
    EXPECT_TRUE(getFixture().frameUntil([&] {
        click(*gui, "play");
        settle();
        return playing("press") == 1U;
    }));
    // clang-format on
}

TEST_F(ThemeSoundTest, AllocatesNothingWhileNothingIsPressed) {
    // clang-format off
    const auto measure = [this] {
        frames(5);
        const test::AllocationTracker tracker;
        frames(10);
        return tracker.getTotal();
    };
    // clang-format on
    const std::size_t empty = measure();
    useTheme(R"({name = 'sounding', sounds = {press = {sound = 'ui/short.wav'}, focus = {sound = 'ui/short.wav'}, type = {sound = 'ui/short.wav'}}})");
    // clang-format off
    auto gui = mount(R"({"kind": "column", "padding": 20, "children": [
        {"kind": "button", "id": "play", "text": "Play", "autofocus": true},
        {"kind": "button", "text": "Styled", "style": {"sounds": {"press": {"sound": "ui/long.wav"}, "focus": false}}},
        {"kind": "textField", "id": "name"}
    ]})");
    // clang-format on
    click(*gui, "play");
    settle();
    EXPECT_EQ(measure(), empty);
}

TEST_F(ThemeSoundTest, ReadsTheSoundsOfThemesAndRejectsBrokenOnes) {
    EXPECT_EQ(getFixture().lua("return tostring(ui.themeSound('press'))"), "nil");
    EXPECT_NE(getFixture().lua("ui.themeSound('click')").find("The theme has no sound named \"click\"."), std::string::npos);

    // clang-format off
    const std::vector<std::pair<std::string, std::string>> cases{
        {"{click = {sound = 'ui/short.wav'}}", R"(The theme has no sound named "click".)"},
        {"{press = 'ui/short.wav'}", R"(The theme sound "press" must be an object, "null" or "false".)"},
        {"{press = {sound = 'ui/short.wav', loud = true}}", R"(Unknown key "loud" in the theme sound "press".)"},
        {"{press = {bus = 'ui'}}", R"(The theme sound "press" must name a sound file in "sound".)"},
        {"{press = {sound = 'ui/short.wav', bus = ''}}", R"(The bus of the theme sound "press" must be the name of an audio bus.)"},
        {"{press = {sound = 'ui/short.wav', volume = -1}}", R"(The volume of the theme sound "press" must be a non-negative number.)"},
        {"{press = {sound = 'ui/short.wav', interval = 'often'}}", R"(The interval of the theme sound "press" must be a non-negative number.)"},
        {"{press = {sound = 'ui/missing.wav'}}", R"(The theme "x" plays the sound file "ui/missing.wav", which the package does not hold.)"},
    };
    // clang-format on
    for (const auto& [sounds, message] : cases) {
        EXPECT_NE(getFixture().lua("ui.addTheme({name = 'x', sounds = " + sounds + "})").find(message), std::string::npos) << sounds;
    }

    // A theme takes the sounds of its base, and removes one with false.
    getFixture().runLua("ui.addTheme({name = 'base', sounds = {press = {sound = 'ui/short.wav'}, toggle = {sound = 'ui/short.wav'}}})");
    getFixture().runLua("ui.setTheme(ui.addTheme({name = 'derived', sounds = {toggle = false}}, 'base'))");
    EXPECT_EQ(getFixture().lua("return ui.themeSound('press').sound .. ' ' .. tostring(ui.themeSound('toggle'))"), "ui/short.wav nil");
}

} // namespace haylen::ui
