#include <gtest/gtest.h>

#include <stdexcept>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "support/DisplayLayout.hpp"

namespace haylen::platform {

TEST(WindowConfigTest, ReadsTheDesktopOptionsOfHaylenJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({
        "window": {"decorated": false, "transparent": true, "alwaysOnTop": true, "showInTaskbar": false, "focusable": false, "mousePassthrough": true, "keepScreenOn": true,
                   "position": {"anchor": "bottom", "area": "work", "display": "primary", "offset": [0, -8], "fill": "width"}}
    })"));
    EXPECT_FALSE(config.window.decorated);
    EXPECT_TRUE(config.window.transparent);
    EXPECT_TRUE(config.window.alwaysOnTop);
    EXPECT_FALSE(config.window.showInTaskbar);
    EXPECT_FALSE(config.window.focusable);
    EXPECT_TRUE(config.window.mousePassthrough);
    EXPECT_TRUE(config.window.keepScreenOn);
    ASSERT_TRUE(config.window.position.has_value());
    EXPECT_EQ(config.window.position->resolve(test::DisplayLayout::twoDisplays(), {400.0F, 100.0F}), (math::Rect{0.0F, 932.0F, 1920.0F, 100.0F}));
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).toJson(), config.toJson());

    // A transparent window clears to transparent unless `clearColor` says otherwise, and so does its splash screen.
    EXPECT_EQ(config.clearColor, math::Color::transparent());
    EXPECT_EQ(config.splash.background, math::Color::transparent());
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"window": {"transparent": true}, "clearColor": "#102030"})")).clearColor, math::Color::fromHex(0x102030FFU));

    const core::AppConfig defaults = core::AppConfig::fromJson(core::Json::object());
    EXPECT_TRUE(defaults.window.decorated);
    EXPECT_FALSE(defaults.window.transparent);
    EXPECT_FALSE(defaults.window.alwaysOnTop);
    EXPECT_TRUE(defaults.window.showInTaskbar);
    EXPECT_TRUE(defaults.window.focusable);
    EXPECT_FALSE(defaults.window.mousePassthrough);
    EXPECT_FALSE(defaults.window.keepScreenOn);
    EXPECT_EQ(defaults.window.frameRate, 0);
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"window": {"frameRate": 120}})")).window.frameRate, 120);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"window": {"frameRate": 1001}})")), std::invalid_argument);
    EXPECT_EQ(defaults.window.maxPixelDensity, 0.0F);
    EXPECT_FLOAT_EQ(core::AppConfig::fromJson(core::Json::parse(R"({"window": {"maxPixelDensity": 1.5}})")).window.maxPixelDensity, 1.5F);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"window": {"maxPixelDensity": -2}})")), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"window": {"maxPixelDensity": "high"}})")), std::invalid_argument);
    EXPECT_FALSE(defaults.window.position.has_value());
    EXPECT_FALSE(defaults.toJson().at("window").contains("position"));
}

TEST(WindowConfigTest, RejectsInvalidDesktopOptions) {
    const char* documents[] = {
        R"({"window": {"decorated": "no"}})", R"({"window": {"mousePassthrough": "regions"}})", R"({"window": {"position": "left"}})", R"({"window": {"position": 12}})", R"({"window": {"position": {"anchor": "middle"}}})", R"({"window": {"position": {"area": "screen"}}})", R"({"window": {"position": {"fill": "all"}}})", R"({"window": {"position": {"display": 0}}})", R"({"window": {"position": {"display": ""}}})", R"({"window": {"position": {"display": true}}})", R"({"window": {"position": {"monitor": 1}}})", R"({"window": {"position": {"offset": [1]}}})", R"({"window": {"position": {"offset": [1, "a"]}}})", R"({"window": {"position": {"x": 3}}})", R"({"window": {"position": {"x": 3, "y": 4, "anchor": "top"}}})", R"({"window": {"position": {"anchor": "top", "size": 3}}})",
    };
    for (const char* document : documents) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
}

TEST(WindowConfigTest, ReadsTheStatusBarOfHaylenJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"window": {"statusBar": "dark", "statusBarColor": "#F0F0F0"}})"));
    EXPECT_EQ(config.window.statusBar, Window::StatusBar::Dark);
    EXPECT_EQ(config.window.statusBarColor, math::Color::fromHex(0xF0F0F0FFU));
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).toJson(), config.toJson());

    const core::AppConfig defaults = core::AppConfig::fromJson(core::Json::object());
    EXPECT_EQ(defaults.window.statusBar, Window::StatusBar::Hidden);
    EXPECT_FALSE(defaults.window.statusBarColor.has_value());
    EXPECT_EQ(defaults.toJson().at("window").at("statusBar"), "hidden");
    EXPECT_FALSE(defaults.toJson().at("window").contains("statusBarColor"));

    for (const char* document : {R"({"window": {"statusBar": "white"}})", R"({"window": {"statusBar": true}})", R"({"window": {"statusBarColor": "white"}})"}) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
}

TEST(WindowConfigTest, ReadsTheSystemGesturesOfHaylenJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"window": {"systemGestures": "system"}})"));
    EXPECT_EQ(config.window.systemGestures, Window::SystemGestures::System);
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).toJson(), config.toJson());

    const core::AppConfig defaults = core::AppConfig::fromJson(core::Json::object());
    EXPECT_EQ(defaults.window.systemGestures, Window::SystemGestures::System);
    EXPECT_EQ(defaults.toJson().at("window").at("systemGestures"), "system");

    try {
        (void)core::AppConfig::fromJson(core::Json::parse(R"({"window": {"systemGestures": "hidden"}})"));
        FAIL();
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), R"(The "window.systemGestures" value "hidden" in "haylen.json" is unknown. It is "system", "deferred" or "autoHideIndicator".)");
    }

    for (const char* document : {R"({"window": {"systemGestures": ""}})", R"({"window": {"systemGestures": 1}})"}) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
}

TEST(WindowConfigTest, ReadsTheRedrawOfHaylenJson) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"window": {"redraw": "onChange"}})"));
    EXPECT_EQ(config.window.redraw, Window::Redraw::OnChange);
    EXPECT_EQ(config.toJson().at("window").at("redraw"), "onChange");
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).toJson(), config.toJson());

    const core::AppConfig defaults = core::AppConfig::fromJson(core::Json::object());
    EXPECT_EQ(defaults.window.redraw, Window::Redraw::Always);
    EXPECT_EQ(defaults.toJson().at("window").at("redraw"), "always");

    try {
        (void)core::AppConfig::fromJson(core::Json::parse(R"({"window": {"redraw": "sometimes"}})"));
        FAIL();
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), R"(The "window.redraw" value "sometimes" in "haylen.json" is unknown. It is "always" or "onChange".)");
    }

    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"window": {"redraw": true}})")), std::invalid_argument);
}

} // namespace haylen::platform
