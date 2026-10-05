#include "haylen/plugins/PluginRegistry.hpp"

#include <stdexcept>

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

// A missing plugin has no instance that could name it, and the names of C++ types differ by compiler, so the message names neither.
void PluginRegistry::throwMissing() {
    throw std::logic_error("The engine has no plugin of the requested type. Add it with \"Engine::addPlugin\" before code asks for it.");
}

} // namespace haylen::plugins
