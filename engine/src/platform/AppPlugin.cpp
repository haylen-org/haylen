#include "haylen/platform/AppPlugin.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>

#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/platform/Permission.hpp"

namespace haylen::platform {

AppPlugin AppPlugin::read(const io::Package& package, std::string_view id, const core::Json& parameters) {
    const std::string path = io::Path::plugin(id, io::Path::kPluginManifestFile);
    const core::Json manifest = core::Json::parse(package.readText(path), nullptr, false);
    if (!manifest.is_object()) {
        throw std::runtime_error("The file \"" + path + "\" of the package is not a JSON object.");
    }
    const auto version = manifest.find("version");
    if (version == manifest.end() || !version->is_string()) {
        throw std::runtime_error("The file \"" + path + "\" of the package has no version.");
    }

    AppPlugin plugin{.id = std::string(id), .version = version->get<std::string>(), .config = parameters, .permissions = readPermissions(manifest, path)};
    const auto declared = manifest.find("parameters");
    if (declared == manifest.end() || !declared->is_object()) {
        return plugin;
    }
    for (const auto& [name, parameter] : declared->items()) {
        if (parameter.is_object() && parameter.contains("default") && !plugin.config.contains(name)) {
            plugin.config[name] = parameter.at("default");
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

std::vector<std::string> AppPlugin::readPermissions(const core::Json& manifest, const std::string& path) {
    const auto declared = manifest.find("permissions");
    if (declared == manifest.end()) {
        return {};
    }
    if (!declared->is_array()) {
        throw std::runtime_error("The \"permissions\" of \"" + path + "\" in the package is not a list of permission names, such as [\"camera\"].");
    }

    std::vector<std::string> permissions;
    for (const core::Json& entry : *declared) {
        const std::string name = entry.is_string() ? entry.get<std::string>() : entry.dump();
        if (Permission::kindFromName(name)) {
            throw std::runtime_error("The \"permissions\" of \"" + path + "\" in the package declare \"" + name + "\", which is a permission of the engine.");
        }
        if (!entry.is_string() || !Permission::isPluginName(name)) {
            throw std::runtime_error("The \"permissions\" of \"" + path + "\" in the package declare \"" + name + "\", which is not a \"camelCase\" name, such as \"camera\" or \"backgroundLocation\".");
        }
        if (std::ranges::find(permissions, name) != permissions.end()) {
            throw std::runtime_error("The \"permissions\" of \"" + path + "\" in the package declare \"" + name + "\" twice.");
        }
        permissions.push_back(name);
    }
    return permissions;
}

} // namespace haylen::platform
