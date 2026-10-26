#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/NativeViews.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/RecordingScene.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"
#include "support/TestFiles.hpp"

namespace haylen::core {

namespace {

// Fails in one hook of every frame, like a plugin with a bug in its bookkeeping.
class FailingFramePlugin final : public plugins::Plugin {
  public:
    enum class Hook {
        Event,
        Overlay,
        EndFrame,
    };

    explicit FailingFramePlugin(Hook value) : failing(value) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "failing";
    }
    void event(core::Engine&, const platform::Event&) override {
        fail(Hook::Event, "event failed");
    }
    void renderOverlay(core::Engine&) override {
        fail(Hook::Overlay, "overlay failed");
    }
    void endFrame(core::Engine&) override {
        fail(Hook::EndFrame, "end of frame failed");
    }

  private:
    void fail(Hook hook, const char* message) const {
        if (hook == failing) {
            throw std::runtime_error(message);
        }
    }

    Hook failing;
};

// Fails to start, and counts the stops it receives, which a plugin that never started must not get.
class FailingStartPlugin final : public plugins::Plugin {
  public:
    explicit FailingStartPlugin(int& stopCount) : stops(stopCount) {}

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "failingStart";
    }
    void start(core::Engine&) override {
        throw std::runtime_error("start failed");
    }
    void stop(core::Engine&) override {
        ++stops;
    }

  private:
    int& stops;
};

class CountingPlugin final : public plugins::Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "counting";
    }
    void start(core::Engine&) override {
        ++started;
    }
    void stop(core::Engine&) override {
        ++stopped;
    }
    void event(core::Engine&, const platform::Event&) override {
        ++events;
    }
    void beginFrame(core::Engine&, float) override {
        ++frames;
    }
    void fixedUpdate(core::Engine&, float) override {
        ++fixedSteps;
    }
    void update(core::Engine&, float) override {
        ++updates;
    }
    void render(core::Engine&) override {
        ++renders;
    }
    void renderUi(core::Engine&, const core::SceneView& view) override {
        uiRenders += view.current ? 1 : 0;
    }
    void endFrame(core::Engine&) override {
        ++endedFrames;
    }

    int started = 0;
    int stopped = 0;
    int events = 0;
    int frames = 0;
    int fixedSteps = 0;
    int updates = 0;
    int renders = 0;
    int uiRenders = 0;
    int endedFrames = 0;
};

} // namespace

TEST(AppConfigTest, ReadsEveryField) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({
        "name": "Tiny Island",
        "identifier": "com.haylen.tinyisland",
        "version": "2.0.0",
        "window": {"title": "Island", "width": 800, "height": 600, "fullscreen": true, "highDpi": false, "resizable": false, "vsync": false, "sampleCount": 4},
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
    EXPECT_EQ(config.window.title, "Island");
    EXPECT_EQ(config.window.width, 800);
    EXPECT_TRUE(config.window.fullscreen);
    EXPECT_FALSE(config.window.highDpi);
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
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json{{"input", {{"scrollSpeed", 0}}}}), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json{{"input", {{"mouseAsFinger", true}}}}), std::invalid_argument);
    EXPECT_EQ(config.uiScaling.mode, ui::Scaling::Mode::Physical);
    EXPECT_EQ(config.uiScaling.factor, 1.25F);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json{{"ui", {{"scaleMode", "huge"}}}}), std::invalid_argument);
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json{{"ui", {{"scale", 9}}}}), std::invalid_argument);
    EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"clearColor", "#102030"}}).splash.background, math::Color::fromHex(0x102030FFU));

    // Without a splash section the launch screen keeps no dark background, lasts until the first frame and fades out over a quarter of a second.
    const core::AppConfig::Splash plain = core::AppConfig::fromJson(core::Json::object()).splash;
    EXPECT_FALSE(plain.darkBackground.has_value());
    EXPECT_EQ(plain.duration, 0.0F);
    EXPECT_EQ(plain.fadeOut, 0.25F);
    EXPECT_EQ(config.native.at("game_platform").at("files").at("macos"), "platform/apple/libgame_platform.dylib");
    EXPECT_THROW((void)core::AppConfig::fromJson(core::Json{{"native", core::Json::array()}}), std::invalid_argument);

    const core::AppConfig roundTrip = core::AppConfig::fromJson(config.toJson());
    EXPECT_EQ(roundTrip.toJson(), config.toJson());
    for (const char* scaling : {"fit", "fill", "stretch", "expand", "pixelPerfect"}) {
        EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"design", {{"scaling", scaling}}}}).toJson().at("design").at("scaling"), scaling);
    }
    for (const char* orientation : {"landscape", "portrait", "any"}) {
        EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"orientation", orientation}}).toJson().at("orientation"), orientation);
    }
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::object()).window.title, "Haylen App");
    for (const char* reload : {"module", "restart"}) {
        EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"debug", {{"reload", reload}}}}).toJson().at("debug").at("reload"), reload);
    }
    try {
        (void)core::AppConfig::fromJson(core::Json{{"debug", {{"reload", "always"}}}});
        ADD_FAILURE() << "An unknown reload mode must be refused.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The \"debug.reload\" option must be \"module\" or \"restart\".");
    }
}

