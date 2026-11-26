#include "3d/animation/Animation3DBinding.hpp"

#include <lua.hpp>

#include "3d/animation/Clip3DBinding.hpp"
#include "3d/animation/ClipPlayer3DBinding.hpp"
#include "3d/animation/MorphTargets3DBinding.hpp"
#include "3d/animation/PoseBatch3DBinding.hpp"
#include "3d/animation/Skeleton3DBinding.hpp"
#include "3d/animation/Skin3DBinding.hpp"
#include "3d/animation/Track3DBinding.hpp"
#include "haylen/lua/Binding.hpp"

namespace haylen::lua::animation3d {

int Animation3DBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTrack", &lua::Binding::native<&Track3DBinding::create>}, {"newClip", &lua::Binding::native<&Clip3DBinding::create>}, {"clipFromGltf", &lua::Binding::native<&Clip3DBinding::fromGltf>}, {"clipsFromGltf", &lua::Binding::native<&Clip3DBinding::allFromGltf>}, {"clipFromMorphSequence", &lua::Binding::native<&Clip3DBinding::fromMorphSequence>}, {"newClipPlayer", &lua::Binding::native<&ClipPlayer3DBinding::create>}, {"newSkeleton", &lua::Binding::native<&Skeleton3DBinding::create>}, {"skeletonFromGltf", &lua::Binding::native<&Skeleton3DBinding::fromGltf>}, {"newPose", &lua::Binding::native<&Skeleton3DBinding::createPose>}, {"newSkin", &lua::Binding::native<&Skin3DBinding::create>}, {"skinFromGltf", &lua::Binding::native<&Skin3DBinding::fromGltf>}, {"skinWeightsFromGltf", &lua::Binding::native<&Skin3DBinding::weightsFromGltf>}, {"newPoseBatch", &lua::Binding::native<&PoseBatch3DBinding::create>}, {"newMorphTargets", &lua::Binding::native<&MorphTargets3DBinding::create>}, {"morphTargetsFromGltf", &lua::Binding::native<&MorphTargets3DBinding::fromGltf>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    MorphTargets3DBinding::pushFaceExpressions(L);
    lua_setfield(L, -2, "faceExpressions");
    return 1;
}

void Animation3DBinding::install(lua_State* L) {
    Track3DBinding::install(L);
    Clip3DBinding::install(L);
    ClipPlayer3DBinding::install(L);
    Skeleton3DBinding::install(L);
    Skin3DBinding::install(L);
    PoseBatch3DBinding::install(L);
    MorphTargets3DBinding::install(L);
    lua::Binding::preload(L, "haylen.animation3d", &open);
}

} // namespace haylen::lua::animation3d
