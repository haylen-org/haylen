#include "haylen/platform/AppPlugin.hpp"

#include <stdexcept>
#include <unordered_map>

#include "platform/PluginManifest.hpp"

namespace haylen::platform {

AppPlugin AppPlugin::read(const io::Package& package, std::string_view id, const core::Json& parameters) {
    const PluginManifest manifest = PluginManifest::read(package, id);
    AppPlugin plugin{.id = std::string(id), .version = manifest.getVersion(), .config = parameters, .permissions = manifest.getPermissions()};
    for (const auto& [name, value] : manifest.getDefaults().items()) {
        if (!plugin.config.contains(name)) {
            plugin.config[name] = value;
        }
    }

    return plugin;
}

std::vector<AppPlugin> AppPlugin::readAll(const io::Package& package, const std::map<std::string, core::AppConfig::Plugin>& plugins) {
    std::vector<AppPlugin> all;
    std::unordered_map<std::string, std::string> owners;
    for (const auto& [id, entry] : plugins) {
        AppPlugin plugin = read(package, id, entry.parameters);
        for (const std::string& permission : plugin.permissions) {
            const auto [owner, added] = owners.emplace(permission, plugin.id);
            if (!added) {
                throw std::runtime_error("The plugins \"" + owner->second + "\" and \"" + plugin.id + "\" both declare the permission \"" + permission + "\" in their \"plugin.json\", while one plugin of an app owns each permission.");
            }
        }

        all.push_back(std::move(plugin));
    }

    return all;
}

} // namespace haylen::platform
