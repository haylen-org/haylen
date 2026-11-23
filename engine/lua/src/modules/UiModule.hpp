#pragma once

#include <string_view>

#include "haylen/core/ScopedConnection.hpp"
#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.ui` and `haylen.imgui`, hands the events of the GUIs to the handlers of their nodes after every listener of the GUI heard them, and lets go of what Lua keeps of a GUI once it unmounted, until the script plugin lets go of the module, so the GUIs that the UI unmounts as it stops still reach Lua.
class UiModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.ui";
    }
    void install(haylen::core::Engine& engine, lua_State* L) override;

  private:
    haylen::core::ScopedConnection handlers;
    haylen::core::ScopedConnection unmounted;
};

} // namespace haylen::lua::modules
