#include "modules/Physics3DModule.hpp"

#include "3d/physics/Physics3DBinding.hpp"

namespace haylen::lua::modules {

void Physics3DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::physics3d::Physics3DBinding::install(L);
}

} // namespace haylen::lua::modules
