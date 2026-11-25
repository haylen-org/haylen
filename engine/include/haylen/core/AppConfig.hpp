#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/audio/OutputSettings.hpp"
#include "haylen/audio/Session.hpp"
#include "haylen/content/Delivery.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/debug/StatsDisplay.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/FoldSimulation.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/WindowPlacement.hpp"
#include "haylen/ui/Scaling.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::core {

// App settings read from `haylen.json` before any script runs, because the window exists before `source/main.lua`.
struct AppConfig {
    // A transparent window opens able to let the desktop show through its transparent pixels, which it keeps for the whole run, and clears to transparent unless `clearColor` says otherwise. The desktop options change at run time through the window, and `position` places the window when it opens. The status bar of phones and tablets launches as `statusBar` says, and the engine paints `statusBarColor` behind it while it shows. The system gestures at the edges of phones and tablets follow `systemGestures`.
    struct Window {
        std::string title = "Haylen";
        int width = 1280;
        int height = 720;
        bool fullscreen = false;

        // The most framebuffer pixels in each point of the window, or 0 for every pixel of the display, as `platform::Window::setMaxPixelDensity` describes.
        float maxPixelDensity = 0.0F;
        bool resizable = true;
        bool vsync = true;

        // The frames per second the app aims for, or 0 for every refresh of the display, as `platform::Window::setFrameRate` describes.
        int frameRate = 0;
        int sampleCount = 1;
        bool decorated = true;
        bool transparent = false;
        bool alwaysOnTop = false;
        bool showInTaskbar = true;
        bool focusable = true;
        bool mousePassthrough = false;
        bool keepScreenOn = false;
        std::optional<platform::WindowPlacement> position{};
        platform::Window::StatusBar statusBar = platform::Window::StatusBar::Hidden;
        std::optional<math::Color> statusBarColor{};
        platform::Window::SystemGestures systemGestures = platform::Window::SystemGestures::System;

        // Whether the app updates and draws in every frame or only in the frames that something changed in, as `core::Engine::setRedraw` describes.
        platform::Window::Redraw redraw = platform::Window::Redraw::Always;

        // Whether the app shows outputs on the external displays of an iPhone or an iPad of iOS 27 and later, whose scene UIKit connects only for an app that registers the scene accessory of external displays, while such a display mirrors the app without it. Every other platform connects external displays on its own.
        bool externalDisplays = false;

        // The graphics APIs the window tries first, in order, before the other APIs of the build in the order of the platform, which an empty list leaves as it is.
        std::vector<gpu::Device::Api> graphicsApis{};

        // Whether the app draws on the GPU that uses the least power, such as the integrated GPU of a laptop that has two, rather than on the most powerful one.
        bool lowPowerGpu = false;
    };

    // The launch screen that every platform shows while the app starts, with a logo relative to the content folder, or the Haylen logo when it is empty, over a background color that defaults to the clear color, and another one while the system uses dark colors. It stays until the app drew its first complete frame, which `Engine::isFrameComplete` tells, and at least the duration in seconds since the launch, and fades out into the app over the fade-out in seconds. The backends of the tooling turn it into the launch storyboard on Apple platforms, the splash screen on Android and the loading page on the web.
    struct Splash {
        std::string logo;
        math::Color background = math::Color::black();
        std::optional<math::Color> darkBackground;
        float duration = 0.0F;
        float fadeOut = 0.25F;
    };

    // How the app reacts when it leaves the foreground. Pausing halts updates, so timers, tweens, physics and scenes stand still, while the event loop keeps delivering network replies. An app in the background also stops rendering.
    struct Lifecycle {
        bool pauseOnBackground = true;
        bool pauseOnFocusLoss = false;
        bool muteOnFocusLoss = false;
    };

    // Whether the mouse acts as a finger, so an app made for touch screens runs with a mouse, and whether a finger acts as the mouse, so an app that reads the mouse runs on a touch screen. Both change at run time through `input::PointerEmulation`. The scroll speed multiplies every scroll of the wheels and the trackpads, and changes at run time through `input::Input`.
    struct Input {
        bool mouseAsTouch = false;
        bool touchAsMouse = false;
        float scrollSpeed = 1.0F;
    };

