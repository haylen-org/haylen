#include "lua/modules/LocalizationModule.hpp"

#include "localization/LocalizationLua.hpp"

namespace haylen::lua::modules {

void LocalizationModule::install(core::Engine&, lua_State* L) {
    localization::LocalizationLua::install(L);
}

} // namespace haylen::lua::modules
