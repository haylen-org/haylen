#include "support/LuaFixture.hpp"

#include <lua.hpp>

#include <stdexcept>
#include <utility>

#include "haylen/lua/Application.hpp"
#include "haylen/lua/ScriptPlugin.hpp"

namespace haylen::test {

LuaFixture::LuaFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application) : LuaFixture(std::move(files), std::move(application), Options{}) {}

LuaFixture::LuaFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options) : EngineFixture(withMain(std::move(files)), std::move(application), std::move(options), [] { return std::make_unique<lua::Application>(); }) {}

std::map<std::string, std::string> LuaFixture::withMain(std::map<std::string, std::string> files) {
    files.try_emplace("source/main.lua", "");
    return files;
}

lua_State* LuaFixture::lua() noexcept {
    return engine().getPlugin<lua::ScriptPlugin>().getState();
}

std::string LuaFixture::lua(const std::string& source) {
    lua_State* L = lua();
    const int top = lua_gettop(L);
    std::string result;
    if (luaL_loadbufferx(L, source.data(), source.size(), "=test", "t") != LUA_OK || lua_pcall(L, 0, 1, 0) != LUA_OK) {
        result = std::string("error: ") + lua_tostring(L, -1);
    } else {
        result = luaL_tolstring(L, -1, nullptr);
    }

    lua_settop(L, top);
    return result;
}

void LuaFixture::runLua(const std::string& source) {
    const std::string result = lua(source);
    if (result.starts_with("error: ")) {
        throw std::runtime_error(result);
    }
}

} // namespace haylen::test
