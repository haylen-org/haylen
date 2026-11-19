#include "haylen/core/AppConfig.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

#include "content/format/Manifest.hpp"
#include "haylen/assets/GroupManifest.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/JsonValidator.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/Window.hpp"
#include "localization/LanguageTag.hpp"

namespace haylen::core {

const std::array<std::pair<std::string_view, AppConfig::Debug::Reload>, 2> AppConfig::kReloadNames{{{"module", Debug::Reload::Module}, {"restart", Debug::Reload::Restart}}};

const std::array<std::pair<std::string_view, content::Delivery>, 3> AppConfig::kDeliveryNames{{{"required", content::Delivery::Required}, {"prefetch", content::Delivery::Prefetch}, {"onDemand", content::Delivery::OnDemand}}};

const std::array<std::string_view, 10> AppConfig::kCategoryNames{"game", "entertainment", "education", "productivity", "utility", "graphics", "music", "video", "social", "developer"};

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

// A tag that `haylen.json` leaves out stays empty, and any other comes back in its canonical form.
std::string AppConfig::readLanguageTag(const Json& section, const char* key) {
    std::string tag;
    readValue(section, key, tag);

    if (!section.contains(key)) {
        return tag;
    }

    std::optional<std::string> canonical = localization::LanguageTag::parse(tag);
    if (!canonical) {
        throw std::invalid_argument("The \"localization." + std::string(key) + "\" value \"" + tag + "\" in \"haylen.json\" is no language tag. Write a language such as \"en\", \"pt-BR\" or \"pt_BR\".");
    }

    return std::move(*canonical);
}

// The fallback names a file of the folder, which the localization plugin checks once it loaded the folder.
void AppConfig::readLocalization(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"folder", "fallback", "language"}, "the \"localization\" section of \"haylen.json\"");
    Localization& localization = config.localization;
    readValue(section, "folder", localization.folder);

    if (section.contains("folder")) {
        localization.folder = io::Path::normalize(localization.folder);
    }

    localization.fallback = readLanguageTag(section, "fallback");
    localization.language = readLanguageTag(section, "language");

    if (!localization.fallback.empty() && localization.folder.empty()) {
        throw std::invalid_argument("The \"localization.fallback\" of \"haylen.json\" needs a \"localization.folder\" that holds its language file.");
    }
}

AppConfig::Content::Updates AppConfig::readUpdates(const Json& section) {
    JsonValidator::requireKnownKeys(section, {"baseUrl", "channel", "enabled"}, "the \"content.updates\" section of \"haylen.json\"");
    Content::Updates updates;
    readValue(section, "baseUrl", updates.baseUrl);
    readValue(section, "channel", updates.channel);
    readValue(section, "enabled", updates.enabled);

    if (!isServerAddress(updates.baseUrl)) {
        throw std::invalid_argument("The \"content.updates.baseUrl\" in \"haylen.json\" must be an \"https\" address that ends with a slash, such as \"https://cdn.example.com/my-game/\", or an \"http\" address of this machine, not \"" + updates.baseUrl + "\".");
    }

    if (!content::Manifest::isValidName(updates.channel)) {
        throw std::invalid_argument("The \"content.updates.channel\" in \"haylen.json\" must have at most 64 lowercase letters, digits, dashes and dots, not \"" + updates.channel + "\".");
    }

    return updates;
}

AppConfig::Content::Rule AppConfig::readDeliveryRule(const Json& section, std::string_view groups) {
    JsonValidator::requireKnownKeys(section, {"match", "group", "mode", "pack"}, "a rule of \"content.delivery\" in \"haylen.json\"");
    Content::Rule rule;
    std::string mode = "required";
    readValue(section, "match", rule.match);
    readValue(section, "group", rule.group);
    readValue(section, "mode", mode);
    readValue(section, "pack", rule.pack);

    if (section.contains("match") == section.contains("group")) {
        throw std::invalid_argument("A rule of \"content.delivery\" in \"haylen.json\" needs either a \"match\" pattern or the name of a preload \"group\".");
    }

    if (section.contains("group") && groups.empty()) {
        throw std::invalid_argument("A rule of \"content.delivery\" in \"haylen.json\" names the group \"" + rule.group + "\", so \"content.groups\" must name the group manifest that defines it.");
    }

    if (section.contains("mode") == section.contains("pack")) {
        throw std::invalid_argument("A rule of \"content.delivery\" in \"haylen.json\" needs either a \"mode\" or a \"pack\", since a store delivers a pack on its own.");
    }

    if (section.contains("pack") && !content::Manifest::isValidName(rule.pack)) {
        throw std::invalid_argument("The \"pack\" of a rule of \"content.delivery\" in \"haylen.json\" must have at most 64 lowercase letters, digits, dashes and dots, not \"" + rule.pack + "\".");
    }

    if (section.contains("match") && !isContentPattern(rule.match)) {
        throw std::invalid_argument("A rule of \"content.delivery\" in \"haylen.json\" must match paths relative to the \"content\" folder, such as \"world/asia/**\", not \"" + rule.match + "\".");
    }

    const std::optional<content::Delivery> delivery = deliveryFromName(mode);
    if (!delivery) {
        throw std::invalid_argument("The \"mode\" of a rule of \"content.delivery\" in \"haylen.json\" must be \"required\", \"prefetch\" or \"onDemand\", not \"" + mode + "\".");
    }

    rule.mode = *delivery;
    return rule;
}

