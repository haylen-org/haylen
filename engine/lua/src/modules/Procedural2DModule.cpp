#include "modules/Procedural2DModule.hpp"

#include "2d/procedural/Procedural2DBinding.hpp"

namespace haylen::lua::modules {

void Procedural2DModule::install(haylen::core::Engine&, lua_State* L) {
    lua::procedural2d::Procedural2DBinding::install(L);
}

} // namespace haylen::lua::modules
