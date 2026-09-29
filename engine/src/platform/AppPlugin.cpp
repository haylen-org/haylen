#include "haylen/platform/AppPlugin.hpp"

#include <stdexcept>

#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::platform {

AppPlugin AppPlugin::read(const io::Package& package, std::string_view id, const core::Json& values) {
    const std::string path = io::Path::plugin(id, io::Path::kPluginManifestFile);
    const core::Json manifest = core::Json::parse(package.readText(path), nullptr, false);
    if (!manifest.is_object()) {
        throw std::runtime_error("The " + path + " of the package is not a JSON object.");
    }
    const auto version = manifest.find("version");
    if (version == manifest.end() || !version->is_string()) {
        throw std::runtime_error("The " + path + " of the package has no version.");
    }

    AppPlugin plugin{.id = std::string(id), .version = version->get<std::string>(), .config = values};
    const auto parameters = manifest.find("parameters");
    if (parameters == manifest.end() || !parameters->is_object()) {
        return plugin;
    }
    for (const auto& [name, parameter] : parameters->items()) {
        if (parameter.is_object() && parameter.contains("default") && !plugin.config.contains(name)) {
            plugin.config[name] = parameter.at("default");
        }
    }
    return plugin;
}

} // namespace haylen::platform