    // The debug statistics and the debug drawings the app starts with, whether counted objects publish `objectCreated` and `objectDestroyed`, whether the developer console works outside development, the safe area to simulate instead of the one of the device and whether the safe area shows over the app.
    struct Debug {
        debug::StatsDisplay::Mode stats = debug::StatsDisplay::Mode::Off;
        std::vector<debug::StatsDisplay::Item> statsItems = debug::StatsDisplay::getDefaultItems();
        std::vector<std::string> drawings;
        bool objectEvents = false;
        bool console = false;
        std::optional<platform::SafeAreaSimulation> safeArea;
        std::optional<platform::FoldSimulation> fold;
        bool showSafeArea = false;
    };

    // How the content of a protected release reaches the device, from the `content` object: the server of its updates and downloads, and the files that need not be on the device before the app starts.
    struct Content {
        // The publication tree that serves the updates of the content and the files that download, with its address ending in a slash and the update channel. While updates are off, the server still serves the files that download.
        struct Updates {
            std::string baseUrl;
            std::string channel = "stable";
            bool enabled = true;
        };

        // The files under `content` whose path matches the pattern, relative to `content`, take the delivery, and with a pack name they go into shards of that pack, which a store delivers on its own, such as the content of a downloadable expansion. A rule may name a preload group instead of a pattern, which `expandGroups` replaces with a rule for each entry of the group in the manifest that `groups` names.
        struct Rule {
            std::string match;
            content::Delivery mode = content::Delivery::Required;
            std::string pack;
            std::string group;
        };

        std::optional<Updates> updates;
        std::vector<Rule> delivery;

        // The group manifest of the rules that name groups, by its path relative to `content`, such as `groups.json`, which the asset manager loads with `assets.defineGroups` too.
        std::string groups;

        // The delivery of an asset by its path relative to `content`: the mode of the first rule that matches it, or required.
        [[nodiscard]] content::Delivery getDelivery(std::string_view path) const noexcept;

        // The pack of an asset by its path relative to `content`: the pack of the first rule that matches it, empty for the content that is no pack of its own.
        [[nodiscard]] std::string_view getPack(std::string_view path) const noexcept;

        // Whether a rule names a preload group that `expandGroups` has not replaced yet, so the deliveries and packs of the rules need the group manifest first.
        [[nodiscard]] bool hasGroupRules() const noexcept;

        // Replaces every rule that names a preload group with a rule for each entry of the group in the group manifest of the package, the path of a file or every file under a folder. The group manifest is content, which a release may not hold on the device yet, so the content manager brings it before it calls this. Throws `std::invalid_argument` for a manifest that the package lacks, a group that it does not define or an entry outside the `content` folder.
        void expandGroups(const io::Package& package);
    };

    // The texts the app ships, from the `localization` object: the folder under `content` whose JSON files load as languages before any script runs, the fallback language among them, and the language the app starts in. Without a language the app follows the languages of the system. The languages are in their canonical form, such as `pt-BR`.
    struct Localization {
        std::string folder;
        std::string fallback;
        std::string language;
    };

    // The name of an app whose `haylen.json` gives none, an empty one or null, which the tooling gives its projects too.
    static constexpr std::string_view kDefaultName = "Haylen App";

    std::string name{kDefaultName};
    std::string identifier = "com.haylen.app";
    std::string version = "1.0.0";

    // The build number of the app, a whole number that grows with every upload to a store, while `version` is the version that people see. It is the bundle version of Apple platforms, the version code of Android and the app build that protected releases and content updates match.
    std::uint32_t build = 1;

    std::string description{kDefaultName};
    std::string company{kDefaultName};
    std::string copyright = std::string("Copyright \xC2\xA9 ").append(kDefaultName);
    std::string website;
    Window window{};
    math::Vec2 designSize{1920.0F, 1080.0F};
    graphics::Viewport::ScalingPolicy scaling = graphics::Viewport::ScalingPolicy::Expand;
    platform::Orientation orientation = platform::Orientation::Landscape;
    double fixedRate = 60.0;
    double maxFrameTime = 0.25;
    math::Color clearColor = math::Color::black();
    Splash splash{};
    Lifecycle lifecycle{};

    // How the app shares sound with the system and other apps, from the `audio` object with `iosSession` and `mixWithOthers`, and how its output plays, with `latency` and `limiter`.
    audio::Session audioSession{};
    audio::OutputSettings audioOutput{};

    Input input{};

    // How large the interface draws, from the `ui` object with `scaleMode` and `scale`.
    ui::Scaling uiScaling{};

    Debug debug{};
    Content content{};
    Localization localization{};

    // The native libraries the app ships by name, which the backend of each target builds and places in the app. The engine keeps the section as `haylen.json` wrote it.
    Json native = Json::object();

