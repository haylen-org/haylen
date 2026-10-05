#include "haylen/core/AppConfig.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::core {

template <typename T> void AppConfig::readValue(const Json& object, const char* key, T& target) {
    if (!object.contains(key)) {
        return;
    }
    try {
        target = object.at(key).get<T>();
    } catch (const Json::exception&) {
        throw std::invalid_argument(std::string("The value of \"") + key + "\" in \"app.json\" has the wrong type.");
    }
}

void AppConfig::requirePositive(double value, const char* key) {
    if (value <= 0.0) {
        throw std::invalid_argument(std::string("The value of \"") + key + "\" in \"app.json\" must be positive.");
    }
}

platform::Orientation AppConfig::orientationFromName(const std::string& text) {
    const std::optional<platform::Orientation> orientation = platform::Window::orientationFromName(text);
    if (!orientation) {
        throw std::invalid_argument("The orientation \"" + text + "\" in \"app.json\" is unknown. It is \"landscape\", \"portrait\" or \"any\".");
    }
    return *orientation;
}

audio::Session::Category AppConfig::sessionCategoryFromName(const std::string& text) {
    const std::optional<audio::Session::Category> category = audio::Session::categoryFromName(text);
    if (!category) {
        throw std::invalid_argument("The \"audio.iosSession\" value \"" + text + "\" in \"app.json\" is unknown. It is \"ambient\", \"soloAmbient\" or \"playback\".");
    }
    return *category;
}

