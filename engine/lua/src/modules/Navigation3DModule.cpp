#include "modules/Navigation3DModule.hpp"

#include <memory>

#include "3d/navigation/NavMeshBinding.hpp"
#include "3d/navigation/Navigation3DBinding.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"

namespace haylen::lua::modules {

void Navigation3DModule::install(haylen::core::Engine& engine, lua_State* L) {
    lua::navigation3d::Navigation3DBinding::install(L);

    // The plugin follows a mesh of the cache from the moment it loads, so pushing it only hands it to Lua.
    // clang-format off
    AssetsModule& assets = engine.getPlugin<ScriptPlugin>().getModule<AssetsModule>();
    assets.registerPusher("navmesh", [](lua_State* state, const std::shared_ptr<void>& asset) {
        lua::Userdata::emplace<haylen::navigation3d::NavMesh>(state, std::static_pointer_cast<haylen::navigation3d::NavMesh>(asset));
    });
    // clang-format on
}

} // namespace haylen::lua::modules
