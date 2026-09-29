#pragma once

namespace haylen::plugins {

class PluginRegistry;

// The plugins every engine starts with.
class BuiltInPlugins final {
  public:
    // Adds them in dependency order, so each one starts after the plugins it uses.
    static void registerAll(PluginRegistry& registry);
};

} // namespace haylen::plugins
