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
    // Pushes JSON as plain Lua values: objects become tables with string keys, arrays become sequences with the array marker of Varn's `json` module and binary values become strings of their bytes. Throws when the value cannot be represented or, like `read`, when it is nested more than 128 levels deep, which files such as saves could otherwise use to exhaust the native stack.
    static void push(lua_State* L, const haylen::core::Json& value);

    // Throws `std::invalid_argument` when `push` would refuse the value, without a Lua state, so native code can check a value on any thread before it reaches Lua.
    static void validate(const haylen::core::Json& value);

    // Converts the Lua value at index to JSON without calling metamethods. A table with the marker of `json.array` or `json.object` becomes that kind, a sequence becomes an array and any other table becomes an object, so an empty table without a marker becomes an empty object, as in Varn's `json` module. UI properties and action maps read an empty object as an empty list. Throws `std::invalid_argument` for functions, userdata, threads, unsupported keys and cycles.
    [[nodiscard]] static haylen::core::Json read(lua_State* L, int index);

    // Pushes JSON that travels with byte buffers, where every `{"$bytes": N}` becomes a Lua string with the bytes of buffer `N`. Throws `std::invalid_argument` for a reference past the buffers.
    static void push(lua_State* L, const haylen::core::Json& value, const std::vector<std::vector<std::byte>>& buffers);

    // Reads like `read`, and turns every `haylen.Bytes` into a reference to a buffer that it appends to `buffers`.
    [[nodiscard]] static haylen::core::Json read(lua_State* L, int index, std::vector<std::vector<std::byte>>& buffers);

    // Pushes JSON that crosses the platform bridge like the `push` with buffers, and turns every data view token into the view it names. Throws `std::invalid_argument` for a token that names no live view.
    static void pushBridged(lua_State* L, const haylen::core::Json& value, const std::vector<std::vector<std::byte>>& buffers);

    // Reads JSON that crosses the platform bridge like the `read` with buffers, and turns every data view into its token, which names the view only while the Lua value keeps it alive.
    [[nodiscard]] static haylen::core::Json readBridged(lua_State* L, int index, std::vector<std::vector<std::byte>>& buffers);

  private:
    // The kind that the metatable of a table marks it as, through `json.array` and `json.object` or the arrays that `push` gives.
    enum class Marker { None, Array, Object };

    static constexpr int kMaxDepth = 128;
    static constexpr const char* kTooDeep = "JSON value is nested too deeply to push to Lua.";

    [[nodiscard]] static bool isSequence(lua_State* L, int table);
    [[nodiscard]] static Marker marker(lua_State* L, int table);
    [[nodiscard]] static std::string objectKey(lua_State* L, int index);
    [[nodiscard]] static haylen::core::Json convert(lua_State* L, int index, int depth, std::vector<std::vector<std::byte>>* buffers, bool views);
    static void pushConverted(lua_State* L, const haylen::core::Json& value, int depth, const std::vector<std::vector<std::byte>>* buffers, bool views);
    static void validateNested(const haylen::core::Json& value, int depth);
};

template <> struct Converter<haylen::core::Json> {
    static void push(lua_State* L, const haylen::core::Json& value) {
        JsonConverter::push(L, value);
    }
    static haylen::core::Json read(lua_State* L, int index) {
        return JsonConverter::read(L, index);
    }
    static bool is(lua_State* L, int index) {
        return !lua_isnone(L, index);
    }
};

} // namespace haylen::lua
