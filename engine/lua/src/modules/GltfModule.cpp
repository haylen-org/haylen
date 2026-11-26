#include "modules/GltfModule.hpp"

#include "3d/gltf/GltfBinding.hpp"

namespace haylen::lua::modules {

void GltfModule::install(haylen::core::Engine&, lua_State* L) {
    lua::gltf::GltfBinding::install(L);
}

} // namespace haylen::lua::modules