void AppConfig::readContent(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"updates", "delivery", "groups"}, "the \"content\" section of \"haylen.json\"");
    readValue(section, "groups", config.content.groups);

    if (section.contains("groups") && !isContentPattern(config.content.groups)) {
        throw std::invalid_argument("The \"content.groups\" in \"haylen.json\" must name a file relative to the \"content\" folder, such as \"groups.json\", not \"" + config.content.groups + "\".");
    }

    if (section.contains("updates")) {
        config.content.updates = readUpdates(section.at("updates"));
    }

    if (!section.contains("delivery")) {
        return;
    }

    const Json& rules = section.at("delivery");
    if (!rules.is_array()) {
        throw std::invalid_argument("The \"content.delivery\" section of \"haylen.json\" must be a list of rules with \"match\" or \"group\" and a \"mode\" or a \"pack\".");
    }

    for (const Json& rule : rules) {
        config.content.delivery.push_back(readDeliveryRule(rule, config.content.groups));
    }
}

std::optional<AppConfig::Debug::Reload> AppConfig::reloadFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kReloadNames, name, &std::pair<std::string_view, Debug::Reload>::first);
    return found != kReloadNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view AppConfig::reloadName(Debug::Reload value) noexcept {
    return std::ranges::find(kReloadNames, value, &std::pair<std::string_view, Debug::Reload>::second)->first;
}

// A number reaches a field only when the field holds it as it is, so a fraction never truncates into a whole number and no number falls outside its type.
template <typename T> void AppConfig::readValue(const Json& object, const char* key, T& target) {
    if (!object.contains(key)) {
        return;
    }

    const Json& value = object.at(key);
    if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
        const bool fits = value.is_number_unsigned() ? std::in_range<T>(value.get<std::uint64_t>()) : value.is_number_integer() && std::in_range<T>(value.get<std::int64_t>());
        if (!fits) {
            throw std::invalid_argument(std::format("The value of \"{}\" in \"haylen.json\" must be a whole number from {} to {}.", key, std::numeric_limits<T>::min(), std::numeric_limits<T>::max()));
        }
    }

    if constexpr (std::is_floating_point_v<T>) {
        const bool fits = value.is_number() && std::isfinite(value.get<double>()) && std::abs(value.get<double>()) <= std::numeric_limits<T>::max();
        if (!fits) {
            throw std::invalid_argument(std::format("The value of \"{}\" in \"haylen.json\" must be a number from {:g} to {:g}.", key, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()));
        }
    }

    try {
        target = value.get<T>();
    } catch (const Json::exception&) {
        throw std::invalid_argument(std::string("The value of \"") + key + "\" in \"haylen.json\" has the wrong type.");
    }
}

std::uint32_t AppConfig::readBuild(const Json& document) {
    if (!document.contains("build")) {
        return 1;
    }

    const Json& value = document.at("build");
    const std::int64_t number = value.is_number_integer() ? value.get<std::int64_t>() : 0;
    if (number < 1 || number > kMaxBuild) {
        throw std::invalid_argument("The \"build\" value " + value.dump() + " in \"haylen.json\" is not a build number. It is a whole number from 1 to " + std::to_string(kMaxBuild) + " that grows with every upload of the app.");
    }

    return static_cast<std::uint32_t>(number);
}

void AppConfig::requirePositive(double value, const char* key) {
    if (value <= 0.0) {
        throw std::invalid_argument(std::string("The value of \"") + key + "\" in \"haylen.json\" must be positive.");
    }
}

void AppConfig::requireNonNegative(double value, const char* key) {
    if (!(value >= 0.0) || !std::isfinite(value)) {
        throw std::invalid_argument(std::string("The value of \"") + key + "\" in \"haylen.json\" must be a number of at least 0.");
    }
}

