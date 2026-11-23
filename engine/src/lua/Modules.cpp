#include "haylen/lua/Modules.hpp"

#include <stdexcept>
#include <string>

namespace haylen::lua {

void Modules::clear() noexcept {
    while (!entries.empty()) {
        entries.pop_back();
    }
}

void Modules::checkName(std::string_view name) const {
    for (const Entry& entry : entries) {
        if (entry.module->getName() == name) {
            throw std::invalid_argument("The Lua module \"" + std::string(name) + "\" is already added. Give each module a name of its own.");
        }
    }
}

// A missing module has no instance that could name it, and the names of C++ types differ by compiler, so the message names neither.
void Modules::throwMissing() {
    throw std::logic_error("The Lua state has no module of the requested type. Add it with \"ScriptPlugin::addModule\" before code asks for it.");
}

} // namespace haylen::lua
