#include "platform/PluginManifest.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/SystemAnswer.hpp"

namespace haylen::platform {

PluginManifest PluginManifest::read(const io::Package& package, std::string_view id) {
    const std::string path = io::Path::plugin(id, io::Path::kPluginManifestFile);
    const core::Json manifest = core::Json::parse(package.readText(path), nullptr, false);
    if (!manifest.is_object()) {
        throw std::runtime_error("The file \"" + path + "\" of the package is not a JSON object.");
    }

    const auto version = manifest.find("version");
    if (version == manifest.end() || !version->is_string()) {
        throw std::runtime_error("The file \"" + path + "\" of the package has no version.");
    }

    PluginManifest read;
    read.version = version->get<std::string>();
    read.dependencies = readDependencies(manifest, path);
    read.defaults = readDefaults(manifest, path);
    read.nativePlatforms = readNativePlatforms(manifest, path);
    read.appleClasses = readAppleClasses(manifest, path);
    return read;
}

const std::string& PluginManifest::getVersion() const noexcept {
    return version;
}

const std::vector<std::string>& PluginManifest::getDependencies() const noexcept {
    return dependencies;
}

const core::Json& PluginManifest::getDefaults() const noexcept {
    return defaults;
}

bool PluginManifest::isNative(std::string_view platform) const {
    return std::ranges::find(nativePlatforms, platform) != nativePlatforms.end();
}

std::optional<std::string> PluginManifest::findAppleClass(std::string_view platform) const {
    if (!isNative(platform)) {
        return std::nullopt;
    }

    for (const AppleClass& appleClass : appleClasses) {
        if (std::ranges::find(appleClass.platforms, platform) != appleClass.platforms.end()) {
            return appleClass.name;
        }
    }

    return std::nullopt;
}

std::vector<std::string> PluginManifest::readDependencies(const core::Json& manifest, const std::string& path) {
    const auto declared = manifest.find("dependencies");
    if (declared == manifest.end()) {
        return {};
    }

    if (!declared->is_object()) {
        throw std::runtime_error("The \"dependencies\" of \"" + path + "\" in the package is not an object of plugin ids with the oldest version that the plugin works with, such as {\"cloud\": \"1.2\"}.");
    }

    std::vector<std::string> ids;
    for (const auto& [id, version] : declared->items()) {
        if (!version.is_string()) {
            throw std::runtime_error("The dependency \"" + id + "\" of \"" + path + "\" in the package has no version, such as \"1.2\".");
        }

        ids.push_back(id);
    }

    return ids;
}

core::Json PluginManifest::readDefaults(const core::Json& manifest, const std::string& path) {
    core::Json values = core::Json::object();
    const auto declared = manifest.find("parameters");
    if (declared == manifest.end()) {
        return values;
    }

    if (!declared->is_object()) {
        throw std::runtime_error("The \"parameters\" of \"" + path + "\" in the package is not an object of parameters by name.");
    }

    for (const auto& [name, parameter] : declared->items()) {
        if (!parameter.is_object()) {
            throw std::runtime_error("The parameter \"" + name + "\" of \"" + path + "\" in the package is not an object with its \"type\" and \"description\".");
        }

        if (parameter.contains("default")) {
            values[name] = parameter.at("default");
        }
    }

    return values;
}

std::vector<std::string> PluginManifest::readNativePlatforms(const core::Json& manifest, const std::string& path) {
    const auto declared = manifest.find("implementations");
    if (declared == manifest.end() || !declared->is_object()) {
        throw std::runtime_error("The file \"" + path + "\" of the package has no \"implementations\", the object that says whether the plugin works with its native part or with its Lua alone on each platform, such as {\"ios\": \"native\", \"web\": \"lua\"}.");
    }

    std::vector<std::string> platforms;
    for (const auto& [platform, implementation] : declared->items()) {
        if (implementation != "native" && implementation != "lua") {
            throw std::runtime_error("The implementation of \"" + path + "\" in the package for \"" + platform + "\" must be \"native\" or \"lua\", not " + implementation.dump() + ".");
        }

        if (implementation == "native") {
            platforms.push_back(platform);
        }
    }

    return platforms;
}

std::vector<PluginManifest::AppleClass> PluginManifest::readAppleClasses(const core::Json& manifest, const std::string& path) {
    const auto declared = manifest.find("contributions");
    if (declared == manifest.end()) {
        return {};
    }

    if (!declared->is_array()) {
        throw std::runtime_error("The \"contributions\" of \"" + path + "\" in the package is not a list of contributions.");
    }

    std::vector<AppleClass> classes;
    for (const core::Json& contribution : *declared) {
        if (!contribution.is_object() || !contribution.contains("type") || !contribution.at("type").is_string()) {
            throw std::runtime_error("A contribution of \"" + path + "\" in the package is not an object with its \"type\" and \"platforms\".");
        }

        if (contribution.at("type") != "appleClass") {
            continue;
        }

        const auto name = contribution.find("name");
        if (name == contribution.end() || !name->is_string() || name->get_ref<const std::string&>().empty()) {
            throw std::runtime_error("The \"appleClass\" contribution of \"" + path + "\" in the package needs the \"name\" of the Objective-C class of the plugin.");
        }

        classes.push_back({.name = name->get<std::string>(), .platforms = readPlatforms(contribution, path)});
    }

    return classes;
}

std::vector<std::string> PluginManifest::readPlatforms(const core::Json& contribution, const std::string& path) {
    const auto declared = contribution.find("platforms");
    const bool listed = declared != contribution.end() && declared->is_array() && !declared->empty() && std::ranges::all_of(*declared, &core::Json::is_string);
    if (!listed) {
        throw std::runtime_error("A contribution of \"" + path + "\" in the package needs \"platforms\", the list of the platforms it applies to, such as [\"ios\"].");
    }

    return declared->get<std::vector<std::string>>();
}

} // namespace haylen::platform