math::Color AppConfig::readColor(const Json& object, const char* key, const char* name) {
    std::string text;
    readValue(object, key, text);
    const auto color = math::Color::parse(text);
    if (!color) {
        throw std::invalid_argument(std::string("The \"") + name + "\" value \"" + text + "\" in \"haylen.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
    }

    return *color;
}

platform::Orientation AppConfig::orientationFromName(const std::string& text) {
    const std::optional<platform::Orientation> orientation = platform::Window::orientationFromName(text);
    if (!orientation) {
        throw std::invalid_argument("The orientation \"" + text + "\" in \"haylen.json\" is unknown. It is \"landscape\", \"portrait\" or \"any\".");
    }

    return *orientation;
}

audio::Session::Category AppConfig::sessionCategoryFromName(const std::string& text) {
    const std::optional<audio::Session::Category> category = audio::Session::categoryFromName(text);
    if (!category) {
        throw std::invalid_argument("The \"audio.iosSession\" value \"" + text + "\" in \"haylen.json\" is unknown. It is \"ambient\", \"soloAmbient\" or \"playback\".");
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

// The names are the ones of `gpu::Device::kApiNames`, each once, and the name that means none draws nothing.
std::vector<gpu::Device::Api> AppConfig::readGraphicsApis(const Json& value) {
    if (!value.is_array()) {
        throw std::invalid_argument("The \"window.graphicsApis\" of \"haylen.json\" is not an array of graphics API names, such as [\"vulkan\", \"openGl\"].");
    }

    std::vector<gpu::Device::Api> apis;
    for (const Json& entry : value) {
        const std::string name = entry.is_string() ? entry.get<std::string>() : entry.dump();
        const auto found = std::ranges::find(gpu::Device::kApiNames, name, &std::pair<std::string_view, gpu::Device::Api>::first);
        if (found == gpu::Device::kApiNames.end() || found->second == gpu::Device::Api::None) {
            throw std::invalid_argument("The \"window.graphicsApis\" value \"" + name + "\" in \"haylen.json\" is not a graphics API. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".");
        }

        if (std::ranges::find(apis, found->second) != apis.end()) {
            throw std::invalid_argument("The \"window.graphicsApis\" of \"haylen.json\" names \"" + name + "\" twice.");
        }

        apis.push_back(found->second);
    }

    return apis;
}

void AppConfig::readPlugins(const Json& section, AppConfig& config) {
    if (!section.is_object()) {
        throw std::invalid_argument("The \"plugins\" section of \"haylen.json\" must be an object of plugins by id.");
    }

    for (const auto& [id, entry] : section.items()) {
        if (!isPluginId(id)) {
            throw std::invalid_argument("The plugin id \"" + id + "\" in \"haylen.json\" is not in \"dash-case\", such as \"cloud-analytics\".");
        }

        config.plugins[id] = readPlugin(id, entry);
    }
}

AppConfig::Plugin AppConfig::readPlugin(const std::string& id, const Json& entry) {
    if (!entry.is_object()) {
        throw std::invalid_argument("The plugin \"" + id + "\" in \"haylen.json\" must be an object with the optional keys \"path\", \"git\", \"ref\" and \"parameters\".");
    }

    JsonValidator::requireKnownKeys(entry, {"path", "git", "ref", "parameters"}, "the plugin \"" + id + "\" of \"haylen.json\"");

    Plugin plugin;
    readPluginText(entry, id, "path", plugin.path);
    readPluginText(entry, id, "git", plugin.git);
    readPluginText(entry, id, "ref", plugin.ref);

    if (!plugin.path.empty() && !plugin.git.empty()) {
        throw std::invalid_argument("The plugin \"" + id + "\" in \"haylen.json\" names both a \"path\" and a \"git\" repository. Keep the one that the plugin comes from.");
    }

    if (!plugin.ref.empty() && plugin.git.empty()) {
        throw std::invalid_argument("The \"ref\" of the plugin \"" + id + "\" in \"haylen.json\" picks a branch, tag or commit of a git repository, so it needs \"git\".");
    }

    const auto parameters = entry.find("parameters");
    if (parameters == entry.end()) {
        return plugin;
    }

    if (!parameters->is_object()) {
        throw std::invalid_argument("The \"parameters\" of the plugin \"" + id + "\" in \"haylen.json\" must be an object of parameter values.");
    }

    plugin.parameters = *parameters;
    return plugin;
}

// The tooling builds the app from `category`, `kind`, `cpp` and `platforms`, so the engine checks only their values and types.
void AppConfig::requireToolingSections(const Json& document) {
    const auto category = document.find("category");
    if (category != document.end() && (!category->is_string() || std::ranges::find(kCategoryNames, category->get_ref<const std::string&>()) == kCategoryNames.end())) {
        throw std::invalid_argument("The \"category\" of \"haylen.json\" must be \"game\", \"entertainment\", \"education\", \"productivity\", \"utility\", \"graphics\", \"music\", \"video\", \"social\" or \"developer\", not " + category->dump() + ".");
    }

    const auto kind = document.find("kind");
    if (kind != document.end() && *kind != "lua" && *kind != "cpp") {
        throw std::invalid_argument("The \"kind\" of \"haylen.json\" must be \"lua\" or \"cpp\", not " + kind->dump() + ".");
    }

    const auto cpp = document.find("cpp");
    if (cpp != document.end() && !cpp->is_object()) {
        throw std::invalid_argument("The \"cpp\" section of \"haylen.json\" must be an object with the \"sources\" of the C++ app.");
    }

    const auto platforms = document.find("platforms");
    if (platforms == document.end()) {
        return;
    }
    if (!platforms->is_object()) {
        throw std::invalid_argument("The \"platforms\" section of \"haylen.json\" must be an object of platform sections by name, such as \"ios\" or \"android\".");
    }
    for (const auto& [name, section] : platforms->items()) {
        if (!section.is_object()) {
            throw std::invalid_argument("The section \"" + name + "\" of \"platforms\" in \"haylen.json\" must be an object.");
        }
    }
}

void AppConfig::readPluginText(const Json& entry, const std::string& id, const char* key, std::string& target) {
    const auto found = entry.find(key);
    if (found == entry.end()) {
        return;
    }

    if (!found->is_string() || found->get_ref<const std::string&>().empty()) {
        throw std::invalid_argument("The \"" + std::string(key) + "\" of the plugin \"" + id + "\" in \"haylen.json\" must be a text.");
    }

    target = found->get<std::string>();
}

bool AppConfig::isContentPattern(std::string_view pattern) noexcept {
    return !pattern.empty() && !pattern.starts_with('/') && pattern.find("..") == std::string_view::npos && pattern.find('\\') == std::string_view::npos;
}

bool AppConfig::Content::hasGroupRules() const noexcept {
    return std::ranges::any_of(delivery, [](const Rule& rule) { return !rule.group.empty() && rule.match.empty(); });
}

// A rule that names a preload group takes its place with a rule for each entry of the group: the path of a file, or every file under a folder.
void AppConfig::Content::expandGroups(const io::Package& package) {
    if (!hasGroupRules()) {
        return;
    }

    const std::string manifest = io::Path::asset(groups);
    if (!package.exists(manifest)) {
        throw std::invalid_argument("The group manifest \"" + manifest + "\" that \"content.groups\" in \"haylen.json\" names is not in the package.");
    }

    const std::map<std::string, std::vector<assets::GroupManifest::Entry>> defined = assets::GroupManifest::read(Json::parse(package.readText(manifest)));
    std::vector<Rule> expanded;
    for (Rule& rule : delivery) {
        if (rule.group.empty() || !rule.match.empty()) {
            expanded.push_back(std::move(rule));
            continue;
        }

        const auto found = defined.find(rule.group);
        if (found == defined.end()) {
            throw std::invalid_argument("A rule of \"content.delivery\" in \"haylen.json\" names the group \"" + rule.group + "\", which \"" + manifest + "\" does not define.");
        }

        for (const assets::GroupManifest::Entry& entry : found->second) {
            if (!isContentPattern(entry.path)) {
                throw std::invalid_argument("The group \"" + rule.group + "\" of \"" + manifest + "\" has the path \"" + entry.path + "\", which is not relative to the \"content\" folder.");
            }

            expanded.push_back({.match = entry.path.ends_with('/') ? entry.path + "**" : entry.path, .mode = rule.mode, .pack = rule.pack, .group = rule.group});
        }
    }

    delivery = std::move(expanded);
}

AppConfig AppConfig::fromPackage(const io::Package& package) {
    AppConfig config = fromJson(Json::parse(package.readText(io::Path::kAppConfigFile)));

    for (const auto& [id, plugin] : config.plugins) {
        const std::string manifest = io::Path::plugin(id, io::Path::kPluginManifestFile);
        if (package.exists(manifest)) {
            continue;
        }

        // The package holds a referenced plugin only when the tooling staged it, so a folder that the player runs directly lacks it.
        if (!plugin.path.empty() || !plugin.git.empty()) {
            throw std::invalid_argument("The plugin \"" + id + "\" in \"haylen.json\" comes from a \"path\" or a \"git\" repository, so the package has no \"" + manifest + "\". Run the app with \"haylen app run\", which places every plugin in the package wherever it comes from.");
        }

        throw std::invalid_argument("The plugin \"" + id + "\" in \"haylen.json\" has no \"" + manifest + "\" in the package.");
    }

    return config;
}

void AppConfig::readWindow(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"title", "width", "height", "fullscreen", "maxPixelDensity", "resizable", "vsync", "frameRate", "sampleCount", "decorated", "transparent", "alwaysOnTop", "showInTaskbar", "focusable", "mousePassthrough", "keepScreenOn", "position", "statusBar", "statusBarColor", "systemGestures", "graphicsApis", "lowPowerGpu"}, "the \"window\" section of \"haylen.json\"");
    readValue(section, "title", config.window.title);
    readValue(section, "width", config.window.width);
    readValue(section, "height", config.window.height);
    readValue(section, "fullscreen", config.window.fullscreen);
    readValue(section, "maxPixelDensity", config.window.maxPixelDensity);
    readValue(section, "resizable", config.window.resizable);
    readValue(section, "vsync", config.window.vsync);
    readValue(section, "frameRate", config.window.frameRate);
    readValue(section, "sampleCount", config.window.sampleCount);
    readValue(section, "decorated", config.window.decorated);
    readValue(section, "transparent", config.window.transparent);
    readValue(section, "alwaysOnTop", config.window.alwaysOnTop);
    readValue(section, "showInTaskbar", config.window.showInTaskbar);
    readValue(section, "focusable", config.window.focusable);
    readValue(section, "mousePassthrough", config.window.mousePassthrough);
    readValue(section, "keepScreenOn", config.window.keepScreenOn);
    readValue(section, "lowPowerGpu", config.window.lowPowerGpu);
    requirePositive(config.window.width, "window.width");
    requirePositive(config.window.height, "window.height");
    requirePositive(config.window.sampleCount, "window.sampleCount");

    if (!std::isfinite(config.window.maxPixelDensity) || config.window.maxPixelDensity < 0.0F) {
        throw std::invalid_argument("The \"window.maxPixelDensity\" value in \"haylen.json\" is out of range. It is 0 for every pixel of the display, or a positive number of framebuffer pixels in each point.");
    }

    if (config.window.frameRate < 0 || config.window.frameRate > platform::Window::kMaxFrameRate) {
        throw std::invalid_argument("The \"window.frameRate\" value " + std::to_string(config.window.frameRate) + " in \"haylen.json\" is out of range. It is 0 for every refresh of the display, or from 1 to " + std::to_string(platform::Window::kMaxFrameRate) + " frames per second.");
    }

    std::string statusBarText;
    readValue(section, "statusBar", statusBarText);

    if (!statusBarText.empty()) {
        const std::optional<platform::Window::StatusBar> statusBar = platform::Window::statusBarFromName(statusBarText);
        if (!statusBar) {
            throw std::invalid_argument("The \"window.statusBar\" value \"" + statusBarText + "\" in \"haylen.json\" is unknown. It is \"hidden\", \"auto\", \"light\" or \"dark\".");
        }

        config.window.statusBar = *statusBar;
    }

    if (section.contains("systemGestures")) {
        std::string systemGesturesText;
        readValue(section, "systemGestures", systemGesturesText);
        const std::optional<platform::Window::SystemGestures> systemGestures = platform::Window::systemGesturesFromName(systemGesturesText);
        if (!systemGestures) {
            throw std::invalid_argument("The \"window.systemGestures\" value \"" + systemGesturesText + "\" in \"haylen.json\" is unknown. It is \"system\", \"deferred\" or \"autoHideIndicator\".");
        }

        config.window.systemGestures = *systemGestures;
    }

    std::string statusBarColorText;
    readValue(section, "statusBarColor", statusBarColorText);

    if (!statusBarColorText.empty()) {
        config.window.statusBarColor = math::Color::parse(statusBarColorText);
        if (!config.window.statusBarColor) {
            throw std::invalid_argument("The \"window.statusBarColor\" value \"" + statusBarColorText + "\" in \"haylen.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
        }
    }

    if (section.contains("graphicsApis")) {
        config.window.graphicsApis = readGraphicsApis(section.at("graphicsApis"));
    }

    if (section.contains("position")) {
        try {
            config.window.position = platform::WindowPlacement::fromJson(section.at("position"));
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument(std::string("The \"window.position\" in \"haylen.json\" is invalid. ") + error.what());
        }
    }
}

void AppConfig::readDesign(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"width", "height", "scaling"}, "the \"design\" section of \"haylen.json\"");
    readValue(section, "width", config.designSize.x);
    readValue(section, "height", config.designSize.y);
    requirePositive(config.designSize.x, "design.width");
    requirePositive(config.designSize.y, "design.height");

    std::string scalingText;
    readValue(section, "scaling", scalingText);

    if (!scalingText.empty()) {
        const auto policy = graphics::Viewport::scalingPolicyFromName(scalingText);
        if (!policy) {
            throw std::invalid_argument("The \"design.scaling\" value \"" + scalingText + "\" in \"haylen.json\" is unknown. It is \"fit\", \"fill\", \"stretch\", \"expand\", \"pixelPerfect\", \"fitWidth\" or \"none\".");
        }

        config.scaling = *policy;
    }
}

void AppConfig::readSplash(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"logo", "background", "darkBackground", "duration", "fadeOut"}, "the \"splash\" section of \"haylen.json\"");
    readValue(section, "logo", config.splash.logo);

    if (section.contains("background")) {
        config.splash.background = readColor(section, "background", "splash.background");
    }

    if (section.contains("darkBackground")) {
        config.splash.darkBackground = readColor(section, "darkBackground", "splash.darkBackground");
    }

    readValue(section, "duration", config.splash.duration);
    requireNonNegative(config.splash.duration, "splash.duration");
    readValue(section, "fadeOut", config.splash.fadeOut);
    requireNonNegative(config.splash.fadeOut, "splash.fadeOut");
}

