#pragma once

#include <string_view>

struct lua_State;

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

// A Lua module of the engine or of a project that extends it, which installs the `haylen.<module>` modules that Lua requires into the Lua state of an app. The modules of the engine and the modules that projects add use this same interface, and every hook runs on the frame thread.
class Module {
  public:
    virtual ~Module() = default;

    // The name that Lua requires the module by, such as `haylen.ui`, which names the module in errors.
    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;

    // Installs the module into the Lua state once the plugins of the engine started, usually with `Binding::preload`, so `require` builds it the first time a script asks for it.
    virtual void install(core::Engine& engine, lua_State* L) = 0;

    // Lets go of the Lua values that the module keeps when the app stops, before the Lua state closes.
    virtual void stop(core::Engine& engine);
};

} // namespace haylen::lua
