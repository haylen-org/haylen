#include "content/LuaCompiler.hpp"

#include <lua.hpp>

#include <memory>
#include <stdexcept>
#include <string>

#include "content/LuaBytecodeAbi.hpp"

namespace haylen::content {

std::vector<std::uint8_t> LuaCompiler::compile(std::string_view source, std::string_view path) {
    const std::unique_ptr<lua_State, decltype(&lua_close)> state(luaL_newstate(), &lua_close);
    if (state == nullptr) {
        throw std::runtime_error("Lua could not start to compile the module \"" + std::string(path) + "\".");
    }
    lua_State* L = state.get();
    const std::string name = "@" + std::string(path);
    if (luaL_loadbufferx(L, source.data(), source.size(), name.c_str(), "t") != LUA_OK) {
        throw std::invalid_argument("The Lua module \"" + std::string(path) + "\" does not compile: " + lua_tostring(L, -1) + ".");
    }

    std::vector<std::uint8_t> chunk;
    if (lua_dump(L, &LuaBytecodeAbi::append, &chunk, 0) != 0) {
        throw std::runtime_error("Lua could not write the bytecode of the module \"" + std::string(path) + "\".");
    }
    return chunk;
}

} // namespace haylen::content
