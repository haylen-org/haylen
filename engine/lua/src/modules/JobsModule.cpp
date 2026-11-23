#include "modules/JobsModule.hpp"

#include "runtime/JobsBinding.hpp"

namespace haylen::lua::modules {

void JobsModule::install(haylen::core::Engine&, lua_State* L) {
    JobsBinding::install(L);
}

} // namespace haylen::lua::modules
