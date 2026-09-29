#include "haylen/core/AppConfig.hpp"

#include <stdexcept>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"

namespace haylen::core {

template <typename T> void AppConfig::readValue(const Json& object, const char* key, T& target) {
    if (!object.contains(key)) {
        return;
    }
    try {
        target = object.at(key).get<T>();
    } catch (const Json::exception&) {
        throw std::invalid_argument(std::string("app.json has an invalid value for '") + key + "'.");
    }
}

void AppConfig::requirePositive(double value, const char* key) {
    if (value <= 0.0) {
        throw std::invalid_argument(std::string("app.json requires a positive value for '") + key + "'.");
    }
}

platform::Orientation AppConfig::orientationFromName(const std::string& text) {
    if (text == "landscape") {
        return platform::Orientation::Landscape;
    }
    if (text == "portrait") {
        return platform::Orientation::Portrait;
    }
    if (text == "any") {
        return platform::Orientation::Any;
    }
    throw std::invalid_argument("app.json has an unknown orientation: " + text);
}

std::string_view AppConfig::orientationName(platform::Orientation value) {
    switch (value) {
    case platform::Orientation::Landscape:
        return "landscape";
    case platform::Orientation::Portrait:
        return "portrait";
    case platform::Orientation::Any:
        return "any";
    }
    return "landscape";
}

std::string_view AppConfig::scalingName(graphics::Viewport::ScalingPolicy policy) {
    switch (policy) {
    case graphics::Viewport::ScalingPolicy::Fit:
        return "fit";
    case graphics::Viewport::ScalingPolicy::Fill:
        return "fill";
    case graphics::Viewport::ScalingPolicy::Stretch:
        return "stretch";
    case graphics::Viewport::ScalingPolicy::Expand:
        return "expand";
    case graphics::Viewport::ScalingPolicy::PixelPerfect:
        return "pixel_perfect";
    }
    return "expand";
}

audio::Session::Category AppConfig::sessionCategoryFromName(const std::string& text) {
    if (text == "ambient") {
        return audio::Session::Category::Ambient;
    }
    if (text == "soloAmbient") {
        return audio::Session::Category::SoloAmbient;
    }
    if (text == "playback") {
        return audio::Session::Category::Playback;
    }
    throw std::invalid_argument("app.json has an unknown audio.iosSession: " + text);
}

std::string_view AppConfig::sessionCategoryName(audio::Session::Category value) {
    switch (value) {
    case audio::Session::Category::Ambient:
        return "ambient";
    case audio::Session::Category::SoloAmbient:
        return "soloAmbient";
    case audio::Session::Category::Playback:
        return "playback";
    }
    return "ambient";
}

AppConfig AppConfig::fromJson(const Json& document) {
    JsonValidator::requireKnownKeys(document, {"name", "identifier", "version", "window", "design", "orientation", "fixedRate", "maxFrameTime", "clearColor", "splash", "lifecycle", "audio", "debug", "autoload"}, "app.json");

    AppConfig config;
    readValue(document, "name", config.name);
    readValue(document, "identifier", config.identifier);
    readValue(document, "version", config.version);
    config.window.title = config.name;

    if (document.contains("window")) {
        const Json& windowJson = document.at("window");
        JsonValidator::requireKnownKeys(windowJson, {"title", "width", "height", "fullscreen", "highDpi", "resizable", "vsync", "sampleCount"}, "app.json window");
        readValue(windowJson, "title", config.window.title);
        readValue(windowJson, "width", config.window.width);
        readValue(windowJson, "height", config.window.height);
        readValue(windowJson, "fullscreen", config.window.fullscreen);
        readValue(windowJson, "highDpi", config.window.highDpi);
        readValue(windowJson, "resizable", config.window.resizable);
        readValue(windowJson, "vsync", config.window.vsync);
        readValue(windowJson, "sampleCount", config.window.sampleCount);
        requirePositive(config.window.width, "window.width");
        requirePositive(config.window.height, "window.height");
        requirePositive(config.window.sampleCount, "window.sampleCount");
    }

    if (document.contains("design")) {
        const Json& designJson = document.at("design");
        JsonValidator::requireKnownKeys(designJson, {"width", "height", "scaling"}, "app.json design");
        readValue(designJson, "width", config.designSize.x);
        readValue(designJson, "height", config.designSize.y);
        requirePositive(config.designSize.x, "design.width");
        requirePositive(config.designSize.y, "design.height");

        std::string scalingText;
        readValue(designJson, "scaling", scalingText);
        if (!scalingText.empty()) {
            const auto policy = graphics::Viewport::scalingPolicyFromName(scalingText);
            if (!policy) {
                throw std::invalid_argument("app.json has an unknown scaling policy: " + scalingText);
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
    if (!clearColorText.empty()) {
        const auto color = math::Color::parse(clearColorText);
        if (!color) {
            throw std::invalid_argument("app.json has an invalid clearColor: " + clearColorText);
        }
        config.clearColor = *color;
    }

    config.splash.background = config.clearColor;
    if (document.contains("splash")) {
        const Json& splashJson = document.at("splash");
        JsonValidator::requireKnownKeys(splashJson, {"logo", "background"}, "app.json splash");
        readValue(splashJson, "logo", config.splash.logo);

        std::string background;
        readValue(splashJson, "background", background);
        if (!background.empty()) {
            const auto color = math::Color::parse(background);
            if (!color) {
                throw std::invalid_argument("app.json has an invalid splash.background: " + background);
            }
            config.splash.background = *color;
        }
    }

    if (document.contains("lifecycle")) {
        const Json& lifecycleJson = document.at("lifecycle");
        JsonValidator::requireKnownKeys(lifecycleJson, {"pauseOnBackground", "pauseOnFocusLoss", "muteOnFocusLoss"}, "app.json lifecycle");
        readValue(lifecycleJson, "pauseOnBackground", config.lifecycle.pauseOnBackground);
        readValue(lifecycleJson, "pauseOnFocusLoss", config.lifecycle.pauseOnFocusLoss);
        readValue(lifecycleJson, "muteOnFocusLoss", config.lifecycle.muteOnFocusLoss);
    }

    if (document.contains("audio")) {
        const Json& audioJson = document.at("audio");
        JsonValidator::requireKnownKeys(audioJson, {"iosSession", "mixWithOthers"}, "app.json audio");
        std::string category;
        readValue(audioJson, "iosSession", category);
        if (!category.empty()) {
            config.audioSession.category = sessionCategoryFromName(category);
        }
        readValue(audioJson, "mixWithOthers", config.audioSession.mixWithOthers);
        if (config.audioSession.mixWithOthers && config.audioSession.category != audio::Session::Category::Playback) {
            throw std::invalid_argument("app.json audio.mixWithOthers needs the playback iosSession.");
        }
    }

    if (document.contains("debug")) {
        const Json& debugJson = document.at("debug");
        JsonValidator::requireKnownKeys(debugJson, {"stats", "objectEvents", "safeArea", "showSafeArea"}, "app.json debug");
        std::string stats;
        readValue(debugJson, "stats", stats);
        if (!stats.empty()) {
            const auto mode = debug::StatsDisplay::modeFromName(stats);
            if (!mode) {
                throw std::invalid_argument("app.json has an unknown debug.stats: " + stats + ". It is off, compact or full.");
            }
            config.debug.stats = *mode;
        }
        readValue(debugJson, "objectEvents", config.debug.objectEvents);
        if (debugJson.contains("safeArea")) {
            try {
                config.debug.safeArea = platform::SafeAreaSimulation::fromJson(debugJson.at("safeArea"));
            } catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::string("app.json has an invalid debug.safeArea. ") + error.what());
            }
        }
        readValue(debugJson, "showSafeArea", config.debug.showSafeArea);
    }

    readValue(document, "autoload", config.autoloads);
    for (const std::string& module : config.autoloads) {
        if (module.empty()) {
            throw std::invalid_argument("app.json has an empty module name in 'autoload'.");
        }
    }
    return config;
}

Json AppConfig::toJson() const {
    Json debugJson = {{"stats", debug::StatsDisplay::modeName(debug.stats)}, {"objectEvents", debug.objectEvents}, {"showSafeArea", debug.showSafeArea}};
    if (debug.safeArea) {
        debugJson["safeArea"] = debug.safeArea->toJson();
    }
    return {
        {"name", name}, {"identifier", identifier}, {"version", version}, {"window", {{"title", window.title}, {"width", window.width}, {"height", window.height}, {"fullscreen", window.fullscreen}, {"highDpi", window.highDpi}, {"resizable", window.resizable}, {"vsync", window.vsync}, {"sampleCount", window.sampleCount}}}, {"design", {{"width", JsonNumber::fromFloat(designSize.x)}, {"height", JsonNumber::fromFloat(designSize.y)}, {"scaling", scalingName(scaling)}}}, {"orientation", orientationName(orientation)}, {"fixedRate", fixedRate}, {"maxFrameTime", maxFrameTime}, {"clearColor", clearColor.toHex()}, {"splash", {{"logo", splash.logo}, {"background", splash.background.toHex()}}}, {"lifecycle", {{"pauseOnBackground", lifecycle.pauseOnBackground}, {"pauseOnFocusLoss", lifecycle.pauseOnFocusLoss}, {"muteOnFocusLoss", lifecycle.muteOnFocusLoss}}}, {"audio", {{"iosSession", sessionCategoryName(audioSession.category)}, {"mixWithOthers", audioSession.mixWithOthers}}}, {"debug", debugJson}, {"autoload", autoloads},
    };
}

} // namespace haylen::core
