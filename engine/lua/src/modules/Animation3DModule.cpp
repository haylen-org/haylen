#include "modules/Animation3DModule.hpp"

#include "3d/animation/Animation3DBinding.hpp"

namespace haylen::lua::modules {

void Animation3DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::animation3d::Animation3DBinding::install(L);
}

} // namespace haylen::lua::modules