// Plugin ids are `dash-case`, like the folders of the plugins: lowercase words of letters and digits joined by single dashes, starting with a letter.
bool AppConfig::isPluginId(std::string_view text) noexcept {
    if (text.empty() || text.front() < 'a' || text.front() > 'z' || text.back() == '-') {
        return false;
    }
    char previous = '\0';
    for (const char character : text) {
        const bool word = (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9');
        if (!word && (character != '-' || previous == '-')) {
            return false;
        }
        previous = character;
    }
    return true;
}

void AppConfig::readPlugins(const Json& section, AppConfig& config) {
    if (!section.is_object()) {
        throw std::invalid_argument("The \"plugins\" section of \"app.json\" must be an object of plugins by id.");
    }
    for (const auto& [id, values] : section.items()) {
        if (!isPluginId(id)) {
            throw std::invalid_argument("The plugin id \"" + id + "\" in \"app.json\" is not in \"dash-case\", such as \"firebase-analytics\".");
        }
        if (!values.is_object()) {
            throw std::invalid_argument("The plugin \"" + id + "\" in \"app.json\" must have an object of parameter values.");
        }
    }
    config.plugins = section;
}

AppConfig AppConfig::fromPackage(const io::Package& package) {
    AppConfig config = fromJson(Json::parse(package.readText(io::Path::kAppConfigFile)));
    for (const auto& [id, values] : config.plugins.items()) {
        const std::string manifest = io::Path::plugin(id, io::Path::kPluginManifestFile);
        if (!package.exists(manifest)) {
            throw std::invalid_argument("The plugin \"" + id + "\" in \"app.json\" has no \"" + manifest + "\" in the package.");
        }
    }
    return config;
}

AppConfig AppConfig::fromJson(const Json& document) {
    JsonValidator::requireKnownKeys(document, {"name", "identifier", "version", "window", "design", "orientation", "fixedRate", "maxFrameTime", "clearColor", "splash", "lifecycle", "audio", "input", "ui", "debug", "autoload", "native", "plugins"}, "\"app.json\"");

    AppConfig config;
    readValue(document, "name", config.name);
    readValue(document, "identifier", config.identifier);
    readValue(document, "version", config.version);
    config.window.title = config.name;

    if (document.contains("window")) {
        const Json& windowJson = document.at("window");
        JsonValidator::requireKnownKeys(windowJson, {"title", "width", "height", "fullscreen", "highDpi", "resizable", "vsync", "sampleCount", "decorated", "transparent", "alwaysOnTop", "showInTaskbar", "focusable", "mousePassthrough", "position"}, "the \"window\" section of \"app.json\"");
        readValue(windowJson, "title", config.window.title);
        readValue(windowJson, "width", config.window.width);
        readValue(windowJson, "height", config.window.height);
        readValue(windowJson, "fullscreen", config.window.fullscreen);
        readValue(windowJson, "highDpi", config.window.highDpi);
        readValue(windowJson, "resizable", config.window.resizable);
        readValue(windowJson, "vsync", config.window.vsync);
        readValue(windowJson, "sampleCount", config.window.sampleCount);
        readValue(windowJson, "decorated", config.window.decorated);
        readValue(windowJson, "transparent", config.window.transparent);
        readValue(windowJson, "alwaysOnTop", config.window.alwaysOnTop);
        readValue(windowJson, "showInTaskbar", config.window.showInTaskbar);
        readValue(windowJson, "focusable", config.window.focusable);
        readValue(windowJson, "mousePassthrough", config.window.mousePassthrough);
        requirePositive(config.window.width, "window.width");
        requirePositive(config.window.height, "window.height");
        requirePositive(config.window.sampleCount, "window.sampleCount");
        if (windowJson.contains("position")) {
            try {
                config.window.position = platform::WindowPlacement::fromJson(windowJson.at("position"));
            } catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::string("The \"window.position\" in \"app.json\" is invalid. ") + error.what());
            }
        }
    }

    if (document.contains("design")) {
        const Json& designJson = document.at("design");
        JsonValidator::requireKnownKeys(designJson, {"width", "height", "scaling"}, "the \"design\" section of \"app.json\"");
        readValue(designJson, "width", config.designSize.x);
        readValue(designJson, "height", config.designSize.y);
        requirePositive(config.designSize.x, "design.width");
        requirePositive(config.designSize.y, "design.height");

        std::string scalingText;
        readValue(designJson, "scaling", scalingText);
        if (!scalingText.empty()) {
            const auto policy = graphics::Viewport::scalingPolicyFromName(scalingText);
            if (!policy) {
                throw std::invalid_argument("The \"design.scaling\" value \"" + scalingText + "\" in \"app.json\" is unknown. It is \"fit\", \"fill\", \"stretch\", \"expand\", \"pixelPerfect\", \"fitWidth\" or \"none\".");
            }
            config.scaling = *policy;
        }
    }

    std::string orientationText;
    readValue(document, "orientation", orientationText);
    if (!orientationText.empty()) {
        config.orientation = orientationFromName(orientationText);
    }

    readValue(document, "fixedRate", config.fixedRate);
    readValue(document, "maxFrameTime", config.maxFrameTime);
    requirePositive(config.fixedRate, "fixedRate");
    requirePositive(config.maxFrameTime, "maxFrameTime");

    std::string clearColorText;
    readValue(document, "clearColor", clearColorText);
    if (config.window.transparent) {
        config.clearColor = math::Color::transparent();
    }
    if (!clearColorText.empty()) {
        const auto color = math::Color::parse(clearColorText);
        if (!color) {
            throw std::invalid_argument("The \"clearColor\" value \"" + clearColorText + "\" in \"app.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
        }
        config.clearColor = *color;
    }

    config.splash.background = config.clearColor;
    if (document.contains("splash")) {
        const Json& splashJson = document.at("splash");
        JsonValidator::requireKnownKeys(splashJson, {"logo", "background"}, "the \"splash\" section of \"app.json\"");
        readValue(splashJson, "logo", config.splash.logo);

        std::string background;
        readValue(splashJson, "background", background);
        if (!background.empty()) {
            const auto color = math::Color::parse(background);
            if (!color) {
                throw std::invalid_argument("The \"splash.background\" value \"" + background + "\" in \"app.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
            }
            config.splash.background = *color;
        }
    }

    if (document.contains("lifecycle")) {
        const Json& lifecycleJson = document.at("lifecycle");
        JsonValidator::requireKnownKeys(lifecycleJson, {"pauseOnBackground", "pauseOnFocusLoss", "muteOnFocusLoss"}, "the \"lifecycle\" section of \"app.json\"");
        readValue(lifecycleJson, "pauseOnBackground", config.lifecycle.pauseOnBackground);
        readValue(lifecycleJson, "pauseOnFocusLoss", config.lifecycle.pauseOnFocusLoss);
        readValue(lifecycleJson, "muteOnFocusLoss", config.lifecycle.muteOnFocusLoss);
    }

    if (document.contains("audio")) {
        const Json& audioJson = document.at("audio");
        JsonValidator::requireKnownKeys(audioJson, {"iosSession", "mixWithOthers"}, "the \"audio\" section of \"app.json\"");
        std::string category;
        readValue(audioJson, "iosSession", category);
        if (!category.empty()) {
            config.audioSession.category = sessionCategoryFromName(category);
        }
        readValue(audioJson, "mixWithOthers", config.audioSession.mixWithOthers);
        if (config.audioSession.mixWithOthers && config.audioSession.category != audio::Session::Category::Playback) {
            throw std::invalid_argument("The \"audio.mixWithOthers\" option in \"app.json\" needs the \"iosSession\" value \"playback\".");
        }
    }

    if (document.contains("input")) {
        const Json& inputJson = document.at("input");
        JsonValidator::requireKnownKeys(inputJson, {"mouseAsTouch", "touchAsMouse"}, "the \"input\" section of \"app.json\"");
        readValue(inputJson, "mouseAsTouch", config.input.mouseAsTouch);
        readValue(inputJson, "touchAsMouse", config.input.touchAsMouse);
    }

    if (document.contains("ui")) {
        const Json& uiJson = document.at("ui");
        JsonValidator::requireKnownKeys(uiJson, {"scaleMode", "scale"}, "the \"ui\" section of \"app.json\"");
        std::string mode;
        readValue(uiJson, "scaleMode", mode);
        if (!mode.empty()) {
            const std::optional<ui::Scaling::Mode> found = ui::Scaling::modeFromName(mode);
            if (!found) {
                throw std::invalid_argument("The \"ui.scaleMode\" value \"" + mode + "\" in \"app.json\" is unknown. It is \"design\" or \"physical\".");
            }
            config.uiScaling.mode = *found;
        }
        readValue(uiJson, "scale", config.uiScaling.factor);
        if (config.uiScaling.factor < ui::Scaling::kMinimumFactor || config.uiScaling.factor > ui::Scaling::kMaximumFactor) {
            throw std::invalid_argument("The \"ui.scale\" value in \"app.json\" must be from 0.25 to 4.");
        }
    }

    if (document.contains("debug")) {
        const Json& debugJson = document.at("debug");
        JsonValidator::requireKnownKeys(debugJson, {"stats", "drawings", "objectEvents", "safeArea", "showSafeArea"}, "the \"debug\" section of \"app.json\"");
        std::string stats;
        readValue(debugJson, "stats", stats);
        if (!stats.empty()) {
            const auto mode = debug::StatsDisplay::modeFromName(stats);
            if (!mode) {
                throw std::invalid_argument("The \"debug.stats\" value \"" + stats + "\" in \"app.json\" is unknown. It is \"off\", \"compact\" or \"full\".");
            }
            config.debug.stats = *mode;
        }
        readValue(debugJson, "drawings", config.debug.drawings);
        if (std::ranges::any_of(config.debug.drawings, [](const std::string& drawing) { return drawing.empty(); })) {
            throw std::invalid_argument("The \"debug.drawings\" list in \"app.json\" has an empty drawing name.");
        }
        readValue(debugJson, "objectEvents", config.debug.objectEvents);
        if (debugJson.contains("safeArea")) {
            try {
                config.debug.safeArea = platform::SafeAreaSimulation::fromJson(debugJson.at("safeArea"));
            } catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::string("The \"debug.safeArea\" in \"app.json\" is invalid. ") + error.what());
            }
        }
        readValue(debugJson, "showSafeArea", config.debug.showSafeArea);
    }

    readValue(document, "autoload", config.autoloads);
    for (const std::string& module : config.autoloads) {
        if (module.empty()) {
            throw std::invalid_argument("The \"autoload\" list in \"app.json\" has an empty module name.");
        }
    }

    if (document.contains("native")) {
        if (!document.at("native").is_object()) {
            throw std::invalid_argument("The \"native\" section of \"app.json\" must be an object of libraries.");
        }
        config.native = document.at("native");
    }

    if (document.contains("plugins")) {
        readPlugins(document.at("plugins"), config);
    }
    return config;
}

