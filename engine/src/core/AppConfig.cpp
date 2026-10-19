#include "haylen/core/AppConfig.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>

#include "content/format/Manifest.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/Window.hpp"

namespace haylen::core {

const std::array<std::pair<std::string_view, AppConfig::Debug::Reload>, 2> AppConfig::kReloadNames{{{"module", Debug::Reload::Module}, {"restart", Debug::Reload::Restart}}};

const std::array<std::pair<std::string_view, content::Delivery>, 3> AppConfig::kDeliveryNames{{{"required", content::Delivery::Required}, {"prefetch", content::Delivery::Prefetch}, {"onDemand", content::Delivery::OnDemand}}};

std::optional<content::Delivery> AppConfig::deliveryFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kDeliveryNames, name, &std::pair<std::string_view, content::Delivery>::first);
    return found != kDeliveryNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view AppConfig::deliveryName(content::Delivery value) noexcept {
    return std::ranges::find(kDeliveryNames, value, &std::pair<std::string_view, content::Delivery>::second)->first;
}

content::Delivery AppConfig::Content::getDelivery(std::string_view path) const noexcept {
    for (const Rule& rule : delivery) {
        if (io::Path::matches(rule.match, path)) {
            return rule.mode;
        }
    }
    return content::Delivery::Required;
}

std::string_view AppConfig::Content::getPack(std::string_view path) const noexcept {
    for (const Rule& rule : delivery) {
        if (io::Path::matches(rule.match, path)) {
            return rule.pack;
        }
    }
    return {};
}

// A server of content is reached over HTTPS, or over plain HTTP only on this machine, which serves releases while they are tested.
bool AppConfig::isServerAddress(std::string_view url) noexcept {
    if (!url.ends_with('/')) {
        return false;
    }
    if (url.starts_with("https://") && url.size() > 9) {
        return true;
    }
    for (const std::string_view local : {"http://localhost", "http://127.0.0.1", "http://[::1]"}) {
        if (url.starts_with(local) && (url.size() == local.size() || url[local.size()] == ':' || url[local.size()] == '/')) {
            return true;
        }
    }
    return false;
}

void AppConfig::readContent(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"updates", "delivery"}, "the \"content\" section of \"app.json\"");
    if (section.contains("updates")) {
        const Json& updates = section.at("updates");
        JsonValidator::requireKnownKeys(updates, {"baseUrl", "channel", "enabled"}, "the \"content.updates\" section of \"app.json\"");
        Content::Updates read;
        readValue(updates, "baseUrl", read.baseUrl);
        readValue(updates, "channel", read.channel);
        readValue(updates, "enabled", read.enabled);
        if (!isServerAddress(read.baseUrl)) {
            throw std::invalid_argument("The \"content.updates.baseUrl\" in \"app.json\" must be an \"https\" address that ends with a slash, such as \"https://cdn.example.com/my-game/\", or an \"http\" address of this machine, not \"" + read.baseUrl + "\".");
        }
        if (!content::Manifest::isValidName(read.channel)) {
            throw std::invalid_argument("The \"content.updates.channel\" in \"app.json\" must have at most 64 lowercase letters, digits, dashes and dots, not \"" + read.channel + "\".");
        }
        config.content.updates = std::move(read);
    }
    if (section.contains("delivery")) {
        const Json& rules = section.at("delivery");
        if (!rules.is_array()) {
            throw std::invalid_argument("The \"content.delivery\" section of \"app.json\" must be a list of rules with \"match\" and a \"mode\" or a \"pack\".");
        }
        for (const Json& rule : rules) {
            JsonValidator::requireKnownKeys(rule, {"match", "mode", "pack"}, "a rule of \"content.delivery\" in \"app.json\"");
            Content::Rule read;
            std::string mode = "required";
            readValue(rule, "match", read.match);
            readValue(rule, "mode", mode);
            readValue(rule, "pack", read.pack);
            if (rule.contains("mode") == rule.contains("pack")) {
                throw std::invalid_argument("A rule of \"content.delivery\" in \"app.json\" needs either a \"mode\" or a \"pack\", since a store delivers a pack on its own.");
            }
            if (rule.contains("pack") && !content::Manifest::isValidName(read.pack)) {
                throw std::invalid_argument("The \"pack\" of a rule of \"content.delivery\" in \"app.json\" must have at most 64 lowercase letters, digits, dashes and dots, not \"" + read.pack + "\".");
            }
            const std::optional<content::Delivery> delivery = deliveryFromName(mode);
            if (read.match.empty() || read.match.starts_with('/') || read.match.find("..") != std::string::npos || read.match.find('\\') != std::string::npos) {
                throw std::invalid_argument("A rule of \"content.delivery\" in \"app.json\" must match paths relative to the \"content\" folder, such as \"world/asia/**\", not \"" + read.match + "\".");
            }
            if (!delivery) {
                throw std::invalid_argument("The \"mode\" of a rule of \"content.delivery\" in \"app.json\" must be \"required\", \"prefetch\" or \"onDemand\", not \"" + mode + "\".");
            }
            read.mode = *delivery;
            config.content.delivery.push_back(std::move(read));
        }
    }
}

