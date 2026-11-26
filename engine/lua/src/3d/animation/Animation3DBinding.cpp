#include "3d/animation/Animation3DBinding.hpp"

#include <lua.hpp>

#include "3d/animation/Clip3DBinding.hpp"
#include "3d/animation/ClipPlayer3DBinding.hpp"
#include "3d/animation/Track3DBinding.hpp"
#include "haylen/lua/Binding.hpp"

namespace haylen::lua::animation3d {

int Animation3DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTrack", &lua::Binding::native<&Track3DBinding::create>}, {"newClip", &lua::Binding::native<&Clip3DBinding::create>}, {"clipFromGltf", &lua::Binding::native<&Clip3DBinding::fromGltf>}, {"clipsFromGltf", &lua::Binding::native<&Clip3DBinding::allFromGltf>}, {"clipFromMorphSequence", &lua::Binding::native<&Clip3DBinding::fromMorphSequence>}, {"newClipPlayer", &lua::Binding::native<&ClipPlayer3DBinding::create>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Animation3DBinding::install(lua_State* L) {
    Track3DBinding::install(L);
    Clip3DBinding::install(L);
    ClipPlayer3DBinding::install(L);
    lua::Binding::preload(L, "haylen.animation3d", &open);
}

} // namespace haylen::lua::animation3d
