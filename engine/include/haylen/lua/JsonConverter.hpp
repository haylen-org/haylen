#pragma once

#include <lua.hpp>

#include <cstddef>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/lua/Converter.hpp"

namespace haylen::lua {

// Converts between JSON and plain Lua values.
class JsonConverter final {
  public:
    // Pushes JSON as plain Lua values: objects become tables with string keys and arrays become sequences. Throws when the value cannot be represented or, like `read`, when it is nested more than 128 levels deep, which files such as saves could otherwise use to exhaust the native stack.
    static void push(lua_State* L, const core::Json& value);

    // Throws `std::invalid_argument` when `push` would refuse the value, without a Lua state, so native code can check a value on any thread before it reaches Lua.
    static void validate(const core::Json& value);

    // Converts the Lua value at index to JSON without calling metamethods. Sequences become arrays and any other table becomes an object, so an empty table becomes an empty object, as in Varn's `json` module. UI properties and action maps read an empty object as an empty list. Throws `std::invalid_argument` for functions, userdata, threads, unsupported keys and cycles.
    [[nodiscard]] static core::Json read(lua_State* L, int index);

    // Pushes JSON that travels with byte buffers, where every `{"$bytes": N}` becomes a Lua string with the bytes of buffer `N`. Throws `std::invalid_argument` for a reference past the buffers.
    static void push(lua_State* L, const core::Json& value, const std::vector<std::vector<std::byte>>& buffers);

    // Reads like `read`, and turns every `haylen.Bytes` into a reference to a buffer that it appends to `buffers`.
    [[nodiscard]] static core::Json read(lua_State* L, int index, std::vector<std::vector<std::byte>>& buffers);

  private:
    static constexpr int kMaxDepth = 128;
    static constexpr const char* kTooDeep = "JSON value is nested too deeply to push to Lua.";
    static constexpr const char* kBinary = "Binary JSON values cannot be converted to Lua.";

    [[nodiscard]] static bool isSequence(lua_State* L, int table);
    [[nodiscard]] static std::string objectKey(lua_State* L, int index);
    [[nodiscard]] static core::Json convert(lua_State* L, int index, int depth, std::vector<std::vector<std::byte>>* buffers);
    static void pushConverted(lua_State* L, const core::Json& value, int depth, const std::vector<std::vector<std::byte>>* buffers);
    static void validateNested(const core::Json& value, int depth);
};

template <> struct Converter<core::Json> {
    static void push(lua_State* L, const core::Json& value) {
        JsonConverter::push(L, value);
    }
    static core::Json read(lua_State* L, int index) {
        return JsonConverter::read(L, index);
    }
    static bool is(lua_State* L, int index) {
        return !lua_isnone(L, index);
    }
};

} // namespace haylen::lua
