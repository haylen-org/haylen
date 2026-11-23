#include "haylen/lua/JsonConverter.hpp"

#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

#include "haylen/core/JsonBytes.hpp"
#include "haylen/lua/Bytes.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/dataview/DataViewJson.h"
#include "varn/dataview/DataViewModule.h"
#include "varn/json/JsonSerializer.h"

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

// Reads the metatable without metamethods, so the marker of Varn's `json.array` or `json.object` decides the kind of a table that its keys leave open, such as an empty one.
JsonConverter::Marker JsonConverter::marker(lua_State* L, int table) {
    if (lua_getmetatable(L, table) == 0) {
        return Marker::None;
    }

    luaL_getmetatable(L, varn::json::JsonSerializer::kArrayMetatable);
    luaL_getmetatable(L, varn::json::JsonSerializer::kObjectMetatable);
    Marker found = Marker::None;
    if (lua_rawequal(L, -3, -2) != 0) {
        found = Marker::Array;
    } else if (lua_rawequal(L, -3, -1) != 0) {
        found = Marker::Object;
    }

    lua_pop(L, 3);
    return found;
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

haylen::core::Json JsonConverter::convert(lua_State* L, int index, int depth, std::vector<std::vector<std::byte>>* buffers, bool views) {
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
            return haylen::core::JsonBytes::makeReference(buffers->size() - 1);
        }

        if (views && luaL_testudata(L, index, varn::dataview::DataViewModule::kMetatable) != nullptr) {
            const varn::dataview::DataView* view = varn::dataview::DataViewModule::test(L, index);
            if (view == nullptr) {
                throw std::invalid_argument("A data view that was released cannot cross the platform bridge.");
            }

            return varn::dataview::DataViewJson::write(*view);
        }

        [[fallthrough]];
    default:
        throw std::invalid_argument(std::string("A ") + luaL_typename(L, index) + " cannot be converted to JSON.");
    }

    if (lua_checkstack(L, 4) == 0) {
        throw std::invalid_argument("Value is nested too deeply to convert to JSON.");
    }

    const int table = lua_absindex(L, index);
    const Marker kind = marker(L, table);
    if (kind == Marker::Array || (kind == Marker::None && isSequence(L, table))) {
        haylen::core::Json array = haylen::core::Json::array();
        const auto length = static_cast<lua_Integer>(lua_rawlen(L, table));
        for (lua_Integer element = 1; element <= length; ++element) {
            lua_rawgeti(L, table, element);
            array.push_back(convert(L, -1, depth + 1, buffers, views));
            lua_pop(L, 1);
        }

        return array;
    }

    haylen::core::Json object = haylen::core::Json::object();
    lua_pushnil(L);
    while (lua_next(L, table) != 0) {
        std::string key = objectKey(L, -2);
        object[std::move(key)] = convert(L, -1, depth + 1, buffers, views);
        lua_pop(L, 1);
    }

    return object;
}

