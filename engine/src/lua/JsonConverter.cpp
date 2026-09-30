#include "haylen/lua/JsonConverter.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include "haylen/core/JsonBytes.hpp"
#include "haylen/lua/Bytes.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

// Scans the table without metamethods, so conversion never runs script code.
bool JsonConverter::isSequence(lua_State* L, int table) {
    const auto length = static_cast<lua_Integer>(lua_rawlen(L, table));
    if (length == 0) {
        return false;
    }

    lua_Integer count = 0;
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        lua_pop(L, 1);
        if (lua_isinteger(L, -1) == 0) {
            lua_pop(L, 1);
            return false;
        }
        ++count;
    }
    return count == length;
}

std::string JsonConverter::objectKey(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TSTRING) {
        std::size_t length = 0;
        const char* text = lua_tolstring(L, index, &length);
        return {text, length};
    }
    if (lua_type(L, index) == LUA_TNUMBER) {
        // The key is converted from a copy so `lua_tolstring` never changes the key that `lua_next` expects.
        lua_pushvalue(L, index);
        std::string key = lua_tostring(L, -1);
        lua_pop(L, 1);
        return key;
    }
    throw std::invalid_argument(std::string("A ") + luaL_typename(L, index) + " key cannot be converted to JSON.");
}

core::Json JsonConverter::convert(lua_State* L, int index, int depth, std::vector<std::vector<std::byte>>* buffers) {
    if (depth > kMaxDepth) {
        throw std::invalid_argument("Value is nested too deeply to convert to JSON.");
    }

    switch (lua_type(L, index)) {
    case LUA_TNIL:
    case LUA_TNONE:
        return nullptr;
    case LUA_TBOOLEAN:
        return lua_toboolean(L, index) != 0;
    case LUA_TNUMBER:
        if (lua_isinteger(L, index) != 0) {
            return static_cast<std::int64_t>(lua_tointeger(L, index));
        }
        return lua_tonumber(L, index);
    case LUA_TSTRING: {
        std::size_t length = 0;
        const char* text = lua_tolstring(L, index, &length);
        return std::string(text, length);
    }
    case LUA_TTABLE:
        break;
    case LUA_TUSERDATA:
        if (const Bytes* bytes = buffers != nullptr ? Userdata::test<Bytes>(L, index) : nullptr) {
            buffers->push_back(bytes->data);
            return core::JsonBytes::makeReference(buffers->size() - 1);
        }
        [[fallthrough]];
    default:
        throw std::invalid_argument(std::string("A ") + luaL_typename(L, index) + " cannot be converted to JSON.");
    }

    if (lua_checkstack(L, 4) == 0) {
        throw std::invalid_argument("Value is nested too deeply to convert to JSON.");
    }

    const int table = lua_absindex(L, index);
    if (isSequence(L, table)) {
        core::Json array = core::Json::array();
        const auto length = static_cast<lua_Integer>(lua_rawlen(L, table));
        for (lua_Integer element = 1; element <= length; ++element) {
            lua_rawgeti(L, table, element);
            array.push_back(convert(L, -1, depth + 1, buffers));
            lua_pop(L, 1);
        }
        return array;
    }

    core::Json object = core::Json::object();
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        std::string key = objectKey(L, -2);
        object[std::move(key)] = convert(L, -1, depth + 1, buffers);
        lua_pop(L, 1);
    }
    return object;
}

void JsonConverter::pushConverted(lua_State* L, const core::Json& value, int depth, const std::vector<std::vector<std::byte>>* buffers) {
    if (depth > kMaxDepth || lua_checkstack(L, 3) == 0) {
        throw std::invalid_argument(kTooDeep);
    }

    switch (value.type()) {
    case core::Json::value_t::null:
    case core::Json::value_t::discarded:
        lua_pushnil(L);
        return;
    case core::Json::value_t::boolean:
        lua_pushboolean(L, value.get<bool>() ? 1 : 0);
        return;
    case core::Json::value_t::number_integer:
        lua_pushinteger(L, static_cast<lua_Integer>(value.get<std::int64_t>()));
        return;
    case core::Json::value_t::number_unsigned: {
        // Integers above the range of Lua integers become floats rather than wrapping to negative numbers.
        const auto number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max())) {
            lua_pushnumber(L, static_cast<lua_Number>(number));
        } else {
            lua_pushinteger(L, static_cast<lua_Integer>(number));
        }
        return;
    }
    case core::Json::value_t::number_float:
        lua_pushnumber(L, value.get<double>());
        return;
    case core::Json::value_t::string: {
        const auto& text = value.get_ref<const std::string&>();
        lua_pushlstring(L, text.data(), text.size());
        return;
    }
    case core::Json::value_t::binary:
        throw std::invalid_argument(kBinary);
    case core::Json::value_t::array:
        lua_createtable(L, static_cast<int>(value.size()), 0);
        for (std::size_t element = 0; element < value.size(); ++element) {
            pushConverted(L, value[element], depth + 1, buffers);
            lua_rawseti(L, -2, static_cast<lua_Integer>(element + 1));
        }
        return;
    case core::Json::value_t::object:
        if (const std::optional<std::size_t> reference = buffers != nullptr ? core::JsonBytes::findReference(value) : std::nullopt) {
            core::JsonBytes::validate(value, buffers->size());
            const std::vector<std::byte>& bytes = (*buffers)[*reference];
            lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
            return;
        }
        lua_createtable(L, 0, static_cast<int>(value.size()));
        for (const auto& [key, member] : value.items()) {
            lua_pushlstring(L, key.data(), key.size());
            pushConverted(L, member, depth + 1, buffers);
            lua_rawset(L, -3);
        }
        return;
    }
}

void JsonConverter::validateNested(const core::Json& value, int depth) {
    if (depth > kMaxDepth) {
        throw std::invalid_argument(kTooDeep);
    }
    if (value.is_binary()) {
        throw std::invalid_argument(kBinary);
    }
    if (value.is_structured()) {
        for (const core::Json& element : value) {
            validateNested(element, depth + 1);
        }
    }
}

void JsonConverter::push(lua_State* L, const core::Json& value) {
    pushConverted(L, value, 0, nullptr);
}

void JsonConverter::push(lua_State* L, const core::Json& value, const std::vector<std::vector<std::byte>>& buffers) {
    pushConverted(L, value, 0, &buffers);
}

void JsonConverter::validate(const core::Json& value) {
    validateNested(value, 0);
}

core::Json JsonConverter::read(lua_State* L, int index) {
    return convert(L, index, 0, nullptr);
}

core::Json JsonConverter::read(lua_State* L, int index, std::vector<std::vector<std::byte>>& buffers) {
    return convert(L, index, 0, &buffers);
}

} // namespace haylen::lua
