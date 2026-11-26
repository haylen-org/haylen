#pragma once

#include <string_view>

#include "haylen/lua/Module.hpp"

namespace haylen::lua::modules {

// Installs `haylen.gltf`, which reads glTF 2.0 files into documents that Lua inspects.
class GltfModule final : public Module {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.gltf";
    }
    void install(haylen::core::Engine& engine, lua_State* L) override;
};

} // namespace haylen::lua::modules