void JsonConverter::pushConverted(lua_State* L, const haylen::core::Json& value, int depth, const std::vector<std::vector<std::byte>>* buffers, bool views) {
    if (depth > kMaxDepth || lua_checkstack(L, 3) == 0) {
        throw std::invalid_argument(kTooDeep);
    }

    switch (value.type()) {
    case haylen::core::Json::value_t::null:
    case haylen::core::Json::value_t::discarded:
        lua_pushnil(L);
        return;
    case haylen::core::Json::value_t::boolean:
        lua_pushboolean(L, value.get<bool>() ? 1 : 0);
        return;
    case haylen::core::Json::value_t::number_integer:
        lua_pushinteger(L, static_cast<lua_Integer>(value.get<std::int64_t>()));
        return;
    case haylen::core::Json::value_t::number_unsigned: {
        // Integers above the range of Lua integers become floats rather than wrapping to negative numbers.
        const auto number = value.get<std::uint64_t>();
        if (number > static_cast<std::uint64_t>(std::numeric_limits<lua_Integer>::max())) {
            lua_pushnumber(L, static_cast<lua_Number>(number));
        } else {
            lua_pushinteger(L, static_cast<lua_Integer>(number));
        }

        return;
    }
    case haylen::core::Json::value_t::number_float:
        lua_pushnumber(L, value.get<double>());
        return;
    case haylen::core::Json::value_t::string: {
        const auto& text = value.get_ref<const std::string&>();
        lua_pushlstring(L, text.data(), text.size());
        return;
    }
    case haylen::core::Json::value_t::binary: {
        const haylen::core::Json::binary_t& bytes = value.get_binary();
        lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
        return;
    }
    case haylen::core::Json::value_t::array:
        // An array carries the array marker of Varn, as the arrays of `json.decode` do, so it converts back to an array even when it is empty.
        lua_createtable(L, static_cast<int>(value.size()), 0);
        luaL_newmetatable(L, varn::json::JsonSerializer::kArrayMetatable);
        lua_setmetatable(L, -2);
        for (std::size_t element = 0; element < value.size(); ++element) {
            pushConverted(L, value[element], depth + 1, buffers, views);
            lua_rawseti(L, -2, static_cast<lua_Integer>(element + 1));
        }

        return;
    case haylen::core::Json::value_t::object:
        if (const std::optional<std::size_t> reference = buffers != nullptr ? haylen::core::JsonBytes::findReference(value) : std::nullopt) {
            haylen::core::JsonBytes::validate(value, buffers->size());
            const std::vector<std::byte>& bytes = (*buffers)[*reference];
            lua_pushlstring(L, reinterpret_cast<const char*>(bytes.data()), bytes.size());
            return;
        }

        if (const std::optional<varn::dataview::DataView::Token> token = views ? varn::dataview::DataViewJson::read(value) : std::nullopt) {
            varn::dataview::DataView view = varn::dataview::DataView::acquire(*token);
            if (!view) {
                throw std::invalid_argument(std::format("The JSON names the data view {}, which is no longer alive.", token->id));
            }

            varn::dataview::DataViewModule::push(L, std::move(view));
            return;
        }

        lua_createtable(L, 0, static_cast<int>(value.size()));
        for (const auto& [key, member] : value.items()) {
            lua_pushlstring(L, key.data(), key.size());
            pushConverted(L, member, depth + 1, buffers, views);
            lua_rawset(L, -3);
        }

        return;
    }
}

void JsonConverter::validateNested(const haylen::core::Json& value, int depth) {
    if (depth > kMaxDepth) {
        throw std::invalid_argument(kTooDeep);
    }

    if (value.is_structured()) {
        for (const haylen::core::Json& element : value) {
            validateNested(element, depth + 1);
        }
    }
}

void JsonConverter::push(lua_State* L, const haylen::core::Json& value) {
    pushConverted(L, value, 0, nullptr, false);
}

void JsonConverter::push(lua_State* L, const haylen::core::Json& value, const std::vector<std::vector<std::byte>>& buffers) {
    pushConverted(L, value, 0, &buffers, false);
}

void JsonConverter::pushBridged(lua_State* L, const haylen::core::Json& value, const std::vector<std::vector<std::byte>>& buffers) {
    pushConverted(L, value, 0, &buffers, true);
}

void JsonConverter::validate(const haylen::core::Json& value) {
    validateNested(value, 0);
}

haylen::core::Json JsonConverter::read(lua_State* L, int index) {
    return convert(L, index, 0, nullptr, false);
}

haylen::core::Json JsonConverter::read(lua_State* L, int index, std::vector<std::vector<std::byte>>& buffers) {
    return convert(L, index, 0, &buffers, false);
}

haylen::core::Json JsonConverter::readBridged(lua_State* L, int index, std::vector<std::vector<std::byte>>& buffers) {
    return convert(L, index, 0, &buffers, true);
}

} // namespace haylen::lua
