#include "haylen/plugins/PluginRegistry.hpp"

#include <stdexcept>
#include <string>

namespace haylen::plugins {

Plugin* PluginRegistry::find(std::string_view name) const noexcept {
    for (const Entry& entry : entries) {
        if (entry.plugin->getName() == name) {
            return entry.plugin.get();
        }
    }
    return nullptr;
}

std::vector<Plugin*> PluginRegistry::getAll() const {
    std::vector<Plugin*> plugins;
    plugins.reserve(entries.size());
    for (const Entry& entry : entries) {
        plugins.push_back(entry.plugin.get());
    }
    return plugins;
}

void PluginRegistry::clear() noexcept {
    // Plugins are released in reverse order so later plugins can still rely on the ones they were built on.
    while (!entries.empty()) {
        entries.pop_back();
    }
}

void PluginRegistry::throwMissing(std::string_view type) {
    throw std::logic_error("The plugin " + std::string(type) + " is not registered.");
}

} // namespace haylen::plugins