void AppConfig::readLifecycle(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"pauseOnBackground", "pauseOnFocusLoss", "muteOnFocusLoss"}, "the \"lifecycle\" section of \"haylen.json\"");
    readValue(section, "pauseOnBackground", config.lifecycle.pauseOnBackground);
    readValue(section, "pauseOnFocusLoss", config.lifecycle.pauseOnFocusLoss);
    readValue(section, "muteOnFocusLoss", config.lifecycle.muteOnFocusLoss);
}

void AppConfig::readAudio(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"iosSession", "mixWithOthers", "background", "latency", "limiter"}, "the \"audio\" section of \"haylen.json\"");

    if (section.contains("latency")) {
        std::string latency;
        readValue(section, "latency", latency);
        const std::optional<audio::OutputSettings::Latency> found = audio::OutputSettings::latencyFromName(latency);
        if (!found) {
            throw std::invalid_argument("The \"audio.latency\" value \"" + latency + "\" in \"haylen.json\" is unknown. It is \"low\", \"balanced\" or \"powerSaving\".");
        }

        config.audioOutput.latency = *found;
    }

    readValue(section, "limiter", config.audioOutput.limiter);

    if (section.contains("iosSession")) {
        std::string category;
        readValue(section, "iosSession", category);
        config.audioSession.category = sessionCategoryFromName(category);
    }

    readValue(section, "mixWithOthers", config.audioSession.mixWithOthers);

    if (config.audioSession.mixWithOthers && config.audioSession.category != audio::Session::Category::Playback) {
        throw std::invalid_argument("The \"audio.mixWithOthers\" option in \"haylen.json\" needs the \"iosSession\" value \"playback\".");
    }

    readValue(section, "background", config.audioSession.background);

    if (config.audioSession.background && config.audioSession.category != audio::Session::Category::Playback) {
        throw std::invalid_argument("The \"audio.background\" option in \"haylen.json\" needs the \"iosSession\" value \"playback\", the only session that iOS and tvOS play in the background.");
    }
}

