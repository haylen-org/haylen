#pragma once

#include "haylen/plugins/Plugin.hpp"

namespace haylen::plugins {

// Registers the `prefab` asset type for `.prefab` files, which are checked on a worker thread, and installs the `haylen.nodes2d` module.
class Nodes2DPlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "nodes2d";
    }
    void start(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::plugins
