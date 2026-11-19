#include "haylen/platform/AppPlugin.hpp"

#include "platform/PluginManifest.hpp"

namespace haylen::platform {

AppPlugin AppPlugin::read(const io::Package& package, std::string_view id, const core::Json& parameters) {
    const PluginManifest manifest = PluginManifest::read(package, id);
    AppPlugin plugin{.id = std::string(id), .version = manifest.getVersion(), .config = parameters};
    for (const auto& [name, value] : manifest.getDefaults().items()) {
        if (!plugin.config.contains(name)) {
            plugin.config[name] = value;
        }
    }

    return plugin;
}

std::vector<AppPlugin> AppPlugin::readAll(const io::Package& package, const std::map<std::string, core::AppConfig::Plugin>& plugins) {
    std::vector<AppPlugin> all;
    for (const auto& [id, entry] : plugins) {
        all.push_back(read(package, id, entry.parameters));
    }

    return all;
}

} // namespace haylen::platform
