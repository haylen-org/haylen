#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.assets`, which loads the assets of the package. The modules of plugins that add asset types also register how their assets are pushed to Lua.
class AssetsModule final : public Module {
  public:
    using Pusher = std::function<void(lua_State*, const std::shared_ptr<void>&)>;

    AssetsModule();

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.assets";
    }
    void install(core::Engine& engine, lua_State* L) override;

    // Registers how the assets of a type reach Lua, in place of what an earlier registration of the type did.
    void registerPusher(std::string type, Pusher pusher);

    // Pushes an asset of the type. Throws `std::logic_error` when no module registered how the assets of the type reach Lua.
    void pushAsset(lua_State* L, std::string_view type, const std::shared_ptr<void>& asset) const;

  private:
    std::unordered_map<std::string, Pusher> pushers;
};

} // namespace haylen::lua::modules
