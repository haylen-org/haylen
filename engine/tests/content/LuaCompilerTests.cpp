#include <gtest/gtest.h>
#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "content/LuaCompiler.hpp"

namespace haylen::content {

class LuaCompilerTest : public ::testing::Test {
  protected:
    LuaCompilerTest() : state(luaL_newstate(), &lua_close) {
        luaL_openlibs(state.get());
    }

    // Loads bytes as a chunk in a mode and returns the message of Lua, or the empty text when they load.
    [[nodiscard]] std::string load(const std::vector<std::uint8_t>& bytes, const char* mode) const {
        if (luaL_loadbufferx(state.get(), reinterpret_cast<const char*>(bytes.data()), bytes.size(), "=chunk", mode) == LUA_OK) {
            return "";
        }
        std::string message = lua_tostring(state.get(), -1);
        lua_pop(state.get(), 1);
        return message;
    }

    std::unique_ptr<lua_State, decltype(&lua_close)> state;
};

TEST_F(LuaCompilerTest, CompilesModulesIntoBytecodeThatKeepsTheirNamesAndLines) {
    const std::vector<std::uint8_t> chunk = LuaCompiler::compile("local menu = {}\nfunction menu.open(value)\n    return value.title\nend\nreturn menu", "source/scenes/menu.lua");
    ASSERT_GT(chunk.size(), 4U);
    EXPECT_EQ(std::string(chunk.begin(), chunk.begin() + 4), LUA_SIGNATURE);
    EXPECT_EQ(LuaCompiler::compile("local menu = {}\nfunction menu.open(value)\n    return value.title\nend\nreturn menu", "source/scenes/menu.lua"), chunk) << "The same module always compiles to the same bytes.";
    EXPECT_NE(load(chunk, "t").find("binary chunk"), std::string::npos) << "Text loading never runs bytecode.";

    // The module keeps its package path, its lines and the names of its locals, so its errors read as they do from the source.
    ASSERT_EQ(load(chunk, "b"), "");
    lua_State* L = state.get();
    lua_call(L, 0, 1);
    lua_getfield(L, -1, "open");
    lua_pushnil(L);
    ASSERT_NE(lua_pcall(L, 1, 0, 0), LUA_OK);
    EXPECT_STREQ(lua_tostring(L, -1), "source/scenes/menu.lua:3: attempt to index a nil value (local 'value')");
}

TEST_F(LuaCompilerTest, NamesTheModuleAndTheLineOfASyntaxError) {
    try {
        (void)LuaCompiler::compile("local menu = {}\nlocal = 1\n", "source/scenes/menu.lua");
        FAIL() << "A module that does not compile must fail.";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string(error.what()).find("The Lua module \"source/scenes/menu.lua\" does not compile: source/scenes/menu.lua:2:"), std::string::npos) << error.what();
    }
}

TEST_F(LuaCompilerTest, DescribesTheAbiOfItsChunks) {
    // Every host that builds releases runs a little-endian Lua with 32-bit instructions and 64-bit integers and numbers.
    EXPECT_EQ(LuaCompiler::getAbi(), "lua-" + std::to_string(LUA_VERSION_NUM / 100) + "." + std::to_string(LUA_VERSION_NUM % 100) + "-f0-i4-x4-l8-n8-le");
}

} // namespace haylen::content
