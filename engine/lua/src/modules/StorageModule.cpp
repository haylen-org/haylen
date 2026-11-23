#include "modules/StorageModule.hpp"

#include "storage/PreferencesBinding.hpp"
#include "storage/StorageBinding.hpp"

namespace haylen::lua::modules {

void StorageModule::install(haylen::core::Engine&, lua_State* L) {
    lua::storage::StorageBinding::install(L);
    lua::storage::PreferencesBinding::install(L);
}

} // namespace haylen::lua::modules
