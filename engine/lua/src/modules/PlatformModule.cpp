#include "modules/PlatformModule.hpp"

#include "platform/AccessibilityBinding.hpp"
#include "platform/PlatformBinding.hpp"
#include "platform/SystemBinding.hpp"

namespace haylen::lua::modules {

void PlatformModule::install(haylen::core::Engine&, lua_State* L) {
    lua::platform::PlatformBinding::install(L);
    lua::platform::SystemBinding::install(L);
    lua::platform::AccessibilityBinding::install(L);
}

} // namespace haylen::lua::modules
