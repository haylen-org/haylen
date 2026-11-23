#include "modules/Physics2DModule.hpp"

#include "2d/physics/Physics2DBinding.hpp"

namespace haylen::lua::modules {

void Physics2DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::physics2d::Physics2DBinding::install(L);
}

} // namespace haylen::lua::modules
