#include "modules/Nodes3DModule.hpp"

#include "3d/nodes/Nodes3DBinding.hpp"

namespace haylen::lua::modules {

void Nodes3DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::nodes3d::Nodes3DBinding::install(L);
}

} // namespace haylen::lua::modules
