#include "lua/modules/StorageModule.hpp"

#include "storage/PreferencesLua.hpp"
#include "storage/StorageLua.hpp"

namespace haylen::lua::modules {

void StorageModule::install(core::Engine&, lua_State* L) {
    storage::StorageLua::install(L);
    storage::PreferencesLua::install(L);
}

} // namespace haylen::lua::modules