std::optional<AppConfig::Debug::Reload> AppConfig::reloadFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kReloadNames, name, &std::pair<std::string_view, Debug::Reload>::first);
    return found != kReloadNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view AppConfig::reloadName(Debug::Reload value) noexcept {
    return std::ranges::find(kReloadNames, value, &std::pair<std::string_view, Debug::Reload>::second)->first;
}

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

void AppConfig::requireNonNegative(double value, const char* key) {
    if (!(value >= 0.0) || !std::isfinite(value)) {
        throw std::invalid_argument(std::string("The value of \"") + key + "\" in \"app.json\" must be a number of at least 0.");
    }
}

math::Color AppConfig::readColor(const Json& object, const char* key, const char* name) {
    std::string text;
    readValue(object, key, text);
    const auto color = math::Color::parse(text);
    if (!color) {
        throw std::invalid_argument(std::string("The \"") + name + "\" value \"" + text + "\" in \"app.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
    }
    return *color;
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
            throw std::invalid_argument("The plugin id \"" + id + "\" in \"app.json\" is not in \"dash-case\", such as \"cloud-analytics\".");
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
    JsonValidator::requireKnownKeys(document, {"name", "identifier", "version", "description", "company", "copyright", "website", "window", "design", "orientation", "fixedRate", "maxFrameTime", "clearColor", "splash", "lifecycle", "audio", "input", "ui", "debug", "content", "autoload", "native", "plugins"}, "\"app.json\"");

    AppConfig config;
    readValue(document, "name", config.name);
    readValue(document, "identifier", config.identifier);
    readValue(document, "version", config.version);
    config.description = config.name;
    config.company = config.name;
    readValue(document, "description", config.description);
    readValue(document, "company", config.company);
    config.copyright = "Copyright \xC2\xA9 " + config.company;
    readValue(document, "copyright", config.copyright);
    readValue(document, "website", config.website);
    config.window.title = config.name;

    if (document.contains("window")) {
        const Json& windowJson = document.at("window");
        JsonValidator::requireKnownKeys(windowJson, {"title", "width", "height", "fullscreen", "highDpi", "resizable", "vsync", "sampleCount", "decorated", "transparent", "alwaysOnTop", "showInTaskbar", "focusable", "mousePassthrough", "position", "statusBar", "statusBarColor"}, "the \"window\" section of \"app.json\"");
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
        std::string statusBarText;
        readValue(windowJson, "statusBar", statusBarText);
        if (!statusBarText.empty()) {
            const std::optional<platform::Window::StatusBar> statusBar = platform::Window::statusBarFromName(statusBarText);
            if (!statusBar) {
                throw std::invalid_argument("The \"window.statusBar\" value \"" + statusBarText + "\" in \"app.json\" is unknown. It is \"hidden\", \"auto\", \"light\" or \"dark\".");
            }
            config.window.statusBar = *statusBar;
        }
        std::string statusBarColorText;
        readValue(windowJson, "statusBarColor", statusBarColorText);
        if (!statusBarColorText.empty()) {
            config.window.statusBarColor = math::Color::parse(statusBarColorText);
            if (!config.window.statusBarColor) {
                throw std::invalid_argument("The \"window.statusBarColor\" value \"" + statusBarColorText + "\" in \"app.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
            }
        }
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
        JsonValidator::requireKnownKeys(splashJson, {"logo", "background", "darkBackground", "duration", "fadeOut"}, "the \"splash\" section of \"app.json\"");
        readValue(splashJson, "logo", config.splash.logo);
        if (splashJson.contains("background")) {
            config.splash.background = readColor(splashJson, "background", "splash.background");
        }
        if (splashJson.contains("darkBackground")) {
            config.splash.darkBackground = readColor(splashJson, "darkBackground", "splash.darkBackground");
        }
        readValue(splashJson, "duration", config.splash.duration);
        requireNonNegative(config.splash.duration, "splash.duration");
        readValue(splashJson, "fadeOut", config.splash.fadeOut);
        requireNonNegative(config.splash.fadeOut, "splash.fadeOut");
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
        JsonValidator::requireKnownKeys(audioJson, {"iosSession", "mixWithOthers", "latency", "limiter"}, "the \"audio\" section of \"app.json\"");
        if (audioJson.contains("latency")) {
            std::string latency;
            readValue(audioJson, "latency", latency);
            const std::optional<audio::OutputSettings::Latency> found = audio::OutputSettings::latencyFromName(latency);
            if (!found) {
                throw std::invalid_argument("The \"audio.latency\" value \"" + latency + "\" in \"app.json\" is unknown. It is \"low\", \"balanced\" or \"powerSaving\".");
            }
            config.audioOutput.latency = *found;
        }
        readValue(audioJson, "limiter", config.audioOutput.limiter);
        if (audioJson.contains("iosSession")) {
            std::string category;
            readValue(audioJson, "iosSession", category);
            config.audioSession.category = sessionCategoryFromName(category);
        }
        readValue(audioJson, "mixWithOthers", config.audioSession.mixWithOthers);
        if (config.audioSession.mixWithOthers && config.audioSession.category != audio::Session::Category::Playback) {
            throw std::invalid_argument("The \"audio.mixWithOthers\" option in \"app.json\" needs the \"iosSession\" value \"playback\".");
        }
    }

    if (document.contains("input")) {
        const Json& inputJson = document.at("input");
        JsonValidator::requireKnownKeys(inputJson, {"mouseAsTouch", "touchAsMouse", "scrollSpeed"}, "the \"input\" section of \"app.json\"");
        readValue(inputJson, "mouseAsTouch", config.input.mouseAsTouch);
        readValue(inputJson, "touchAsMouse", config.input.touchAsMouse);
        readValue(inputJson, "scrollSpeed", config.input.scrollSpeed);
        if (!(config.input.scrollSpeed > 0.0F && std::isfinite(config.input.scrollSpeed))) {
            throw std::invalid_argument("The \"input.scrollSpeed\" value in \"app.json\" must be a positive number.");
        }
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
        JsonValidator::requireKnownKeys(debugJson, {"stats", "drawings", "objectEvents", "console", "safeArea", "fold", "showSafeArea", "reload"}, "the \"debug\" section of \"app.json\"");
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
        readValue(debugJson, "console", config.debug.console);
        if (debugJson.contains("safeArea")) {
            try {
                config.debug.safeArea = platform::SafeAreaSimulation::fromJson(debugJson.at("safeArea"));
            } catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::string("The \"debug.safeArea\" in \"app.json\" is invalid. ") + error.what());
            }
        }
        if (debugJson.contains("fold")) {
            try {
                config.debug.fold = platform::FoldSimulation::fromJson(debugJson.at("fold"));
            } catch (const std::invalid_argument& error) {
                throw std::invalid_argument(std::string("The \"debug.fold\" in \"app.json\" is invalid. ") + error.what());
            }
        }
        readValue(debugJson, "showSafeArea", config.debug.showSafeArea);
        if (debugJson.contains("reload")) {
            std::string reload;
            readValue(debugJson, "reload", reload);
            const auto mode = reloadFromName(reload);
            if (!mode) {
                throw std::invalid_argument("The \"debug.reload\" option must be \"module\" or \"restart\".");
            }
            config.debug.reload = *mode;
        }
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
    if (document.contains("content")) {
        readContent(document.at("content"), config);
    }
    return config;
}

Json AppConfig::toJson() const {
    Json debugJson = {{"stats", debug::StatsDisplay::modeName(debug.stats)}, {"drawings", debug.drawings}, {"objectEvents", debug.objectEvents}, {"console", debug.console}, {"showSafeArea", debug.showSafeArea}, {"reload", reloadName(debug.reload)}};
    if (debug.safeArea) {
        debugJson["safeArea"] = debug.safeArea->toJson();
    }
    if (debug.fold) {
        debugJson["fold"] = debug.fold->toJson();
    }
    Json splashJson = {{"logo", splash.logo}, {"background", splash.background.toHex()}, {"duration", JsonNumber::fromFloat(splash.duration)}, {"fadeOut", JsonNumber::fromFloat(splash.fadeOut)}};
    if (splash.darkBackground) {
        splashJson["darkBackground"] = splash.darkBackground->toHex();
    }
    Json windowJson = {{"title", window.title}, {"width", window.width}, {"height", window.height}, {"fullscreen", window.fullscreen}, {"highDpi", window.highDpi}, {"resizable", window.resizable}, {"vsync", window.vsync}, {"sampleCount", window.sampleCount}, {"decorated", window.decorated}, {"transparent", window.transparent}, {"alwaysOnTop", window.alwaysOnTop}, {"showInTaskbar", window.showInTaskbar}, {"focusable", window.focusable}, {"mousePassthrough", window.mousePassthrough}};
    windowJson["statusBar"] = platform::Window::statusBarName(window.statusBar);
    if (window.statusBarColor) {
        windowJson["statusBarColor"] = window.statusBarColor->toHex();
    }
    if (window.position) {
        windowJson["position"] = window.position->toJson();
    }
    Json contentJson = {{"delivery", Json::array()}};
    for (const Content::Rule& rule : content.delivery) {
        if (rule.pack.empty()) {
            contentJson["delivery"].push_back({{"match", rule.match}, {"mode", deliveryName(rule.mode)}});
        } else {
            contentJson["delivery"].push_back({{"match", rule.match}, {"pack", rule.pack}});
        }
    }
    if (content.updates) {
        contentJson["updates"] = {{"baseUrl", content.updates->baseUrl}, {"channel", content.updates->channel}, {"enabled", content.updates->enabled}};
    }
    return {
        {"name", name}, {"identifier", identifier}, {"version", version}, {"description", description}, {"company", company}, {"copyright", copyright}, {"website", website}, {"window", windowJson}, {"design", {{"width", JsonNumber::fromFloat(designSize.x)}, {"height", JsonNumber::fromFloat(designSize.y)}, {"scaling", graphics::Viewport::scalingPolicyName(scaling)}}}, {"orientation", platform::Window::orientationName(orientation)}, {"fixedRate", fixedRate}, {"maxFrameTime", maxFrameTime}, {"clearColor", clearColor.toHex()}, {"splash", splashJson}, {"lifecycle", {{"pauseOnBackground", lifecycle.pauseOnBackground}, {"pauseOnFocusLoss", lifecycle.pauseOnFocusLoss}, {"muteOnFocusLoss", lifecycle.muteOnFocusLoss}}}, {"audio", {{"iosSession", audio::Session::categoryName(audioSession.category)}, {"mixWithOthers", audioSession.mixWithOthers}, {"latency", audio::OutputSettings::latencyName(audioOutput.latency)}, {"limiter", audioOutput.limiter}}}, {"input", {{"mouseAsTouch", input.mouseAsTouch}, {"touchAsMouse", input.touchAsMouse}, {"scrollSpeed", JsonNumber::fromFloat(input.scrollSpeed)}}}, {"ui", {{"scaleMode", std::string(ui::Scaling::modeName(uiScaling.mode))}, {"scale", JsonNumber::fromFloat(uiScaling.factor)}}}, {"debug", debugJson}, {"content", contentJson}, {"autoload", autoloads}, {"native", native}, {"plugins", plugins},
    };
}

} // namespace haylen::core
