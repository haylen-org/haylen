#include "lua/Warnings.hpp"

#include <string_view>

#include "haylen/core/Log.hpp"

namespace haylen::lua {

void Warnings::install(lua_State* L) {
    lua_setwarnf(L, &receive, this);
}

// A warning arrives in pieces until its last one, and a warning of one piece that starts with `@` is a control message.
void Warnings::receive(void* data, const char* message, int continued) {
    Warnings& warnings = *static_cast<Warnings*>(data);
    if (warnings.pending.empty() && continued == 0 && message[0] == '@') {
        const std::string_view control(message);
        if (control == "@on" || control == "@off") {
            warnings.enabled = control == "@on";
        }
        return;
    }

    warnings.pending += message;
    if (continued != 0) {
        return;
    }
    if (warnings.enabled) {
        core::Log::warning("Lua warning: {}", warnings.pending);
    }
    warnings.pending.clear();
}

} // namespace haylen::lua
