#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Exposes the asset manager to Lua as `haylen.assets`. Plugins that add asset types also register how their assets are pushed to Lua.
class AssetsPlugin final : public Plugin {
  public:
    using LuaPusher = std::function<void(lua_State*, const std::shared_ptr<void>&)>;

    AssetsPlugin();

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "assets";
    }
    void installLua(core::Engine& engine, lua_State* L) override;

    void registerLuaPusher(std::string type, LuaPusher pusher);
    void pushAsset(lua_State* L, std::string_view type, const std::shared_ptr<void>& asset) const;

  private:
    std::unordered_map<std::string, LuaPusher> pushers;
};

} // namespace haylen::plugins
