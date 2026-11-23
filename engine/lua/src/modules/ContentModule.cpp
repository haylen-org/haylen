#include "modules/ContentModule.hpp"

#include "content/ContentBinding.hpp"

namespace haylen::lua::modules {

void ContentModule::install(haylen::core::Engine&, lua_State* L) {
    lua::content::ContentBinding::install(L);
}

} // namespace haylen::lua::modules