void AppConfig::readInput(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"mouseAsTouch", "touchAsMouse", "scrollSpeed"}, "the \"input\" section of \"haylen.json\"");
    readValue(section, "mouseAsTouch", config.input.mouseAsTouch);
    readValue(section, "touchAsMouse", config.input.touchAsMouse);
    readValue(section, "scrollSpeed", config.input.scrollSpeed);

    if (!(config.input.scrollSpeed > 0.0F && std::isfinite(config.input.scrollSpeed))) {
        throw std::invalid_argument("The \"input.scrollSpeed\" value in \"haylen.json\" must be a positive number.");
    }
}

void AppConfig::readUi(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"scaleMode", "scale"}, "the \"ui\" section of \"haylen.json\"");
    std::string mode;
    readValue(section, "scaleMode", mode);

    if (!mode.empty()) {
        const std::optional<ui::Scaling::Mode> found = ui::Scaling::modeFromName(mode);
        if (!found) {
            throw std::invalid_argument("The \"ui.scaleMode\" value \"" + mode + "\" in \"haylen.json\" is unknown. It is \"design\" or \"physical\".");
        }

        config.uiScaling.mode = *found;
    }

    readValue(section, "scale", config.uiScaling.factor);

    if (config.uiScaling.factor < ui::Scaling::kMinimumFactor || config.uiScaling.factor > ui::Scaling::kMaximumFactor) {
        throw std::invalid_argument("The \"ui.scale\" value in \"haylen.json\" must be from 0.25 to 4.");
    }
}

