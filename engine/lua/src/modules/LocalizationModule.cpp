#include "modules/LocalizationModule.hpp"

#include "localization/LocalizationBinding.hpp"

namespace haylen::lua::modules {

void LocalizationModule::install(haylen::core::Engine&, lua_State* L) {
    lua::localization::LocalizationBinding::install(L);
}

} // namespace haylen::lua::modules
