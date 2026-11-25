#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "support/TestFiles.hpp"

namespace haylen::core {

class AppConfigTest : public ::testing::Test {
  protected:
    // Returns the message that refuses the document, or an empty text when the document is valid.
    [[nodiscard]] static std::string refusal(const std::string& document) {
        try {
            (void)AppConfig::fromJson(Json::parse(document));
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }

    // Returns the message that refuses a package with the `haylen.json` and the group manifest, or an empty text when the package is valid.
    [[nodiscard]] static std::string refusalOfPackage(const std::string& document, const std::string& groups) {
        io::MemoryPackage package("app", {{"haylen.json", test::TestFiles::bytes(document)}, {"content/groups.json", test::TestFiles::bytes(groups)}});
        try {
            AppConfig config = AppConfig::fromPackage(package);
            config.content.expandGroups(package);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }
};

TEST_F(AppConfigTest, ReadsEveryField) {
    const AppConfig config = AppConfig::fromJson(Json::parse(R"({
        "name": "Tiny Island",
        "identifier": "com.haylen.tinyisland",
        "version": "2.0.0",
        "build": 14,
        "window": {"title": "Island", "width": 800, "height": 600, "fullscreen": true, "maxPixelDensity": 2, "resizable": false, "vsync": false, "sampleCount": 4},
        "design": {"width": 1280, "height": 720, "scaling": "fit"},
        "orientation": "portrait",
        "fixedRate": 30,
        "maxFrameTime": 0.1,
        "clearColor": "#102030",
        "splash": {"logo": "ui/logo.png", "background": "#405060", "darkBackground": "#FF000000", "duration": 1.5, "fadeOut": 0.5},
        "input": {"mouseAsTouch": true, "scrollSpeed": 0.5},
        "ui": {"scaleMode": "physical", "scale": 1.25},
        "native": {"game_platform": {"files": {"macos": "platform/apple/libgame_platform.dylib"}}}
    })"));

    EXPECT_EQ(config.name, "Tiny Island");
    EXPECT_EQ(config.identifier, "com.haylen.tinyisland");
    EXPECT_EQ(config.version, "2.0.0");
    EXPECT_EQ(config.build, 14U);
    EXPECT_EQ(config.window.title, "Island");
    EXPECT_EQ(config.window.width, 800);
    EXPECT_TRUE(config.window.fullscreen);
    EXPECT_FLOAT_EQ(config.window.maxPixelDensity, 2.0F);
    EXPECT_FALSE(config.window.resizable);
    EXPECT_FALSE(config.window.vsync);
    EXPECT_EQ(config.window.sampleCount, 4);
    EXPECT_EQ(config.designSize, math::Vec2(1280.0F, 720.0F));
    EXPECT_EQ(config.scaling, graphics::Viewport::ScalingPolicy::Fit);
    EXPECT_EQ(config.orientation, platform::Orientation::Portrait);
    EXPECT_EQ(config.fixedRate, 30.0);
    EXPECT_EQ(config.clearColor, math::Color::fromHex(0x102030FFU));
    EXPECT_EQ(config.splash.logo, "ui/logo.png");
    EXPECT_EQ(config.splash.background, math::Color::fromHex(0x405060FFU));
    EXPECT_EQ(config.splash.darkBackground, math::Color::fromHex(0x000000FFU));
    EXPECT_EQ(config.splash.duration, 1.5F);
    EXPECT_EQ(config.splash.fadeOut, 0.5F);
    EXPECT_TRUE(config.input.mouseAsTouch);
    EXPECT_FALSE(config.input.touchAsMouse);
    EXPECT_EQ(config.input.scrollSpeed, 0.5F);
    EXPECT_THROW((void)AppConfig::fromJson(Json{{"input", {{"scrollSpeed", 0}}}}), std::invalid_argument);
    EXPECT_THROW((void)AppConfig::fromJson(Json{{"input", {{"mouseAsFinger", true}}}}), std::invalid_argument);
    EXPECT_EQ(config.uiScaling.mode, ui::Scaling::Mode::Physical);
    EXPECT_EQ(config.uiScaling.factor, 1.25F);
    EXPECT_THROW((void)AppConfig::fromJson(Json{{"ui", {{"scaleMode", "huge"}}}}), std::invalid_argument);
    EXPECT_THROW((void)AppConfig::fromJson(Json{{"ui", {{"scale", 9}}}}), std::invalid_argument);
    EXPECT_EQ(AppConfig::fromJson(Json{{"clearColor", "#102030"}}).splash.background, math::Color::fromHex(0x102030FFU));

    // Without a splash section the launch screen keeps no dark background, lasts until the first frame and fades out over a quarter of a second.
    const AppConfig::Splash plain = AppConfig::fromJson(Json::object()).splash;
    EXPECT_FALSE(plain.darkBackground.has_value());
    EXPECT_EQ(plain.duration, 0.0F);
    EXPECT_EQ(plain.fadeOut, 0.25F);
    EXPECT_EQ(config.native.at("game_platform").at("files").at("macos"), "platform/apple/libgame_platform.dylib");
    EXPECT_THROW((void)AppConfig::fromJson(Json{{"native", Json::array()}}), std::invalid_argument);

    const AppConfig roundTrip = AppConfig::fromJson(config.toJson());
    EXPECT_EQ(roundTrip.toJson(), config.toJson());

    for (const char* scaling : {"fit", "fill", "stretch", "expand", "pixelPerfect"}) {
        EXPECT_EQ(AppConfig::fromJson(Json{{"design", {{"scaling", scaling}}}}).toJson().at("design").at("scaling"), scaling);
    }

    for (const char* orientation : {"landscape", "portrait", "any"}) {
        EXPECT_EQ(AppConfig::fromJson(Json{{"orientation", orientation}}).toJson().at("orientation"), orientation);
    }

    EXPECT_EQ(AppConfig::fromJson(Json::object()).window.title, "Haylen App");
}

// An app without a name, with an empty one or with null is the app of the default name, the one that the tooling gives its projects.
TEST_F(AppConfigTest, AnAppWithoutANameTakesTheDefaultName) {
    const std::array<Json, 3> documents{Json::object(), Json{{"name", ""}}, Json{{"name", nullptr}}};
    for (const Json& document : documents) {
        const AppConfig config = AppConfig::fromJson(document);
        EXPECT_EQ(config.name, AppConfig::kDefaultName) << document.dump();
        EXPECT_EQ(config.window.title, AppConfig::kDefaultName) << document.dump();
        EXPECT_EQ(config.company, AppConfig::kDefaultName) << document.dump();
    }

    EXPECT_EQ(AppConfig::fromJson(Json{{"name", "Sky Garden"}}).name, "Sky Garden");
}

// The sections that the application reads itself stay as they are for it, and the engine refuses them for an application that does not read them.
TEST_F(AppConfigTest, KeepsTheSectionsThatTheApplicationReads) {
    const Json document = Json::parse(R"({"name": "Sections", "lua": {"autoloads": ["state"]}})");
    static constexpr std::array<std::string_view, 1> kSections{"lua"};
    const AppConfig config = AppConfig::fromJson(document, kSections);

    ASSERT_TRUE(config.sections.contains("lua"));
    EXPECT_EQ(config.sections.at("lua"), document.at("lua"));
    EXPECT_EQ(config.toJson().at("lua"), document.at("lua"));
    EXPECT_EQ(refusal(document.dump()), "Unknown key \"lua\" in \"haylen.json\".");
    EXPECT_FALSE(AppConfig::fromJson(Json::object(), kSections).sections.contains("lua"));
}

// The description, the company and the copyright that desktops and installers show follow the name of the app until it gives its own, and the website stays empty.
TEST_F(AppConfigTest, ThePublisherFollowsTheNameUntilTheAppGivesItsOwn) {
    const AppConfig named = AppConfig::fromJson(Json{{"name", "Tiny Island"}});
    EXPECT_EQ(named.description, "Tiny Island");
    EXPECT_EQ(named.company, "Tiny Island");
    EXPECT_EQ(named.copyright, "Copyright \xC2\xA9 Tiny Island");

    const AppConfig published = AppConfig::fromJson(Json{{"name", "Tiny Island"}, {"description", "Sail between islands."}, {"company", "Harbor Games"}});
    EXPECT_EQ(published.description, "Sail between islands.");
    EXPECT_EQ(published.copyright, "Copyright \xC2\xA9 Harbor Games");
    EXPECT_EQ(AppConfig::fromJson(Json{{"copyright", "Copyright 2026 Harbor Games"}}).copyright, "Copyright 2026 Harbor Games");
    EXPECT_EQ(published.toJson().at("company"), "Harbor Games");
    EXPECT_EQ(named.website, "");
    EXPECT_EQ(AppConfig::fromJson(Json{{"website", "https://harbor.example"}}).toJson().at("website"), "https://harbor.example");
    EXPECT_EQ(AppConfig::fromJson(Json::object()).toJson().at("copyright"), AppConfig{}.copyright);
}

// The build number grows with every upload, apart from the version that people see, and stays within the version codes that Google Play takes.
TEST_F(AppConfigTest, TheBuildNumberIsAWholeNumberFromOne) {
    EXPECT_EQ(AppConfig::fromJson(Json::object()).build, 1U);
    EXPECT_EQ(AppConfig::fromJson(Json{{"build", AppConfig::kMaxBuild}}).toJson().at("build"), AppConfig::kMaxBuild);

    for (const char* document : {R"({"build": 0})", R"({"build": -3})", R"({"build": 2.5})", R"({"build": "7"})", R"({"build": 2100000001})", R"({"build": 18446744073709551615})"}) {
        EXPECT_THROW((void)AppConfig::fromJson(Json::parse(document)), std::invalid_argument) << document;
    }

    try {
        (void)AppConfig::fromJson(Json{{"build", "7"}});
        FAIL() << "A build number in a string must be refused.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The \"build\" value \"7\" in \"haylen.json\" is not a build number. It is a whole number from 1 to 2100000000 that grows with every upload of the app.");
    }
}

// A mistake of syntax in "haylen.json" names the file, its line and its column.
TEST_F(AppConfigTest, NamesTheLineOfAMistakeOfSyntax) {
    EXPECT_EQ(refusalOfPackage("{\n  \"name\": \"Cave\",\n  \"version\": 1.0.0\n}", "{}"), "The file \"haylen.json\" is not valid JSON. Check line 3, column 17.");
}

TEST_F(AppConfigTest, RejectsInvalidValues) {
    for (const char* document : {R"([])", R"({"name": 3})", R"({"company": 3})", R"({"description": false})", R"({"window": {"width": 0}})", R"({"design": {"scaling": "zoom"}})", R"({"design": {"height": -1}})", R"({"orientation": "sideways"})", R"({"fixedRate": 0})", R"({"clearColor": "blue"})", R"({"title": "typo"})", R"({"window": {"fullScreen": true}})", R"({"design": {"widht": 10}})", R"({"splash": {"background": "blue"}})", R"({"splash": {"image": "logo.png"}})", R"({"splash": {"darkBackground": ""}})", R"({"splash": {"duration": -1}})", R"({"splash": {"fadeOut": "slow"}})"}) {
        EXPECT_THROW((void)AppConfig::fromJson(Json::parse(document)), std::invalid_argument) << document;
    }

    // An empty text is a value like any other, never a way to ask for the default.
    for (const char* document : {R"({"window": {"statusBar": ""}})", R"({"window": {"statusBarColor": ""}})", R"({"design": {"scaling": ""}})", R"({"ui": {"scaleMode": ""}})", R"({"debug": {"stats": ""}})", R"({"orientation": ""})", R"({"clearColor": ""})"}) {
        EXPECT_THROW((void)AppConfig::fromJson(Json::parse(document)), std::invalid_argument) << document;
    }
}

TEST_F(AppConfigTest, RefusesNumbersThatTheirFieldsCannotHold) {
    for (const char* document : {R"({"window": {"width": 800.5}})", R"({"window": {"height": 1e20}})", R"({"window": {"frameRate": -1e300}})", R"({"window": {"sampleCount": 4294967300}})"}) {
        try {
            (void)AppConfig::fromJson(Json::parse(document));
            ADD_FAILURE() << "A number that its field cannot hold must be refused: " << document;
        } catch (const std::invalid_argument& error) {
            EXPECT_TRUE(std::string_view(error.what()).ends_with("in \"haylen.json\" must be a whole number from -2147483648 to 2147483647.")) << error.what();
        }
    }

    try {
        (void)AppConfig::fromJson(Json::parse(R"({"splash": {"duration": 1e300}})"));
        FAIL() << "A number beyond the range of a float must be refused.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The value of \"splash.duration\" in \"haylen.json\" must be a number from -3.40282e+38 to 3.40282e+38.");
    }
}

// An app asks for the GPU that uses the least power of a machine that has two, and draws on the most powerful one otherwise.
TEST_F(AppConfigTest, ReadsWhetherTheAppWantsTheGpuOfLowPower) {
    EXPECT_FALSE(AppConfig::fromJson(Json::object()).window.lowPowerGpu);
    const AppConfig config = AppConfig::fromJson(Json::parse(R"({"window": {"lowPowerGpu": true}})"));
    EXPECT_TRUE(config.window.lowPowerGpu);
    EXPECT_EQ(config.toJson().at("window").at("lowPowerGpu"), true);
}

// The window tries the graphics APIs of the list first, each named once, and an empty list leaves the order of the platform.
TEST_F(AppConfigTest, ReadsTheGraphicsApisTheWindowTriesFirst) {
    EXPECT_TRUE(AppConfig::fromJson(Json::object()).window.graphicsApis.empty());
    const AppConfig config = AppConfig::fromJson(Json::parse(R"({"window": {"graphicsApis": ["vulkan", "openGl"]}})"));
    EXPECT_EQ(config.window.graphicsApis, (std::vector<gpu::Device::Api>{gpu::Device::Api::Vulkan, gpu::Device::Api::OpenGl}));
    EXPECT_EQ(config.toJson().at("window").at("graphicsApis"), Json::parse(R"(["vulkan", "openGl"])"));

    EXPECT_EQ(refusal(R"({"window": {"graphicsApis": ["glide"]}})"), "The \"window.graphicsApis\" value \"glide\" in \"haylen.json\" is not a graphics API. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".");
    EXPECT_EQ(refusal(R"({"window": {"graphicsApis": ["none"]}})"), "The \"window.graphicsApis\" value \"none\" in \"haylen.json\" is not a graphics API. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".");
    EXPECT_EQ(refusal(R"({"window": {"graphicsApis": ["metal", "metal"]}})"), "The \"window.graphicsApis\" of \"haylen.json\" names \"metal\" twice.");
    EXPECT_EQ(refusal(R"({"window": {"graphicsApis": "metal"}})"), "The \"window.graphicsApis\" of \"haylen.json\" is not an array of graphics API names, such as [\"vulkan\", \"openGl\"].");
}

TEST_F(AppConfigTest, ReadsHowTheContentReachesTheDevice) {
    const AppConfig config = AppConfig::fromJson(Json::parse(R"({"content": {"updates": {"baseUrl": "https://cdn.example.com/game/", "channel": "beta"}, "delivery": [{"match": "world/asia/**", "mode": "onDemand"}, {"match": "cinematics/*.webm", "mode": "prefetch"}, {"match": "islands/**", "pack": "islands"}]}})"));
    ASSERT_TRUE(config.content.updates);
    EXPECT_EQ(config.content.updates->baseUrl, "https://cdn.example.com/game/");
    EXPECT_EQ(config.content.updates->channel, "beta");
    EXPECT_TRUE(config.content.updates->enabled);
    EXPECT_EQ(config.content.getDelivery("world/asia/maps/tokyo.tmj"), content::Delivery::OnDemand);
    EXPECT_EQ(config.content.getDelivery("cinematics/intro.webm"), content::Delivery::Prefetch);
    EXPECT_EQ(config.content.getDelivery("cinematics/4k/intro.webm"), content::Delivery::Required);
    EXPECT_EQ(config.content.getDelivery("world/europe/paris.tmj"), content::Delivery::Required);
    EXPECT_EQ(config.content.getPack("islands/coral/reef.tmj"), "islands");
    EXPECT_EQ(config.content.getPack("world/asia/maps/tokyo.tmj"), "");
    const AppConfig again = AppConfig::fromJson(config.toJson());
    EXPECT_EQ(again.content.updates->channel, "beta");
    EXPECT_EQ(again.content.delivery.size(), 3U);
    EXPECT_EQ(again.content.getPack("islands/reef.tmj"), "islands");
    EXPECT_FALSE(AppConfig::fromJson(Json::object()).content.updates);

    // A content server is an `https` address that ends with a slash, or an `http` address of this machine while a release is tested.
    EXPECT_NO_THROW((void)AppConfig::fromJson(Json::parse(R"({"content": {"updates": {"baseUrl": "http://127.0.0.1:8080/tree/"}}})")));

    for (const char* document : {R"({"content": {"updates": {"baseUrl": "http://cdn.example.com/"}}})", R"({"content": {"updates": {"baseUrl": "https://cdn.example.com"}}})", R"({"content": {"updates": {}}})", R"({"content": {"updates": {"baseUrl": "https://a.com/", "channel": "Stable"}}})", R"({"content": {"delivery": [{"match": "../x", "mode": "onDemand"}]}})", R"({"content": {"delivery": [{"match": "x/**", "mode": "later"}]}})", R"({"content": {"delivery": {"match": "x"}}})", R"({"content": {"delivery": [{"match": "x/**"}]}})", R"({"content": {"delivery": [{"match": "x/**", "mode": "onDemand", "pack": "x"}]}})", R"({"content": {"delivery": [{"match": "x/**", "pack": "Expansion"}]}})", R"({"content": {"cdn": true}})"}) {
        EXPECT_THROW((void)AppConfig::fromJson(Json::parse(document)), std::invalid_argument) << document;
    }
}

// A rule that names a preload group delivers the files of the group, which the group manifest of the app lists for the asset manager too.
TEST_F(AppConfigTest, DeliversThePreloadGroupsThatRulesName) {
    const std::string app = R"({"content": {"groups": "groups.json", "delivery": [{"group": "asia", "mode": "onDemand"}, {"match": "world/**", "mode": "prefetch"}]}})";
    io::MemoryPackage package("app", {{"haylen.json", test::TestFiles::bytes(app)}, {"content/groups.json", test::TestFiles::bytes(R"({"groups": {"asia": ["world/asia/", {"path": "music/tokyo.ogg", "options": {"stream": true}}]}})")}});
    AppConfig config = AppConfig::fromPackage(package);
    EXPECT_TRUE(config.content.hasGroupRules()) << "Reading the configuration reads no content.";
    config.content.expandGroups(package);
    EXPECT_FALSE(config.content.hasGroupRules());
    EXPECT_EQ(config.content.getDelivery("world/asia/maps/tokyo.tmj"), content::Delivery::OnDemand);
    EXPECT_EQ(config.content.getDelivery("music/tokyo.ogg"), content::Delivery::OnDemand);
    EXPECT_EQ(config.content.getDelivery("world/europe/paris.tmj"), content::Delivery::Prefetch) << "The rules after the group keep their order.";
    EXPECT_EQ(config.content.getDelivery("music/paris.ogg"), content::Delivery::Required);
    EXPECT_EQ(AppConfig::fromJson(AppConfig::fromJson(Json::parse(app)).toJson()).content.delivery.front().group, "asia");

    const std::string asia = R"({"groups": {"asia": ["world/asia/"]}})";
    EXPECT_NE(refusalOfPackage(R"({"content": {"delivery": [{"group": "asia", "mode": "onDemand"}]}})", asia).find("\"content.groups\" must name the group manifest"), std::string::npos);
    EXPECT_NE(refusalOfPackage(R"({"content": {"groups": "groups.json", "delivery": [{"group": "europe", "mode": "onDemand"}]}})", asia).find("names the group \"europe\", which \"content/groups.json\" does not define"), std::string::npos);
    EXPECT_NE(refusalOfPackage(R"({"content": {"groups": "missing.json", "delivery": [{"group": "asia", "mode": "onDemand"}]}})", asia).find("is not in the package"), std::string::npos);
    EXPECT_NE(refusalOfPackage(R"({"content": {"groups": "groups.json", "delivery": [{"group": "asia", "match": "x/**", "mode": "onDemand"}]}})", asia).find("either a \"match\" pattern or the name of a preload \"group\""), std::string::npos);
    EXPECT_NE(refusalOfPackage(R"({"content": {"groups": "groups.json", "delivery": [{"group": "asia", "mode": "onDemand"}]}})", R"({"groups": {"asia": ["../secret/"]}})").find("is not relative to the \"content\" folder"), std::string::npos);
    EXPECT_NE(refusalOfPackage(R"({"content": {"groups": "/groups.json"}})", asia).find("must name a file relative to the \"content\" folder"), std::string::npos);
}

TEST_F(AppConfigTest, ReadsThePluginsOfTheApp) {
    const AppConfig config = AppConfig::fromJson(Json::parse(R"({"plugins": {"ads": {"parameters": {"testMode": true, "placements": ["top"]}}, "camera": {"path": "../plugins/camera"}, "share": {"git": "https://example.com/share.git", "ref": "1.0.0"}, "cloud-analytics": {}}})"));
    EXPECT_EQ(config.plugins.size(), 4U);
    EXPECT_EQ(config.plugins.at("ads").parameters.at("testMode"), true);
    EXPECT_EQ(config.plugins.at("camera").path, "../plugins/camera");
    EXPECT_EQ(config.plugins.at("camera").parameters, Json::object());
    EXPECT_EQ(config.plugins.at("share").git, "https://example.com/share.git");
    EXPECT_EQ(config.plugins.at("share").ref, "1.0.0");
    EXPECT_TRUE(config.plugins.at("cloud-analytics").path.empty() && config.plugins.at("cloud-analytics").git.empty());
    EXPECT_EQ(AppConfig::fromJson(config.toJson()).plugins, config.plugins);
    EXPECT_TRUE(AppConfig::fromJson(Json::object()).plugins.empty());

    // The section is an object of plugin entries, keyed by ids in `dash-case`, which also keeps every id inside the plugins folder of the package.
    for (const char* document : {R"({"plugins": ["ads"]})", R"({"plugins": {"ads": true}})", R"({"plugins": {"Ads": {}}})", R"({"plugins": {"a--ds": {}}})", R"({"plugins": {"ads-": {}}})", R"({"plugins": {"2d-kit": {}}})", R"({"plugins": {"../ads": {}}})", R"({"plugins": {"": {}}})"}) {
        EXPECT_THROW((void)AppConfig::fromJson(Json::parse(document)), std::invalid_argument) << document;
    }

    EXPECT_EQ(refusal(R"({"plugins": {"ads": []}})"), "The plugin \"ads\" in \"haylen.json\" must be an object with the optional keys \"path\", \"git\", \"ref\" and \"parameters\".");
    EXPECT_EQ(refusal(R"({"plugins": {"ads": {"testMode": true}}})"), "Unknown key \"testMode\" in the plugin \"ads\" of \"haylen.json\".");
    EXPECT_EQ(refusal(R"({"plugins": {"ads": {"path": 3}}})"), "The \"path\" of the plugin \"ads\" in \"haylen.json\" must be a text.");
    EXPECT_EQ(refusal(R"({"plugins": {"ads": {"git": ""}}})"), "The \"git\" of the plugin \"ads\" in \"haylen.json\" must be a text.");
    EXPECT_EQ(refusal(R"({"plugins": {"ads": {"ref": "main"}}})"), "The \"ref\" of the plugin \"ads\" in \"haylen.json\" picks a branch, tag or commit of a git repository, so it needs \"git\".");
    EXPECT_EQ(refusal(R"({"plugins": {"ads": {"parameters": []}}})"), "The \"parameters\" of the plugin \"ads\" in \"haylen.json\" must be an object of parameter values.");

    // A plugin of a repository may lie in a folder of it, which "path" names next to "git".
    const AppConfig nested = AppConfig::fromJson(Json::parse(R"({"plugins": {"ads": {"git": "https://example.com/plugins.git", "ref": "main", "path": "ads"}}})"));
    EXPECT_EQ(nested.plugins.at("ads").git, "https://example.com/plugins.git");
    EXPECT_EQ(nested.plugins.at("ads").ref, "main");
    EXPECT_EQ(nested.plugins.at("ads").path, "ads");

    // Every plugin of `haylen.json` has its manifest in the package, which only the tooling stages for a plugin that `haylen.json` references elsewhere.
    io::MemoryPackage package("app", {{"haylen.json", test::TestFiles::bytes(R"({"plugins": {"ads": {}}})")}});
    try {
        (void)AppConfig::fromPackage(package);
        FAIL() << "The package has no manifest for the plugin.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The plugin \"ads\" in \"haylen.json\" has no \"plugins/ads/plugin.json\" in the package.");
    }

    package.setFile("plugins/ads/plugin.json", test::TestFiles::bytes(R"({"id": "ads"})"));
    EXPECT_EQ(AppConfig::fromPackage(package).plugins.size(), 1U);

    package.setFile("haylen.json", test::TestFiles::bytes(R"({"plugins": {"ads": {}, "camera": {"path": "../plugins/camera"}}})"));
    try {
        (void)AppConfig::fromPackage(package);
        FAIL() << "A folder package lacks a referenced plugin.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The plugin \"camera\" in \"haylen.json\" comes from a \"path\" or a \"git\" repository, so the package has no \"plugins/camera/plugin.json\". Run the app with \"haylen app run\", which places every plugin in the package wherever it comes from.");
    }

    package.setFile("plugins/camera/plugin.json", test::TestFiles::bytes(R"({"id": "camera"})"));
    EXPECT_EQ(AppConfig::fromPackage(package).plugins.size(), 2U);
}

// The tooling builds the app from its category, its kind, its C++ sources and the sections of its platforms, so the engine takes them and checks only their values and types.
TEST_F(AppConfigTest, TakesTheSectionsOfTheToolingAndChecksTheirTypes) {
    EXPECT_NO_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"category": "game", "kind": "cpp", "cpp": {"sources": ["src"]}, "platforms": {"ios": {"infoPlist": {}}, "android": {}}})")));
    EXPECT_NO_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"kind": "lua"})")));

    // clang-format off
    const auto failure = [](const char* document) {
        try {
            (void)core::AppConfig::fromJson(core::Json::parse(document));
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // clang-format on
    const std::string categories = "\"game\", \"entertainment\", \"education\", \"productivity\", \"utility\", \"graphics\", \"music\", \"video\", \"social\" or \"developer\"";
    EXPECT_EQ(failure(R"({"category": "racing"})"), "The \"category\" of \"haylen.json\" must be " + categories + ", not \"racing\".");
    EXPECT_EQ(failure(R"({"category": 1})"), "The \"category\" of \"haylen.json\" must be " + categories + ", not 1.");
    EXPECT_EQ(failure(R"({"kind": "rust"})"), "The \"kind\" of \"haylen.json\" must be \"lua\" or \"cpp\", not \"rust\".");
    EXPECT_EQ(failure(R"({"kind": 1})"), "The \"kind\" of \"haylen.json\" must be \"lua\" or \"cpp\", not 1.");
    EXPECT_EQ(failure(R"({"cpp": ["src"]})"), "The \"cpp\" section of \"haylen.json\" must be an object with the \"sources\" of the C++ app.");
    EXPECT_EQ(failure(R"({"platforms": []})"), "The \"platforms\" section of \"haylen.json\" must be an object of platform sections by name, such as \"ios\" or \"android\".");
    EXPECT_EQ(failure(R"({"platforms": {"ios": true}})"), "The section \"ios\" of \"platforms\" in \"haylen.json\" must be an object.");
}

} // namespace haylen::core