// The description, the company and the copyright that desktops and installers show follow the name of the app until it gives its own, and the website stays empty.
TEST(AppConfigTest, ThePublisherFollowsTheNameUntilTheAppGivesItsOwn) {
    const core::AppConfig named = core::AppConfig::fromJson(core::Json{{"name", "Tiny Island"}});
    EXPECT_EQ(named.description, "Tiny Island");
    EXPECT_EQ(named.company, "Tiny Island");
    EXPECT_EQ(named.copyright, "Copyright \xC2\xA9 Tiny Island");

    const core::AppConfig published = core::AppConfig::fromJson(core::Json{{"name", "Tiny Island"}, {"description", "Sail between islands."}, {"company", "Harbor Games"}});
    EXPECT_EQ(published.description, "Sail between islands.");
    EXPECT_EQ(published.copyright, "Copyright \xC2\xA9 Harbor Games");
    EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"copyright", "Copyright 2026 Harbor Games"}}).copyright, "Copyright 2026 Harbor Games");
    EXPECT_EQ(published.toJson().at("company"), "Harbor Games");
    EXPECT_EQ(named.website, "");
    EXPECT_EQ(core::AppConfig::fromJson(core::Json{{"website", "https://harbor.example"}}).toJson().at("website"), "https://harbor.example");
    EXPECT_EQ(core::AppConfig::fromJson(core::Json::object()).toJson().at("copyright"), core::AppConfig{}.copyright);
}

TEST(AppConfigTest, RejectsInvalidValues) {
    for (const char* document : {R"([])", R"({"name": 3})", R"({"company": 3})", R"({"description": false})", R"({"window": {"width": 0}})", R"({"design": {"scaling": "zoom"}})", R"({"design": {"height": -1}})", R"({"orientation": "sideways"})", R"({"fixedRate": 0})", R"({"clearColor": "blue"})", R"({"title": "typo"})", R"({"window": {"fullScreen": true}})", R"({"design": {"widht": 10}})", R"({"splash": {"background": "blue"}})", R"({"splash": {"image": "logo.png"}})", R"({"splash": {"darkBackground": ""}})", R"({"splash": {"duration": -1}})", R"({"splash": {"fadeOut": "slow"}})"}) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
    try {
        (void)core::AppConfig::fromJson(core::Json::parse(R"({"autoload": ["state.player", ""]})"));
        FAIL() << "An autoload needs the name of its module.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The \"autoload\" list in \"app.json\" has an empty module name.");
    }
}

