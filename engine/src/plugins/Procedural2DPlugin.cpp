#include "plugins/Procedural2DPlugin.hpp"

#include "2d/procedural/Procedural2DLua.hpp"

namespace haylen::plugins {

void Procedural2DPlugin::installLua(core::Engine&, lua_State* L) {
    procedural2d::Procedural2DLua::install(L);
}

} // namespace haylen::plugins