Json AppConfig::toJson() const {
    Json debugJson = {{"stats", debug::StatsDisplay::modeName(debug.stats)}, {"drawings", debug.drawings}, {"objectEvents", debug.objectEvents}, {"showSafeArea", debug.showSafeArea}};
    if (debug.safeArea) {
        debugJson["safeArea"] = debug.safeArea->toJson();
    }
    Json windowJson = {{"title", window.title}, {"width", window.width}, {"height", window.height}, {"fullscreen", window.fullscreen}, {"highDpi", window.highDpi}, {"resizable", window.resizable}, {"vsync", window.vsync}, {"sampleCount", window.sampleCount}, {"decorated", window.decorated}, {"transparent", window.transparent}, {"alwaysOnTop", window.alwaysOnTop}, {"showInTaskbar", window.showInTaskbar}, {"focusable", window.focusable}, {"mousePassthrough", window.mousePassthrough}};
    if (window.position) {
        windowJson["position"] = window.position->toJson();
    }
    return {
        {"name", name}, {"identifier", identifier}, {"version", version}, {"window", windowJson}, {"design", {{"width", JsonNumber::fromFloat(designSize.x)}, {"height", JsonNumber::fromFloat(designSize.y)}, {"scaling", graphics::Viewport::scalingPolicyName(scaling)}}}, {"orientation", platform::Window::orientationName(orientation)}, {"fixedRate", fixedRate}, {"maxFrameTime", maxFrameTime}, {"clearColor", clearColor.toHex()}, {"splash", {{"logo", splash.logo}, {"background", splash.background.toHex()}}}, {"lifecycle", {{"pauseOnBackground", lifecycle.pauseOnBackground}, {"pauseOnFocusLoss", lifecycle.pauseOnFocusLoss}, {"muteOnFocusLoss", lifecycle.muteOnFocusLoss}}}, {"audio", {{"iosSession", audio::Session::categoryName(audioSession.category)}, {"mixWithOthers", audioSession.mixWithOthers}}}, {"input", {{"mouseAsTouch", input.mouseAsTouch}, {"touchAsMouse", input.touchAsMouse}}}, {"ui", {{"scaleMode", std::string(ui::Scaling::modeName(uiScaling.mode))}, {"scale", JsonNumber::fromFloat(uiScaling.factor)}}}, {"debug", debugJson}, {"autoload", autoloads}, {"native", native}, {"plugins", plugins},
    };
}

} // namespace haylen::core