void AppConfig::readDebug(const Json& section, AppConfig& config) {
    JsonValidator::requireKnownKeys(section, {"stats", "statsItems", "drawings", "objectEvents", "console", "safeArea", "fold", "showSafeArea", "reload", "executionBudget", "memoryBudget"}, "the \"debug\" section of \"haylen.json\"");
    std::string stats;
    readValue(section, "stats", stats);

    if (!stats.empty()) {
        const auto mode = debug::StatsDisplay::modeFromName(stats);
        if (!mode) {
            throw std::invalid_argument("The \"debug.stats\" value \"" + stats + "\" in \"haylen.json\" is unknown. It is \"off\", \"compact\" or \"full\".");
        }

        config.debug.stats = *mode;
    }

    if (section.contains("statsItems")) {
        std::vector<std::string> items;
        readValue(section, "statsItems", items);
        config.debug.statsItems.clear();

        for (const std::string& name : items) {
            const std::optional<debug::StatsDisplay::Item> item = debug::StatsDisplay::itemFromName(name);
            if (!item) {
                throw std::invalid_argument("The \"debug.statsItems\" item \"" + name + "\" in \"haylen.json\" is unknown. It is \"frameRate\", \"drawCalls\", \"geometry\", \"memory\", \"counters\", \"physics\", \"particles\" or \"audio\".");
            }

            config.debug.statsItems.push_back(*item);
        }
    }

    readValue(section, "drawings", config.debug.drawings);

    if (std::ranges::any_of(config.debug.drawings, [](const std::string& drawing) { return drawing.empty(); })) {
        throw std::invalid_argument("The \"debug.drawings\" list in \"haylen.json\" has an empty drawing name.");
    }

    readValue(section, "objectEvents", config.debug.objectEvents);
    readValue(section, "console", config.debug.console);

    if (section.contains("safeArea")) {
        try {
            config.debug.safeArea = platform::SafeAreaSimulation::fromJson(section.at("safeArea"));
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument(std::string("The \"debug.safeArea\" in \"haylen.json\" is invalid. ") + error.what());
        }
    }

    if (section.contains("fold")) {
        try {
            config.debug.fold = platform::FoldSimulation::fromJson(section.at("fold"));
        } catch (const std::invalid_argument& error) {
            throw std::invalid_argument(std::string("The \"debug.fold\" in \"haylen.json\" is invalid. ") + error.what());
        }
    }

    readValue(section, "showSafeArea", config.debug.showSafeArea);

    if (section.contains("reload")) {
        std::string reload;
        readValue(section, "reload", reload);
        const auto mode = reloadFromName(reload);
        if (!mode) {
            throw std::invalid_argument("The \"debug.reload\" option must be \"module\" or \"restart\".");
        }

        config.debug.reload = *mode;
    }

    if (section.contains("executionBudget")) {
        double seconds = 0.0;
        readValue(section, "executionBudget", seconds);
        if (!(seconds >= kMinExecutionBudget && seconds <= kMaxExecutionBudget)) {
            throw std::invalid_argument("The \"debug.executionBudget\" value in \"haylen.json\" must be a number of seconds from 0.1 to 600.");
        }

        config.debug.executionBudget = std::chrono::milliseconds(std::llround(seconds * 1000.0));
    }

    if (section.contains("memoryBudget")) {
        double megabytes = 0.0;
        readValue(section, "memoryBudget", megabytes);
        if (!(megabytes >= kMinMemoryBudget && megabytes <= kMaxMemoryBudget)) {
            throw std::invalid_argument("The \"debug.memoryBudget\" value in \"haylen.json\" must be a number of megabytes from 16 to 4096.");
        }

        config.debug.memoryBudget = static_cast<std::size_t>(std::llround(megabytes * 1024.0 * 1024.0));
    }
}

