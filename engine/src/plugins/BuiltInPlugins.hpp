#pragma once

namespace haylen::platform {
class Host;
}

namespace haylen::plugins {

class PluginRegistry;

// The plugins every engine starts with.
class BuiltInPlugins final {
  public:
    // Adds them in dependency order, so each one starts after the plugins it uses, with what they need to know about the platform.
    static void registerAll(PluginRegistry& registry, const platform::Host& host);
};

} // namespace haylen::plugins
