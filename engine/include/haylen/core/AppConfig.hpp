#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/audio/Session.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/debug/StatsDisplay.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "haylen/platform/WindowPlacement.hpp"

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

    // The launch screen that every platform shows while the app starts, with a logo relative to the content folder, or the Haylen logo when it is empty, over a background color that defaults to the clear color. The script `haylen.py` turns it into the launch storyboard on Apple platforms, the splash screen on Android and the loading page on the web.
    struct Splash {
        std::string logo;
        math::Color background = math::Color::black();
    };

    // How the app reacts when it leaves the foreground. Pausing halts updates, so timers, tweens, physics and scenes stand still, while the event loop keeps delivering network replies. An app in the background also stops rendering.
    struct Lifecycle {
        bool pauseOnBackground = true;
        bool pauseOnFocusLoss = false;
        bool muteOnFocusLoss = false;
    };

    // The debug statistics the app starts with, whether counted objects publish `objectCreated` and `objectDestroyed`, the safe area to simulate instead of the one of the device, and whether the safe area shows over the app.
    struct Debug {
        debug::StatsDisplay::Mode stats = debug::StatsDisplay::Mode::Off;
        bool objectEvents = false;
        std::optional<platform::SafeAreaSimulation> safeArea;
        bool showSafeArea = false;
    };

    std::string name = "Haylen App";
    std::string identifier = "dev.haylen.app";
    std::string version = "1.0.0";
    Window window{};
    math::Vec2 designSize{1920.0F, 1080.0F};
    graphics::Viewport::ScalingPolicy scaling = graphics::Viewport::ScalingPolicy::Expand;
    platform::Orientation orientation = platform::Orientation::Landscape;
    double fixedRate = 60.0;
    double maxFrameTime = 0.25;
    math::Color clearColor = math::Color::black();
    Splash splash{};
    Lifecycle lifecycle{};

    // How the app shares sound with the system and other apps, from the `audio` object with `iosSession` and `mixWithOthers`.
    audio::Session audioSession{};
    Debug debug{};

    // Lua modules that load before the first scene and live for the whole app.
    std::vector<std::string> autoloads;

    // The native libraries the app ships by name, which `haylen.py` builds and places in the package of each platform. The engine keeps the section as `app.json` wrote it.
    Json native = Json::object();

    // The plugins the app uses by id, each with the values of its parameters as `app.json` wrote them. Every id names the folder `plugins/<id>` of the package, which holds its `plugin.json` and its Lua modules.
    Json plugins = Json::object();

    // Set by the runtime rather than `app.json`: a package opened from a folder during development reloads when its files change.
    bool hotReload = false;

    // Reads every present field and validates it. Throws `std::invalid_argument` with the offending field on bad values.
    [[nodiscard]] static AppConfig fromJson(const Json& document);

    // Reads `app.json` of the package like `fromJson` and also checks that the package holds `plugins/<id>/plugin.json` for every plugin the app lists.
    [[nodiscard]] static AppConfig fromPackage(const io::Package& package);
    [[nodiscard]] Json toJson() const;

  private:
    template <typename T> static void readValue(const Json& object, const char* key, T& target);
    static void requirePositive(double value, const char* key);
    static void readPlugins(const Json& section, AppConfig& config);
    [[nodiscard]] static bool isPluginId(std::string_view text) noexcept;
    [[nodiscard]] static platform::Orientation orientationFromName(const std::string& text);
    [[nodiscard]] static audio::Session::Category sessionCategoryFromName(const std::string& text);
};

} // namespace haylen::core