AppConfig AppConfig::fromJson(const Json& document) {
    JsonValidator::requireKnownKeys(document, {"name", "identifier", "version", "build", "description", "company", "copyright", "website", "window", "design", "orientation", "fixedRate", "maxFrameTime", "clearColor", "splash", "lifecycle", "audio", "input", "ui", "debug", "content", "localization", "autoload", "native", "plugins", "category", "kind", "cpp", "platforms"}, "\"haylen.json\"");

    AppConfig config;
    readValue(document, "name", config.name);
    readValue(document, "identifier", config.identifier);
    readValue(document, "version", config.version);
    config.build = readBuild(document);

    config.description = config.name;
    config.company = config.name;
    config.window.title = config.name;
    readValue(document, "description", config.description);
    readValue(document, "company", config.company);
    config.copyright = "Copyright \xC2\xA9 " + config.company;
    readValue(document, "copyright", config.copyright);
    readValue(document, "website", config.website);

    if (document.contains("window")) {
        readWindow(document.at("window"), config);
    }

    if (document.contains("design")) {
        readDesign(document.at("design"), config);
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
            throw std::invalid_argument("The \"clearColor\" value \"" + clearColorText + "\" in \"haylen.json\" is not a color such as \"#RRGGBB\" or \"#AARRGGBB\".");
        }

        config.clearColor = *color;
    }

    config.splash.background = config.clearColor;
    if (document.contains("splash")) {
        readSplash(document.at("splash"), config);
    }

    if (document.contains("lifecycle")) {
        readLifecycle(document.at("lifecycle"), config);
    }

    if (document.contains("audio")) {
        readAudio(document.at("audio"), config);
    }

    if (document.contains("input")) {
        readInput(document.at("input"), config);
    }

    if (document.contains("ui")) {
        readUi(document.at("ui"), config);
    }

    if (document.contains("debug")) {
        readDebug(document.at("debug"), config);
    }

    readValue(document, "autoload", config.autoloads);

    for (const std::string& module : config.autoloads) {
        if (module.empty()) {
            throw std::invalid_argument("The \"autoload\" list in \"haylen.json\" has an empty module name.");
        }
    }

    if (document.contains("native")) {
        if (!document.at("native").is_object()) {
            throw std::invalid_argument("The \"native\" section of \"haylen.json\" must be an object of libraries.");
        }

        config.native = document.at("native");
    }

    if (document.contains("plugins")) {
        readPlugins(document.at("plugins"), config);
    }
    requireToolingSections(document);

    if (document.contains("content")) {
        readContent(document.at("content"), config);
    }

    if (document.contains("localization")) {
        readLocalization(document.at("localization"), config);
    }

    return config;
}