// The window tries the graphics APIs of the list first, each named once, and an empty list leaves the order of the platform.
TEST(AppConfigTest, ReadsTheGraphicsApisTheWindowTriesFirst) {
    EXPECT_TRUE(core::AppConfig::fromJson(core::Json::object()).window.graphicsApis.empty());
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"window": {"graphicsApis": ["vulkan", "openGl"]}})"));
    EXPECT_EQ(config.window.graphicsApis, (std::vector<gpu::Device::Api>{gpu::Device::Api::Vulkan, gpu::Device::Api::OpenGl}));
    EXPECT_EQ(config.toJson().at("window").at("graphicsApis"), core::Json::parse(R"(["vulkan", "openGl"])"));

    // clang-format off
    const auto error = [](const char* document) {
        try {
            (void)core::AppConfig::fromJson(core::Json::parse(document));
        } catch (const std::invalid_argument& failure) {
            return std::string(failure.what());
        }
        return std::string();
    };
    // clang-format on
    EXPECT_EQ(error(R"({"window": {"graphicsApis": ["glide"]}})"), "The \"window.graphicsApis\" value \"glide\" in \"app.json\" is not a graphics API. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".");
    EXPECT_EQ(error(R"({"window": {"graphicsApis": ["none"]}})"), "The \"window.graphicsApis\" value \"none\" in \"app.json\" is not a graphics API. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".");
    EXPECT_EQ(error(R"({"window": {"graphicsApis": ["metal", "metal"]}})"), "The \"window.graphicsApis\" of \"app.json\" names \"metal\" twice.");
    EXPECT_EQ(error(R"({"window": {"graphicsApis": "metal"}})"), "The \"window.graphicsApis\" of \"app.json\" is not an array of graphics API names, such as [\"vulkan\", \"openGl\"].");
}

TEST(AppConfigTest, ReadsHowTheContentReachesTheDevice) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"content": {"updates": {"baseUrl": "https://cdn.example.com/game/", "channel": "beta"}, "delivery": [{"match": "world/asia/**", "mode": "onDemand"}, {"match": "cinematics/*.webm", "mode": "prefetch"}, {"match": "islands/**", "pack": "islands"}]}})"));
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
    const core::AppConfig again = core::AppConfig::fromJson(config.toJson());
    EXPECT_EQ(again.content.updates->channel, "beta");
    EXPECT_EQ(again.content.delivery.size(), 3U);
    EXPECT_EQ(again.content.getPack("islands/reef.tmj"), "islands");
    EXPECT_FALSE(core::AppConfig::fromJson(core::Json::object()).content.updates);

    // A content server is an `https` address that ends with a slash, or an `http` address of this machine while a release is tested.
    EXPECT_NO_THROW((void)core::AppConfig::fromJson(core::Json::parse(R"({"content": {"updates": {"baseUrl": "http://127.0.0.1:8080/tree/"}}})")));
    for (const char* document : {R"({"content": {"updates": {"baseUrl": "http://cdn.example.com/"}}})", R"({"content": {"updates": {"baseUrl": "https://cdn.example.com"}}})", R"({"content": {"updates": {}}})", R"({"content": {"updates": {"baseUrl": "https://a.com/", "channel": "Stable"}}})", R"({"content": {"delivery": [{"match": "../x", "mode": "onDemand"}]}})", R"({"content": {"delivery": [{"match": "x/**", "mode": "later"}]}})", R"({"content": {"delivery": {"match": "x"}}})", R"({"content": {"delivery": [{"match": "x/**"}]}})", R"({"content": {"delivery": [{"match": "x/**", "mode": "onDemand", "pack": "x"}]}})", R"({"content": {"delivery": [{"match": "x/**", "pack": "Expansion"}]}})", R"({"content": {"cdn": true}})"}) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
}

// A rule that names a preload group delivers the files of the group, which the group manifest of the app lists for the asset manager too.
TEST(AppConfigTest, DeliversThePreloadGroupsThatRulesName) {
    const std::string app = R"({"content": {"groups": "groups.json", "delivery": [{"group": "asia", "mode": "onDemand"}, {"match": "world/**", "mode": "prefetch"}]}})";
    io::MemoryPackage package("app", {{"app.json", test::TestFiles::bytes(app)}, {"content/groups.json", test::TestFiles::bytes(R"({"groups": {"asia": ["world/asia/", {"path": "music/tokyo.ogg", "options": {"stream": true}}]}})")}});
    const core::AppConfig config = core::AppConfig::fromPackage(package);
    EXPECT_EQ(config.content.getDelivery("world/asia/maps/tokyo.tmj"), content::Delivery::OnDemand);
    EXPECT_EQ(config.content.getDelivery("music/tokyo.ogg"), content::Delivery::OnDemand);
    EXPECT_EQ(config.content.getDelivery("world/europe/paris.tmj"), content::Delivery::Prefetch) << "The rules after the group keep their order.";
    EXPECT_EQ(config.content.getDelivery("music/paris.ogg"), content::Delivery::Required);
    EXPECT_EQ(core::AppConfig::fromJson(core::AppConfig::fromJson(core::Json::parse(app)).toJson()).content.delivery.front().group, "asia");

    // clang-format off
    const auto failure = [](const std::string& document, const std::string& groups) {
        io::MemoryPackage broken("app", {{"app.json", test::TestFiles::bytes(document)}, {"content/groups.json", test::TestFiles::bytes(groups)}});
        try {
            (void)core::AppConfig::fromPackage(broken);
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // clang-format on
    const std::string asia = R"({"groups": {"asia": ["world/asia/"]}})";
    EXPECT_NE(failure(R"({"content": {"delivery": [{"group": "asia", "mode": "onDemand"}]}})", asia).find("\"content.groups\" must name the group manifest"), std::string::npos);
    EXPECT_NE(failure(R"({"content": {"groups": "groups.json", "delivery": [{"group": "europe", "mode": "onDemand"}]}})", asia).find("names the group \"europe\", which \"content/groups.json\" does not define"), std::string::npos);
    EXPECT_NE(failure(R"({"content": {"groups": "missing.json", "delivery": [{"group": "asia", "mode": "onDemand"}]}})", asia).find("is not in the package"), std::string::npos);
    EXPECT_NE(failure(R"({"content": {"groups": "groups.json", "delivery": [{"group": "asia", "match": "x/**", "mode": "onDemand"}]}})", asia).find("either a \"match\" pattern or the name of a preload \"group\""), std::string::npos);
    EXPECT_NE(failure(R"({"content": {"groups": "groups.json", "delivery": [{"group": "asia", "mode": "onDemand"}]}})", R"({"groups": {"asia": ["../secret/"]}})").find("is not relative to the \"content\" folder"), std::string::npos);
    EXPECT_NE(failure(R"({"content": {"groups": "/groups.json"}})", asia).find("must name a file relative to the \"content\" folder"), std::string::npos);
}

TEST(AppConfigTest, ReadsThePluginsOfTheApp) {
    const core::AppConfig config = core::AppConfig::fromJson(core::Json::parse(R"({"plugins": {"ads": {"testMode": true, "placements": ["top"]}, "cloud-analytics": {}}})"));
    EXPECT_EQ(config.plugins.size(), 2U);
    EXPECT_EQ(config.plugins.at("ads").at("testMode"), true);
    EXPECT_EQ(config.plugins.at("cloud-analytics"), core::Json::object());
    EXPECT_EQ(core::AppConfig::fromJson(config.toJson()).plugins, config.plugins);
    EXPECT_TRUE(core::AppConfig::fromJson(core::Json::object()).plugins.empty());

    // The section is an object of parameter objects, keyed by ids in `dash-case`, which also keeps every id inside the plugins folder of the package.
    for (const char* document : {R"({"plugins": ["ads"]})", R"({"plugins": {"ads": true}})", R"({"plugins": {"Ads": {}}})", R"({"plugins": {"a--ds": {}}})", R"({"plugins": {"ads-": {}}})", R"({"plugins": {"2d-kit": {}}})", R"({"plugins": {"../ads": {}}})", R"({"plugins": {"": {}}})"}) {
        EXPECT_THROW((void)core::AppConfig::fromJson(core::Json::parse(document)), std::invalid_argument) << document;
    }
    try {
        (void)core::AppConfig::fromJson(core::Json::parse(R"({"plugins": {"ads": []}})"));
        FAIL() << "A plugin needs an object of parameter values.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The plugin \"ads\" in \"app.json\" must have an object of parameter values.");
    }

    // Every plugin of `app.json` has its manifest in the package.
    io::MemoryPackage package("app", {{"app.json", test::TestFiles::bytes(R"({"plugins": {"ads": {}}})")}});
    try {
        (void)core::AppConfig::fromPackage(package);
        FAIL() << "The package has no manifest for the plugin.";
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The plugin \"ads\" in \"app.json\" has no \"plugins/ads/plugin.json\" in the package.");
    }
    package.setFile("plugins/ads/plugin.json", test::TestFiles::bytes(R"({"id": "ads"})"));
    EXPECT_EQ(core::AppConfig::fromPackage(package).plugins.size(), 1U);
}

TEST(PluginRegistryTest, FindsPluginsByTypeAndName) {
    plugins::PluginRegistry registry;
    CountingPlugin& plugin = registry.add(std::make_unique<CountingPlugin>());
    EXPECT_EQ(&registry.get<CountingPlugin>(), &plugin);
    EXPECT_EQ(registry.find("counting"), &plugin);
    EXPECT_EQ(registry.find("missing"), nullptr);
    EXPECT_EQ(registry.size(), 1U);
    EXPECT_EQ(registry.getAll().size(), 1U);

    registry.clear();
    EXPECT_EQ(registry.find<CountingPlugin>(), nullptr);
    try {
        (void)registry.get<CountingPlugin>();
        FAIL() << "The registry has no plugin left.";
    } catch (const std::logic_error& error) {
        EXPECT_STREQ(error.what(), "The engine has no plugin of the requested type. Add it with \"Engine::addPlugin\" before code asks for it.");
    }
}

TEST(EngineTest, DrivesPluginsScenesAndTimers) {
    std::vector<std::string> log;
    auto scene = std::make_shared<test::RecordingScene>("main", log);
    CountingPlugin* plugin = nullptr;

    // clang-format off
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&](core::Engine& engine) {
        plugin = &engine.addPlugin(std::make_unique<CountingPlugin>());
        engine.getScenes().push(scene);
    }));
    // clang-format on

    int timerCalls = 0;
    fixture.engine().getTimers().after(0.05F, [&] { ++timerCalls; });
    fixture.frames(6, 1.0 / 60.0);

    EXPECT_EQ(log, (std::vector<std::string>{"main:load", "main:enter", "main:enterTransitionFinished"}));
    EXPECT_EQ(scene->updates, 6);
    EXPECT_EQ(scene->renders, 6);
    EXPECT_EQ(scene->uiRenders, 6);
    EXPECT_GE(scene->fixedSteps, 5);
    EXPECT_EQ(timerCalls, 1);
    ASSERT_NE(plugin, nullptr);
    EXPECT_EQ(plugin->started, 1);
    EXPECT_EQ(plugin->frames, 6);
    EXPECT_EQ(plugin->updates, 6);
    EXPECT_EQ(plugin->renders, 6);
    EXPECT_EQ(plugin->uiRenders, 6);
    EXPECT_EQ(plugin->endedFrames, 6);
    EXPECT_GE(plugin->fixedSteps, 5);
    EXPECT_EQ(fixture.engine().getClock().getFrameIndex(), 6U);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().passes, 0U);

    platform::Event event;
    event.type = platform::Event::Type::KeyDown;
    event.key = input::Key::A;
    fixture.engine().handleEvent(event);
    EXPECT_EQ(plugin->events, 1);
    EXPECT_EQ(log.back(), "main:event");
    EXPECT_TRUE(fixture.engine().getInput().isKeyDown(input::Key::A));
}

// The frame loop reuses its storage, so only the scripts allocate, through the allocator of Lua.
TEST(EngineTest, SteadyFramesAllocateNothing) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        box = {x = 0}
        require('haylen.scene').push({update = function(self, dt) box.x = box.x + dt end, render = function() end})
        require('haylen.timer').every(0.01, function() end)
        require('haylen.tween').to(box, 100, {x = 5})
    )");
    // clang-format on
    fixture.frames(10);
    const test::AllocationTracker tracker;
    fixture.frames(30);
    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST(EngineTest, AnnouncesPluginsAddedWhileRunning) {
    test::EngineFixture fixture;
    std::vector<std::string> started;
    fixture.engine().getEvents().on(core::LifecycleEvent::kPluginStarted, [&](core::EventBus::Event& event) { started.push_back(event.getData().at("name").get<std::string>()); });
    const CountingPlugin& plugin = fixture.engine().addPlugin(std::make_unique<CountingPlugin>());
    EXPECT_EQ(plugin.started, 1);
    EXPECT_EQ(started, std::vector<std::string>{"counting"});
}

TEST(EngineTest, TracksAppStatesFromPlatformEvents) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    std::vector<std::string> log;
    const core::ScopedConnection states = engine.appStateChanged.connect([&](core::Engine::AppState state) { log.push_back("state " + std::to_string(static_cast<int>(state))); });
    const core::ScopedConnection resized = engine.resized.connect([&] { log.emplace_back("resized"); });
    const core::ScopedConnection quit = engine.quitRequested.connect([&] { log.emplace_back("quit"); });
    for (const std::string_view name : {core::LifecycleEvent::kAppActive, core::LifecycleEvent::kAppInactive, core::LifecycleEvent::kAppBackground, core::LifecycleEvent::kWindowResized, core::LifecycleEvent::kWindowFocusLost, core::LifecycleEvent::kWindowFocusGained, core::LifecycleEvent::kAppQuitRequested}) {
        engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + (event.getData().is_null() ? "" : " " + event.getData().dump())); });
    }

    engine.getInput().handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::A}, engine.getViewport());
    engine.getVirtualInput().setButton("jump", true);
    const int persisted = fixture.host().getPersistCount();
    for (const platform::Event::Type type : {platform::Event::Type::FocusLost, platform::Event::Type::FocusGained, platform::Event::Type::Suspended, platform::Event::Type::FocusGained, platform::Event::Type::Resumed, platform::Event::Type::Resized, platform::Event::Type::QuitRequested}) {
        engine.handleEvent({.type = type});
    }
    EXPECT_EQ(log, (std::vector<std::string>{"windowFocusLost", "state 1", "appInactive", "windowFocusGained", "state 0", "appActive", "state 2", "appBackground", "windowFocusGained", "state 0", "appActive", "resized", R"(windowResized {"height":1080.0,"width":1920.0})", "quit", "appQuitRequested"}));
    EXPECT_FALSE(engine.getInput().isKeyDown(input::Key::A));
    EXPECT_FALSE(engine.getVirtualInput().isButtonDown("jump"));
    EXPECT_EQ(fixture.host().getPersistCount(), persisted + 1) << "Going to the background makes the files of the app durable.";
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
}

TEST(EngineTest, TreatsInterruptionsAsInactiveTime) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    std::vector<core::Engine::AppState> states;
    const core::ScopedConnection connection = engine.appStateChanged.connect([&](core::Engine::AppState state) { states.push_back(state); });

    // A phone call keeps the app inactive even when its window gets the focus back, and going away and back keeps the interruption until it ends.
    for (const platform::Event::Type type : {platform::Event::Type::InterruptionBegan, platform::Event::Type::FocusLost, platform::Event::Type::FocusGained, platform::Event::Type::Suspended, platform::Event::Type::Resumed}) {
        engine.handleEvent({.type = type});
    }
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
    EXPECT_EQ(states, (std::vector{core::Engine::AppState::Inactive, core::Engine::AppState::Background, core::Engine::AppState::Inactive, core::Engine::AppState::Active}));

    // An interruption that ends while the window has no focus leaves the app inactive until the focus returns.
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    engine.handleEvent({.type = platform::Event::Type::InterruptionBegan});
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
}

TEST(EngineTest, ComesBackInactiveWhileTheWindowLacksTheFocus) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();

    // A dialog took the focus of the window before the app went to the background and still has it when the app comes back.
    for (const platform::Event::Type type : {platform::Event::Type::FocusLost, platform::Event::Type::Suspended, platform::Event::Type::Resumed}) {
        engine.handleEvent({.type = type});
    }
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Active);
}

TEST(EngineTest, PublishesTheKeyboardAndTheNetwork) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "design": {"width": 960, "height": 540, "scaling": "fit"}})"}});
    core::Engine& engine = fixture.engine();
    std::vector<std::string> log;
    for (const std::string_view name : {core::LifecycleEvent::kKeyboardShown, core::LifecycleEvent::kKeyboardHidden, core::LifecycleEvent::kNetworkOnline, core::LifecycleEvent::kNetworkOffline}) {
        engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + (event.getData().is_null() ? "" : " " + event.getData().dump())); });
    }

    // The keyboard frame reaches the app in design units, and only changes are published.
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 1080.0F, 0.0F, 0.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 600.0F, 1920.0F, 480.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged, .keyboardFrame = {0.0F, 600.0F, 1920.0F, 480.0F}});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged});
    engine.handleEvent({.type = platform::Event::Type::KeyboardChanged});

    // The network is unknown until the platform reports it, and the first report publishes the state the app starts in.
    EXPECT_EQ(engine.getNetworkState(), core::Engine::NetworkState::Unknown);
    for (const bool online : {true, true, false, false, true}) {
        engine.handleEvent({.type = platform::Event::Type::NetworkChanged, .online = online});
    }
    EXPECT_EQ(engine.getNetworkState(), core::Engine::NetworkState::Online);
    EXPECT_EQ(log, (std::vector<std::string>{R"(keyboardShown {"height":240.0,"width":960.0,"x":0.0,"y":300.0})", "keyboardHidden", "networkOnline", "networkOffline", "networkOnline"}));
}

TEST(EngineTest, PublishesWindowAndGamepadChanges) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    std::vector<std::string> log;
    for (const std::string_view name : {core::LifecycleEvent::kWindowFullscreenChanged, core::LifecycleEvent::kWindowOrientationChanged, core::LifecycleEvent::kWindowSafeAreaChanged, core::LifecycleEvent::kGamepadConnected, core::LifecycleEvent::kGamepadDisconnected}) {
        engine.getEvents().on(name, [&log, name](core::EventBus::Event& event) { log.push_back(std::string(name) + " " + event.getData().dump()); });
    }
    fixture.host().setGamepad(1, {.connected = true, .name = "Pad"});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"(gamepadConnected {"gamepad":2,"name":"Pad"})"}));

    log.clear();
    fixture.host().setFullscreen(true);
    fixture.host().setSafeAreaInsets({.top = 108.0F});
    fixture.host().setGamepad(1, {});
    fixture.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{R"(windowFullscreenChanged {"fullscreen":true})", R"(windowSafeAreaChanged {"height":972.0,"width":1920.0,"x":0.0,"y":108.0})", R"(gamepadDisconnected {"gamepad":2,"name":"Pad"})"}));

    log.clear();
    fixture.host().setSafeAreaInsets({});
    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    ASSERT_EQ(log.size(), 2U);
    EXPECT_EQ(log[0], R"(windowOrientationChanged {"orientation":"portrait"})");
    EXPECT_EQ(log[1].rfind("windowSafeAreaChanged", 0), 0U);
}

TEST(EngineTest, KeepsHeldTouchesOnTheirScreenPointAcrossAResize) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    platform::Event began;
    began.type = platform::Event::Type::TouchBegan;
    began.touchCount = 1;
    began.touches[0] = {.id = 1, .position = {400.0F, 300.0F}, .changed = true};
    engine.handleEvent(began);
    fixture.frames(1);

    fixture.host().resize({1080.0F, 1920.0F});
    fixture.frames(1);
    const math::Vec2 expected = engine.getViewport().toDesign({400.0F, 300.0F});
    ASSERT_EQ(engine.getInput().getTouches().size(), 1U);
    EXPECT_NEAR(engine.getInput().getTouches()[0].position.x, expected.x, 1e-3F);
    EXPECT_NEAR(engine.getInput().getTouches()[0].position.y, expected.y, 1e-3F);
}

TEST(EngineTest, HaltsUpdatesByTheLifecycleOptions) {
    test::EngineFixture fixture({{"app.json", R"({"name": "Test App", "identifier": "com.haylen.tests", "lifecycle": {"pauseOnFocusLoss": true, "muteOnFocusLoss": true}})"}});
    core::Engine& engine = fixture.engine();
    EXPECT_TRUE(engine.getLifecycle().pauseOnBackground);
    EXPECT_TRUE(engine.getLifecycle().pauseOnFocusLoss);
    int ticks = 0;
    engine.getTimers().every(0.05F, [&] { ++ticks; });

    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    EXPECT_TRUE(engine.isHalted());
    EXPECT_TRUE(engine.getAudio().isOutputMuted());
    EXPECT_FALSE(engine.getAudio().isBusMuted("master")) << "What the app saves of its mute stays its own choice.";
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.audio').outputMuted()) .. ' ' .. tostring(require('haylen.audio').busMuted('master'))"), "true false");
    fixture.frames(5);
    EXPECT_EQ(ticks, 0);

    // Coming back does not jump the clock by the time the app was away.
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(engine.getAudio().isOutputMuted());
    fixture.frames(1, 10.0);
    EXPECT_EQ(ticks, 0);
    fixture.frames(1, 0.06);
    EXPECT_EQ(ticks, 1);

    engine.handleEvent({.type = platform::Event::Type::Suspended});
    EXPECT_TRUE(engine.isHalted());
    const std::uint64_t passes = engine.getRenderer2D().getStats().passes;
    fixture.frames(3);
    EXPECT_EQ(engine.getRenderer2D().getStats().passes, passes);
    engine.setLifecycle({.pauseOnBackground = false});
    EXPECT_FALSE(engine.isHalted());
    fixture.frames(1, 0.06);
    fixture.frames(1, 0.06);
    EXPECT_EQ(ticks, 2);
    engine.handleEvent({.type = platform::Event::Type::Resumed});
    EXPECT_FALSE(engine.isHalted());
}

// The storage and the preferences become durable for an app that the platform may end in the background, also when a listener of the event failed.
TEST(EngineTest, MakesSavedDataDurableInTheBackgroundEvenWhenAListenerFails) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.events').on('appBackground', function() require('haylen.preferences').set('progress', 3) error('the save broke') end)");
    const int persisted = fixture.host().getPersistCount();
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    EXPECT_EQ(fixture.host().getPersistCount(), persisted + 2);
    EXPECT_NE(fixture.engine().getStorage().readText("preferences.json").find("\"progress\": 3"), std::string::npos);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("the save broke"), std::string_view::npos);
}

TEST(EngineTest, PausesTheGame) {
    std::vector<std::string> log;
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    const core::ScopedConnection paused = engine.pausedChanged.connect([&](bool value) { log.push_back(value ? "paused" : "resumed"); });
    engine.getEvents().on(core::LifecycleEvent::kPaused, [&](core::EventBus::Event&) { log.emplace_back("event paused"); });
    engine.getEvents().on(core::LifecycleEvent::kUnpaused, [&](core::EventBus::Event&) { log.emplace_back("event unpaused"); });
    auto game = std::make_shared<test::RecordingScene>("game", log);
    engine.getScenes().push(game);
    fixture.frames(1);

    engine.setPaused(true);
    engine.setPaused(true);
    EXPECT_TRUE(engine.isPaused());
    const int fixedBefore = game->fixedSteps;
    fixture.frames(3);
    EXPECT_EQ(game->updates, 1);
    EXPECT_EQ(game->fixedSteps, fixedBefore);
    EXPECT_EQ(game->renders, 4);

    engine.setPaused(false);
    fixture.frames(1);
    EXPECT_EQ(game->updates, 2);
    EXPECT_EQ(log, (std::vector<std::string>{"game:load", "game:enter", "game:enterTransitionFinished", "game:paused", "paused", "event paused", "game:unpaused", "resumed", "event unpaused"}));
}

TEST(EngineTest, ReportsLowMemoryToTheApp) {
    test::EngineFixture fixture({{"content/kept.json", "{}"}, {"content/dropped.json", "[]"}});
    assets::Manager& assets = fixture.engine().getAssets();
    const std::shared_ptr<void> kept = assets.load("json", "kept.json");
    (void)assets.load("json", "dropped.json");
    EXPECT_EQ(assets.getCachedCount(), 1U);

    int warnings = 0;
    const core::ScopedConnection lowMemory = fixture.engine().lowMemory.connect([&] { ++warnings; });
    fixture.runLua("warnings = 0 require('haylen.scene').push({event = function(_, event) if event.type == 'lowMemory' then warnings = warnings + 1 end end})");
    fixture.frames(1);
    platform::Event event;
    event.type = platform::Event::Type::LowMemory;
    fixture.engine().handleEvent(event);
    EXPECT_EQ(warnings, 1);
    EXPECT_EQ(fixture.lua("return warnings"), "1");
    EXPECT_EQ(assets.releaseUnused(), 0U) << "The warning already pruned the entries of released assets.";
}

TEST(EngineTest, ReportsErrorsAndKeepsRendering) {
    std::vector<std::string> log;
    auto scene = std::make_shared<test::RecordingScene>("main", log);
    // clang-format off
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&](core::Engine& engine) {
        engine.getScenes().push(scene);
    }));
    // clang-format on

    std::string reported;
    const core::ScopedConnection connection = fixture.engine().errorRaised.connect([&](const lua::Error& error) { reported = error.what(); });
    fixture.frames(1);
    fixture.engine().reportError("first failure");
    fixture.engine().reportError("second failure");
    fixture.frames(3);

    EXPECT_EQ(reported, "first failure");
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "first failure");
    EXPECT_EQ(scene->updates, 1);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);
}

TEST(EngineTest, HandsTheErrorThatStoppedTheAppToTheHost) {
    test::EngineFixture fixture({{"source/main.lua", "local function explode()\n    error('the island sank')\nend\nexplode()"}});
    ASSERT_EQ(fixture.host().getErrorReports().size(), 1U);
    const core::Json& report = fixture.host().getErrorReports().front();
    EXPECT_EQ(report.at("message"), "the island sank");
    EXPECT_EQ(report.at("file"), "source/main.lua");
    EXPECT_EQ(report.at("line"), 2);
    EXPECT_NE(report.at("traceback").get<std::string>().find("source/main.lua:2"), std::string::npos);
    ASSERT_FALSE(report.at("frames").empty());
    EXPECT_EQ(report.at("frames").at(1), (core::Json{{"source", "source/main.lua"}, {"line", 2}, {"function", "local 'explode'"}, {"kind", "lua"}}));
    EXPECT_EQ(report, fixture.engine().getError()->toJson());

    // Only the error that the error screen shows reaches the host.
    fixture.engine().reportError("a later failure");
    EXPECT_EQ(fixture.host().getErrorReports().size(), 1U);
}

TEST(EngineTest, TurnsStartupFailuresIntoErrors) {
    {
        test::EngineFixture failing({}, std::make_unique<test::TestApplication>([](core::Engine&) { throw std::runtime_error("cannot start"); }));
        ASSERT_NE(failing.engine().getError(), nullptr);
        EXPECT_STREQ(failing.engine().getError()->what(), "cannot start");
    }

    test::EngineFixture broken({{"source/main.lua", "this is not lua"}});
    ASSERT_NE(broken.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(broken.engine().getError()->what()).find("source/main.lua"), std::string::npos);
}

TEST(EngineTest, ReportsFailuresFromAsynchronousCallbacksAndRendering) {
    test::EngineFixture fixture;
    fixture.engine().getJobs().postToFrame([] { throw std::runtime_error("late failure"); });
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "late failure");
}

TEST(EngineTest, ReportsFailuresOfPluginsInEveryHookOfAFrame) {
    for (const auto& [hook, message] : std::vector<std::pair<FailingFramePlugin::Hook, std::string_view>>{{FailingFramePlugin::Hook::Event, "event failed"}, {FailingFramePlugin::Hook::Overlay, "overlay failed"}, {FailingFramePlugin::Hook::EndFrame, "end of frame failed"}}) {
        test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([failing = hook](core::Engine& engine) { engine.addPlugin(std::make_unique<FailingFramePlugin>(failing)); }));
        fixture.engine().handleEvent({.type = platform::Event::Type::KeyDown, .key = input::Key::A});
        fixture.frames(1);
        ASSERT_NE(fixture.engine().getError(), nullptr) << message;
        EXPECT_EQ(std::string_view(fixture.engine().getError()->what()), message);
    }
}

TEST(EngineTest, StartsPluginsAddedBeforeItStartsOnce) {
    test::TemporaryDirectory directory;
    platform::HeadlessHost host(directory.getPath());
    std::optional<core::Engine::Phase> startPhase;
    core::Engine engine(host, std::make_shared<io::MemoryPackage>("app"), core::AppConfig{}, std::make_unique<test::TestApplication>([&startPhase](core::Engine& running) { startPhase = running.getPhase(); }));
    const CountingPlugin& plugin = engine.addPlugin(std::make_unique<CountingPlugin>());
    EXPECT_EQ(plugin.started, 0);

    // The start of the app runs as a lifecycle step, and a second start changes nothing.
    engine.start();
    engine.start();
    EXPECT_EQ(plugin.started, 1);
    EXPECT_EQ(startPhase, core::Engine::Phase::Lifecycle);
    EXPECT_EQ(engine.getPhase(), core::Engine::Phase::Frame);
    engine.stop();
    EXPECT_EQ(plugin.stopped, 1);
}

TEST(EngineTest, ReportsFailuresOfListenersWhenNativeUiCoversTheApp) {
    test::EngineFixture fixture;
    fixture.runLua("require('haylen.events').on('appInactive', function() error('the cover broke', 0) end)");
    fixture.host().getNativeViews().coverApp();
    fixture.frames(1);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "the cover broke");
    EXPECT_EQ(fixture.engine().getAppState(), core::Engine::AppState::Inactive);
}

TEST(EngineTest, StopsOnlyThePluginsThatStarted) {
    int stops = 0;
    test::EngineFixture fixture({}, std::make_unique<test::TestApplication>([&stops](core::Engine& engine) { engine.addPlugin(std::make_unique<FailingStartPlugin>(stops)); }));
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_STREQ(fixture.engine().getError()->what(), "start failed");
    fixture.engine().stop();
    EXPECT_EQ(stops, 0);
}

TEST(EngineTest, ShowsErrorsOfAsyncTasksWithTheirStack) {
    test::EngineFixture fixture({{"source/scenes/loader.lua", "return function()\n    require('async').sleep(1):await()\n    error('the island failed to load')\nend"}});
    fixture.runLua("require('async').spawn(require('scenes.loader'))");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    const lua::Error& error = *fixture.engine().getError();
    EXPECT_EQ(error.getMessage(), "the island failed to load");
    EXPECT_EQ(error.getFile(), "source/scenes/loader.lua");
    EXPECT_EQ(error.getLine(), 3);

    // The task shows its own frames only, without the native entry of Varn that runs it, and the task function, which native code calls, shows where it is defined.
    ASSERT_EQ(error.getFrames().size(), 2U);
    EXPECT_EQ(error.getFrames()[0].source, "[C]");
    EXPECT_EQ(error.getFrames()[0].function, "global 'error'");
    EXPECT_EQ(error.getFrames()[0].kind, lua::Error::Frame::Kind::C);
    EXPECT_EQ(error.getFrames()[1].source, "source/scenes/loader.lua");
    EXPECT_EQ(error.getFrames()[1].line, 3);
    EXPECT_EQ(error.getFrames()[1].function, "function <source/scenes/loader.lua:1>");
    EXPECT_EQ(error.getFrames()[1].kind, lua::Error::Frame::Kind::Lua);
    EXPECT_EQ(fixture.host().getErrorReports().back(), error.toJson());
    EXPECT_NE(fixture.lua("require('async').spawn(3)").find("bad argument #1 to 'spawn' (function expected, got number)"), std::string::npos);
}

TEST(EngineTest, BoundsThePollOfTheEventLoopByTheFrameTime) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local async = require('async')
        steps = 0
        for task = 1, 2000 do
            async.spawn(function()
                while true do
                    steps = steps + 1
                    async.yield():await()
                end
            end)
        end
    )");
    // clang-format on

    // Tasks that always hand the loop back would keep an unbounded poll running for ever, so each frame takes what its budget allows and leaves the rest ready.
    const auto started = std::chrono::steady_clock::now();
    fixture.frames(1);
    EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::seconds(1));
    const core::Engine::EventLoopPoll& polled = fixture.engine().getEventLoopPoll();
    EXPECT_EQ(polled.budget, std::chrono::nanoseconds(4166666));
    EXPECT_TRUE(polled.ran);
    EXPECT_TRUE(polled.pending);
    EXPECT_EQ(polled.idleMilliseconds, 0);
    const std::string first = fixture.lua("return steps");
    EXPECT_TRUE(fixture.frameUntil([&] { return fixture.lua("return steps") != first; }));
    EXPECT_EQ(fixture.lua("local loop = require('haylen.debug').stats().eventLoop return string.format('%.3f %s %s %d', loop.budget, loop.ran, loop.pending, loop.idle)"), "4.167 true true 0");

    // The budget is a quarter of the frame time, never less than 1 millisecond and never more than 8.
    fixture.frames(1, 1.0);
    EXPECT_EQ(fixture.engine().getEventLoopPoll().budget, std::chrono::milliseconds(8));
    fixture.frames(1, 0.001);
    EXPECT_EQ(fixture.engine().getEventLoopPoll().budget, std::chrono::milliseconds(1));
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(EngineTest, KeepsRunningWhileEveryRejectionIsObservedOrHandled) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        local async = require('async')
        local platform = require('haylen.platform')
        local function failing(message)
            return async.promise(function()
                async.sleep(1):await()
                error(message, 0)
            end)
        end
        caught = failing('A failure that does not matter.'):catch(function(message) return 'caught ' .. message end)
        abandoned = platform.call('store.slow')
        abandoned:cancel()
        async.spawn(function()
            local results = async.allSettled({failing('First.'), failing('Second.')}):await()
            settled = tostring(results[1].ok) .. ' ' .. results[2].error
            recovered = caught:await()
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(recovered ~= nil)") == "true"; }));
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return settled .. ' | ' .. recovered"), "false Second. | caught A failure that does not matter.");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(EngineTest, StopsTheAppWhenARejectionOfNativeCodeIsNotObserved) {
    test::EngineFixture fixture({{"source/main.lua", "require('haylen.jobs').spawn(function() error('The job that nobody awaits failed.', 0) end)"}});
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.engine().getError() != nullptr; }));
    EXPECT_TRUE(fixture.engine().getError()->getMessage().starts_with("A promise rejected, and nothing awaited or caught it. The job that nobody awaits failed.")) << fixture.engine().getError()->getMessage();
}

TEST(EngineTest, ReportsRenderSubmissionFailures) {
    test::EngineFixture fixture;
    fixture.host().resize({20000.0F, 20000.0F});
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([](core::Engine& engine) { engine.getRenderer2D().beginWorld(graphics2d::Camera{}, {.ambientLight = math::Color::black()}); }));
    fixture.frames(2);
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(std::string_view(fixture.engine().getError()->what()).find("Texture dimensions exceed the device limit"), std::string::npos);

    fixture.host().resize({1024.0F, 768.0F});
    fixture.frames(1);
    EXPECT_GT(fixture.engine().getRenderer2D().getStats().sprites, 0U);
}

TEST(EngineTest, AllowsOnlyOneEngineAtATime) {
    test::EngineFixture running;
    EXPECT_THROW(test::EngineFixture second, std::logic_error);
    running.frames(1);
}

TEST(EngineTest, QuitStopsFramesAndAsksTheHost) {
    test::EngineFixture fixture;
    EXPECT_TRUE(fixture.engine().isRunning());
    fixture.engine().quit();
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_TRUE(fixture.host().isQuitRequested());

    const std::uint64_t before = fixture.engine().getClock().getFrameIndex();
    fixture.frames(3);
    EXPECT_EQ(fixture.engine().getClock().getFrameIndex(), before);
}

TEST(EngineTest, KeepsTheExitStatusOfTheQuit) {
    test::EngineFixture fixture;
    EXPECT_EQ(fixture.engine().getExitStatus(), 0);
    fixture.engine().quit(42);
    EXPECT_FALSE(fixture.engine().isRunning());
    EXPECT_EQ(fixture.engine().getExitStatus(), 42);
}

TEST(EngineTest, ExposesServicesAndPlatformName) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    EXPECT_EQ(engine.getPlatformName(), "headless");
    EXPECT_EQ(engine.getConfig().identifier, "com.haylen.tests");
    EXPECT_EQ(&engine.getWindow(), &fixture.host());
    EXPECT_EQ(engine.getPackage().getName(), "test");
    EXPECT_NE(engine.getLuaState(), nullptr);
    EXPECT_NE(engine.getDefaultFont(), nullptr);
    EXPECT_NE(engine.getPlugins().find("core"), nullptr);
    EXPECT_NE(engine.getPlugins().find("graphics2d"), nullptr);
    EXPECT_NE(engine.getPlugins().find("physics2d"), nullptr);
    EXPECT_NE(engine.getPlugins().find("assets"), nullptr);
}

TEST(EngineTest, ConstructorRejectsMissingParts) {
    test::TemporaryDirectory directory;
    platform::HeadlessHost host(directory.getPath());
    EXPECT_THROW(core::Engine(host, nullptr, core::AppConfig{}, std::make_unique<lua::Application>()), std::invalid_argument);
    EXPECT_THROW(core::Engine(host, std::make_shared<io::MemoryPackage>("empty"), core::AppConfig{}, nullptr), std::invalid_argument);
}

} // namespace haylen::core