    // A plugin that the app uses: where the tooling takes it from, an empty `path` and `git` meaning the folder `plugins/<id>` of the app, and the values of its parameters as `haylen.json` wrote them. The package always holds the plugin at `plugins/<id>`, with its `plugin.json` and its Lua modules, wherever it comes from.
    struct Plugin {
        std::string path;
        std::string git;
        std::string ref;
        Json parameters = Json::object();

        friend bool operator==(const Plugin&, const Plugin&) = default;
    };

    // The plugins the app uses by id.
    std::map<std::string, Plugin> plugins;

    // The sections of `haylen.json` that the application reads itself, such as the `lua` section of a Lua app, by name and as `haylen.json` wrote them.
    std::map<std::string, Json, std::less<>> sections;

    // Reads every present field and validates it, keeping the sections that the application names in `sections`, which the application reads itself. Throws `std::invalid_argument` with the offending field on bad values and on a key that neither the engine nor the application reads.
    [[nodiscard]] static AppConfig fromJson(const Json& document, std::span<const std::string_view> applicationSections = {});

    // Reads `haylen.json` of the package like `fromJson` and also checks that the package holds `plugins/<id>/plugin.json` for every plugin the app lists, which a folder that the player runs directly lacks for a plugin that `haylen.json` references elsewhere. It reads no content, so the rules that name preload groups stay until `Content::expandGroups`.
    [[nodiscard]] static AppConfig fromPackage(const io::Package& package, std::span<const std::string_view> applicationSections = {});

    // Reads only the plugins that `haylen.json` of the package lists, as `fromPackage` checks them, for a runtime that loads their native parts before it creates the application.
    [[nodiscard]] static std::map<std::string, Plugin> readPlugins(const io::Package& package);
    [[nodiscard]] Json toJson() const;

    [[nodiscard]] static std::optional<content::Delivery> deliveryFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view deliveryName(content::Delivery value) noexcept;

    // The largest build number, the largest version code that Google Play takes.
    static constexpr std::uint32_t kMaxBuild = 2'100'000'000;

  private:
    static const std::array<std::string_view, 28> kKeys;
    static const std::array<std::pair<std::string_view, content::Delivery>, 3> kDeliveryNames;
    static const std::array<std::string_view, 10> kCategoryNames;

    template <typename T> static void readValue(const Json& object, std::string_view section, const char* key, T& target);
    [[nodiscard]] static std::string readName(const Json& document);
    [[nodiscard]] static std::uint32_t readBuild(const Json& document);
    static void requirePositive(double value, const char* key);
    static void requireNonNegative(double value, const char* key);
    [[nodiscard]] static math::Color readColor(const Json& object, std::string_view section, const char* key);
    static void readKeys(const Json& document, std::span<const std::string_view> applicationSections, AppConfig& config);
    static void readPlugins(const Json& section, AppConfig& config);
    static void requirePluginManifests(const io::Package& package, const AppConfig& config);
    [[nodiscard]] static Plugin readPlugin(const std::string& id, const Json& entry);
    static void readPluginText(const Json& entry, const std::string& id, const char* key, std::string& target);
    static void requireToolingSections(const Json& document);
    [[nodiscard]] static std::vector<gpu::Device::Api> readGraphicsApis(const Json& value);
    [[nodiscard]] static bool isContentPattern(std::string_view pattern) noexcept;
    static void readWindow(const Json& section, AppConfig& config);
    static void readDesign(const Json& section, AppConfig& config);
    static void readSplash(const Json& section, AppConfig& config);
    static void readLifecycle(const Json& section, AppConfig& config);
    static void readAudio(const Json& section, AppConfig& config);
    static void readInput(const Json& section, AppConfig& config);
    static void readUi(const Json& section, AppConfig& config);
    static void readDebug(const Json& section, AppConfig& config);
    [[nodiscard]] static Content::Updates readUpdates(const Json& section);
    [[nodiscard]] static Content::Rule readDeliveryRule(const Json& section, std::string_view groups);
    static void readContent(const Json& section, AppConfig& config);
    [[nodiscard]] static std::string readLanguageTag(const Json& section, const char* key);
    static void readLocalization(const Json& section, AppConfig& config);
    [[nodiscard]] static bool isServerAddress(std::string_view url) noexcept;
    [[nodiscard]] static bool isPluginId(std::string_view text) noexcept;
    [[nodiscard]] static platform::Orientation orientationFromName(const std::string& text);
    [[nodiscard]] static audio::Session::Category sessionCategoryFromName(const std::string& text);
};

} // namespace haylen::core