Json AppConfig::toJson() const {
    Json statsItems = Json::array();
    for (const debug::StatsDisplay::Item item : debug.statsItems) {
        statsItems.push_back(debug::StatsDisplay::itemName(item));
    }

    Json debugJson = {{"stats", debug::StatsDisplay::modeName(debug.stats)}, {"statsItems", statsItems}, {"drawings", debug.drawings}, {"objectEvents", debug.objectEvents}, {"console", debug.console}, {"showSafeArea", debug.showSafeArea}, {"reload", reloadName(debug.reload)}, {"executionBudget", static_cast<double>(debug.executionBudget.count()) / 1000.0}, {"memoryBudget", static_cast<double>(debug.memoryBudget) / (1024.0 * 1024.0)}};
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

    Json windowJson = {{"title", window.title}, {"width", window.width}, {"height", window.height}, {"fullscreen", window.fullscreen}, {"maxPixelDensity", JsonNumber::fromFloat(window.maxPixelDensity)}, {"resizable", window.resizable}, {"vsync", window.vsync}, {"frameRate", window.frameRate}, {"sampleCount", window.sampleCount}, {"decorated", window.decorated}, {"transparent", window.transparent}, {"alwaysOnTop", window.alwaysOnTop}, {"showInTaskbar", window.showInTaskbar}, {"focusable", window.focusable}, {"mousePassthrough", window.mousePassthrough}, {"keepScreenOn", window.keepScreenOn}, {"lowPowerGpu", window.lowPowerGpu}};
    windowJson["statusBar"] = platform::Window::statusBarName(window.statusBar);
    windowJson["systemGestures"] = platform::Window::systemGesturesName(window.systemGestures);

    if (window.statusBarColor) {
        windowJson["statusBarColor"] = window.statusBarColor->toHex();
    }

    if (window.position) {
        windowJson["position"] = window.position->toJson();
    }

    windowJson["graphicsApis"] = Json::array();

    for (const gpu::Device::Api api : window.graphicsApis) {
        windowJson["graphicsApis"].push_back(std::ranges::find(gpu::Device::kApiNames, api, &std::pair<std::string_view, gpu::Device::Api>::second)->first);
    }

    // A rule that names a group keeps the name until `Content::expandGroups` replaces it with the patterns of the group.
    Json contentJson = {{"delivery", Json::array()}};
    for (const Content::Rule& rule : content.delivery) {
        Json written = rule.match.empty() ? Json{{"group", rule.group}} : Json{{"match", rule.match}};
        if (rule.pack.empty()) {
            written["mode"] = deliveryName(rule.mode);
        } else {
            written["pack"] = rule.pack;
        }

        contentJson["delivery"].push_back(std::move(written));
    }

    if (!content.groups.empty()) {
        contentJson["groups"] = content.groups;
    }

    if (content.updates) {
        contentJson["updates"] = {{"baseUrl", content.updates->baseUrl}, {"channel", content.updates->channel}, {"enabled", content.updates->enabled}};
    }

    Json pluginsJson = Json::object();
    for (const auto& [id, plugin] : plugins) {
        Json written = {{"parameters", plugin.parameters}};
        if (!plugin.path.empty()) {
            written["path"] = plugin.path;
        }

        if (!plugin.git.empty()) {
            written["git"] = plugin.git;
        }

        if (!plugin.ref.empty()) {
            written["ref"] = plugin.ref;
        }

        pluginsJson[id] = std::move(written);
    }

    Json localizationJson = Json::object();
    if (!localization.folder.empty()) {
        localizationJson["folder"] = localization.folder;
    }

    if (!localization.fallback.empty()) {
        localizationJson["fallback"] = localization.fallback;
    }

    if (!localization.language.empty()) {
        localizationJson["language"] = localization.language;
    }

    return {
        {"name", name}, {"identifier", identifier}, {"version", version}, {"build", build}, {"description", description}, {"company", company}, {"copyright", copyright}, {"website", website}, {"window", windowJson}, {"design", {{"width", JsonNumber::fromFloat(designSize.x)}, {"height", JsonNumber::fromFloat(designSize.y)}, {"scaling", graphics::Viewport::scalingPolicyName(scaling)}}}, {"orientation", platform::Window::orientationName(orientation)}, {"fixedRate", fixedRate}, {"maxFrameTime", maxFrameTime}, {"clearColor", clearColor.toHex()}, {"splash", splashJson}, {"lifecycle", {{"pauseOnBackground", lifecycle.pauseOnBackground}, {"pauseOnFocusLoss", lifecycle.pauseOnFocusLoss}, {"muteOnFocusLoss", lifecycle.muteOnFocusLoss}}}, {"audio", {{"iosSession", audio::Session::categoryName(audioSession.category)}, {"mixWithOthers", audioSession.mixWithOthers}, {"background", audioSession.background}, {"latency", audio::OutputSettings::latencyName(audioOutput.latency)}, {"limiter", audioOutput.limiter}}}, {"input", {{"mouseAsTouch", input.mouseAsTouch}, {"touchAsMouse", input.touchAsMouse}, {"scrollSpeed", JsonNumber::fromFloat(input.scrollSpeed)}}}, {"ui", {{"scaleMode", std::string(ui::Scaling::modeName(uiScaling.mode))}, {"scale", JsonNumber::fromFloat(uiScaling.factor)}}}, {"debug", debugJson}, {"content", contentJson}, {"localization", localizationJson}, {"autoload", autoloads}, {"native", native}, {"plugins", pluginsJson},
    };
}

} // namespace haylen::core
