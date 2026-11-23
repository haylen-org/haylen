#include "modules/InputModule.hpp"

#include "input/InputBinding.hpp"

namespace haylen::lua::modules {

void InputModule::install(haylen::core::Engine&, lua_State* L) {
    lua::input::InputBinding::install(L);
}

} // namespace haylen::lua::modules
