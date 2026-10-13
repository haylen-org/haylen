#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/audio/OutputSettings.hpp"
#include "haylen/audio/Session.hpp"
#include "haylen/content/Delivery.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/debug/StatsDisplay.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/FoldSimulation.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "haylen/platform/WindowPlacement.hpp"
#include "haylen/ui/Scaling.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::core {

// App settings read from `app.json` before any script runs, because the window exists before `source/main.lua`.
struct AppConfig {
    // A transparent window opens able to let the desktop show through its transparent pixels, which it keeps for the whole run, and clears to transparent unless `clearColor` says otherwise. The desktop options change at run time through the window, and `position` places the window when it opens.
    struct Window {
        std::string title = "Haylen";
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        bool highDpi = true;
        bool resizable = true;
        bool vsync = true;
        int sampleCount = 1;
        bool decorated = true;
        bool transparent = false;
        bool alwaysOnTop = false;
        bool showInTaskbar = true;
        bool focusable = true;
        bool mousePassthrough = false;
        std::optional<platform::WindowPlacement> position;
    };

    // The launch screen that every platform shows while the app starts, with a logo relative to the content folder, or the Haylen logo when it is empty, over a background color that defaults to the clear color, and another one while the system uses dark colors. It stays until the app drew its first frame and at least the duration in seconds since the launch, and fades out into the app over the fade-out in seconds. The script `haylen.py` turns it into the launch storyboard on Apple platforms, the splash screen on Android and the loading page on the web.
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

    // The debug statistics and the debug drawings the app starts with, whether counted objects publish `objectCreated` and `objectDestroyed`, whether the developer console works outside development, the safe area to simulate instead of the one of the device, whether the safe area shows over the app, and how a changed Lua module applies while the app runs in development.
    struct Debug {
        // A changed module reloads in place with the state of the app, or restarts the app. Changes that cannot apply in place restart it either way.
        enum class Reload : std::uint8_t {
            Module,
            Restart,
        };

        debug::StatsDisplay::Mode stats = debug::StatsDisplay::Mode::Off;
        std::vector<std::string> drawings;
        bool objectEvents = false;
        bool console = false;
        std::optional<platform::SafeAreaSimulation> safeArea;
        std::optional<platform::FoldSimulation> fold;
        bool showSafeArea = false;
        Reload reload = Reload::Module;
    };

    // How the content of a protected release reaches the device, from the `content` object: the server of its updates and downloads, and the files that need not be on the device before the app starts.
    struct Content {
        // The publication tree that serves the updates of the content and the files that download, with its address ending in a slash and the update channel. While updates are off, the server still serves the files that download.
        struct Updates {
            std::string baseUrl;
            std::string channel = "stable";
            bool enabled = true;
        };

        // The files under `content` whose path matches the pattern, relative to `content`, take the delivery, and with a pack name they go into shards of that pack, which a store delivers on its own, such as the content of a downloadable expansion.
        struct Rule {
            std::string match;
            content::Delivery mode = content::Delivery::Required;
            std::string pack;
        };

        std::optional<Updates> updates;
        std::vector<Rule> delivery;

        // The delivery of an asset by its path relative to `content`: the mode of the first rule that matches it, or required.
        [[nodiscard]] content::Delivery getDelivery(std::string_view path) const noexcept;

        // The pack of an asset by its path relative to `content`: the pack of the first rule that matches it, empty for the content that is no pack of its own.
        [[nodiscard]] std::string_view getPack(std::string_view path) const noexcept;
    };

    std::string name = "Haylen App";
    std::string identifier = "dev.haylen.app";
    std::string version = "1.0.0";
    std::string description = "Haylen App";
    std::string company = "Haylen App";
    std::string copyright = "Copyright \xC2\xA9 Haylen App";
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

    // Lua modules that load before the first scene and live for the whole app.
    std::vector<std::string> autoloads;

    // The native libraries the app ships by name, which `haylen.py` builds and places in the package of each platform. The engine keeps the section as `app.json` wrote it.
    Json native = Json::object();

    // The plugins the app uses by id, each with the values of its parameters as `app.json` wrote them. Every id names the folder `plugins/<id>` of the package, which holds its `plugin.json` and its Lua modules.
    Json plugins = Json::object();

    // Reads every present field and validates it. Throws `std::invalid_argument` with the offending field on bad values.
    [[nodiscard]] static AppConfig fromJson(const Json& document);

    // Reads `app.json` of the package like `fromJson` and also checks that the package holds `plugins/<id>/plugin.json` for every plugin the app lists.
    [[nodiscard]] static AppConfig fromPackage(const io::Package& package);
    [[nodiscard]] Json toJson() const;

    [[nodiscard]] static std::optional<Debug::Reload> reloadFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view reloadName(Debug::Reload value) noexcept;
    [[nodiscard]] static std::optional<content::Delivery> deliveryFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view deliveryName(content::Delivery value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Debug::Reload>, 2> kReloadNames;
    static const std::array<std::pair<std::string_view, content::Delivery>, 3> kDeliveryNames;

    template <typename T> static void readValue(const Json& object, const char* key, T& target);
    static void requirePositive(double value, const char* key);
    static void requireNonNegative(double value, const char* key);
    [[nodiscard]] static math::Color readColor(const Json& object, const char* key, const char* name);
    static void readPlugins(const Json& section, AppConfig& config);
    static void readContent(const Json& section, AppConfig& config);
    [[nodiscard]] static bool isServerAddress(std::string_view url) noexcept;
    [[nodiscard]] static bool isPluginId(std::string_view text) noexcept;
    [[nodiscard]] static platform::Orientation orientationFromName(const std::string& text);
    [[nodiscard]] static audio::Session::Category sessionCategoryFromName(const std::string& text);
};

} // namespace haylen::core
