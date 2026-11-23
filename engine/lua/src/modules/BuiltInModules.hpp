#pragma once

namespace haylen::lua {
class Modules;
}

namespace haylen::lua::modules {

// The Lua modules every app that runs Lua starts with.
class BuiltInModules final {
  public:
    // Adds them in dependency order, so each one installs after the modules it uses, such as the module of assets before the modules that register how their assets reach Lua.
    static void registerAll(Modules& modules);
};

} // namespace haylen::lua::modules
