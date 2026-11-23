#include "lua/modules/PlatformModule.hpp"

#include "platform/AccessibilityLua.hpp"
#include "platform/PlatformLua.hpp"
#include "platform/SystemLua.hpp"

namespace haylen::lua::modules {

void PlatformModule::install(core::Engine&, lua_State* L) {
    platform::PlatformLua::install(L);
    platform::SystemLua::install(L);
    platform::AccessibilityLua::install(L);
}

} // namespace haylen::lua::modules
