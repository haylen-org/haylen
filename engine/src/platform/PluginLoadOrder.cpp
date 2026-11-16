#include "platform/PluginLoadOrder.hpp"

#include <algorithm>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "platform/PluginManifest.hpp"

namespace haylen::platform {

// The object of plugins keeps the order that `haylen.json` gives it, which a sorted JSON object would lose.
std::vector<std::string> PluginLoadOrder::read(const io::Package& package) {
    const nlohmann::ordered_json app = nlohmann::ordered_json::parse(package.readText(io::Path::kAppConfigFile), nullptr, false);
    if (!app.is_object()) {
        throw std::runtime_error("The file \"haylen.json\" of the package is not a JSON object.");
    }

    const auto plugins = app.find("plugins");
    if (plugins == app.end() || !plugins->is_object()) {
        return {};
    }

    std::vector<std::string> listed;
    for (const auto& [id, values] : plugins->items()) {
        listed.push_back(id);
    }

    std::map<std::string, std::vector<std::string>> required;
    for (const std::string& id : listed) {
        const PluginManifest manifest = PluginManifest::read(package, id);
        std::vector<std::string>& dependencies = required[id];
        for (const std::string& dependency : manifest.getDependencies()) {
            if (std::ranges::find(listed, dependency) != listed.end()) {
                dependencies.push_back(dependency);
            }
        }
    }

    std::set<std::string> visited;
    std::vector<std::string> order;
    for (const std::string& id : listed) {
        visit(id, required, visited, order);
    }

    return order;
}

void PluginLoadOrder::visit(const std::string& id, const std::map<std::string, std::vector<std::string>>& required, std::set<std::string>& visited, std::vector<std::string>& order) {
    if (!visited.insert(id).second) {
        return;
    }

    for (const std::string& dependency : required.at(id)) {
        visit(dependency, required, visited, order);
    }

    order.push_back(id);
}

} // namespace haylen::platform
