#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.spatial3d`, with the spatial structures of 3D, the bounding volume hierarchy of meshes and the walk of grid cells.
class Spatial3DModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.spatial3d";
    }
    void install(haylen::core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
