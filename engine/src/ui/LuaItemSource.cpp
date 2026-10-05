#include "ui/LuaItemSource.hpp"

#include <lua.hpp>

#include <algorithm>
#include <stdexcept>
#include <tuple>

#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/ui/Collection.hpp"
#include "ui/CollectionLua.hpp"
#include "ui/components/collections/CollectionDiff.hpp"

namespace haylen::ui {

LuaItemSource::LuaItemSource(lua_State* L, const Gui& owner, Collection& shown) : state(lua::Runtime::getMainThread(L)), gui(owner), collection(shown) {}

void LuaItemSource::pushList(lua_State* L) const {
    CollectionLua::pushValue(L, gui, collection, "items");
}

std::uint16_t LuaItemSource::internType(std::string_view type) {
    const auto found = std::ranges::find(typeNames, type);
    if (found != typeNames.end()) {
        return static_cast<std::uint16_t>(found - typeNames.begin());
    }
    typeNames.emplace_back(type);
    return static_cast<std::uint16_t>(typeNames.size() - 1);
}

std::pair<std::string, std::uint16_t> LuaItemSource::readItem(lua_State* L, int table, std::size_t position) {
    auto [id, type] = CollectionLua::readItem(L, table, position, collection);
    return {std::move(id), internType(type)};
}

void LuaItemSource::checkIds(const std::vector<std::string>& added, std::size_t first, std::size_t last) const {
    std::vector<std::string_view> all;
    all.reserve(ids.size() - (last - first) + added.size());
    all.insert(all.end(), ids.begin(), ids.begin() + static_cast<std::ptrdiff_t>(first));
    all.insert(all.end(), added.begin(), added.end());
    all.insert(all.end(), ids.begin() + static_cast<std::ptrdiff_t>(last), ids.end());
    CollectionDiff::checkUnique(all, collection.getId());
}

void LuaItemSource::assign(lua_State* L, int list) {
    const int table = lua_absindex(L, list);
    const std::size_t count = lua_rawlen(L, table);
    std::vector<std::string> nextIds(count);
    std::vector<std::uint16_t> nextTypes(count);
    for (std::size_t index = 0; index < count; ++index) {
        lua_rawgeti(L, table, static_cast<lua_Integer>(index + 1));
        std::tie(nextIds[index], nextTypes[index]) = readItem(L, -1, index + 1);
        lua_pop(L, 1);
    }
    const std::vector<std::string_view> before(ids.begin(), ids.end());
    const std::vector<std::string_view> after(nextIds.begin(), nextIds.end());
    const CollectionDiff::Result result = CollectionDiff::compare(before, after, collection.getId());

    CollectionLua::setValue(L, gui, collection, "items", table);
    ids = std::move(nextIds);
    types = std::move(nextTypes);
    changed.emit({.kind = Change::Kind::Replaced, .index = 0, .count = ids.size(), .target = 0, .previous = result.previous});
}

// The items of the list move up to make room, the way `table.insert` moves them.
void LuaItemSource::insert(lua_State* L, std::size_t index, int added) {
    const int source = lua_absindex(L, added);
    const std::size_t count = lua_rawlen(L, source);
    std::vector<std::string> newIds(count);
    std::vector<std::uint16_t> newTypes(count);
    for (std::size_t offset = 0; offset < count; ++offset) {
        lua_rawgeti(L, source, static_cast<lua_Integer>(offset + 1));
        std::tie(newIds[offset], newTypes[offset]) = readItem(L, -1, index + offset + 1);
        lua_pop(L, 1);
    }
    checkIds(newIds, index, index);

    pushList(L);
    const int list = lua_gettop(L);
    const auto shift = static_cast<lua_Integer>(count);
    for (auto position = static_cast<lua_Integer>(ids.size()); position > static_cast<lua_Integer>(index); --position) {
        lua_rawgeti(L, list, position);
        lua_rawseti(L, list, position + shift);
    }
    for (std::size_t offset = 0; offset < count; ++offset) {
        lua_rawgeti(L, source, static_cast<lua_Integer>(offset + 1));
        lua_rawseti(L, list, static_cast<lua_Integer>(index + offset + 1));
    }
    lua_pop(L, 1);
    ids.insert(ids.begin() + static_cast<std::ptrdiff_t>(index), std::make_move_iterator(newIds.begin()), std::make_move_iterator(newIds.end()));
    types.insert(types.begin() + static_cast<std::ptrdiff_t>(index), newTypes.begin(), newTypes.end());
    changed.emit({.kind = Change::Kind::Inserted, .index = index, .count = count});
}

void LuaItemSource::remove(lua_State* L, std::size_t index, std::size_t count) {
    const std::size_t removed = std::min(count, ids.size() - index);
    pushList(L);
    const int list = lua_gettop(L);
    const auto total = static_cast<lua_Integer>(ids.size());
    const auto shift = static_cast<lua_Integer>(removed);
    for (auto position = static_cast<lua_Integer>(index + removed + 1); position <= total; ++position) {
        lua_rawgeti(L, list, position);
        lua_rawseti(L, list, position - shift);
    }
    for (lua_Integer position = total - shift + 1; position <= total; ++position) {
        lua_pushnil(L);
        lua_rawseti(L, list, position);
    }
    lua_pop(L, 1);
    const auto first = static_cast<std::ptrdiff_t>(index);
    ids.erase(ids.begin() + first, ids.begin() + first + static_cast<std::ptrdiff_t>(removed));
    types.erase(types.begin() + first, types.begin() + first + static_cast<std::ptrdiff_t>(removed));
    changed.emit({.kind = Change::Kind::Removed, .index = index, .count = removed});
}

void LuaItemSource::move(lua_State* L, std::size_t from, std::size_t to) {
    pushList(L);
    const int list = lua_gettop(L);
    lua_rawgeti(L, list, static_cast<lua_Integer>(from + 1));
    const int step = from < to ? 1 : -1;
    for (auto position = static_cast<lua_Integer>(from + 1); position != static_cast<lua_Integer>(to + 1); position += step) {
        lua_rawgeti(L, list, position + step);
        lua_rawseti(L, list, position);
    }
    lua_rawseti(L, list, static_cast<lua_Integer>(to + 1));
    lua_pop(L, 1);

    const auto source = ids.begin() + static_cast<std::ptrdiff_t>(from);
    const auto target = ids.begin() + static_cast<std::ptrdiff_t>(to);
    const auto typeSource = types.begin() + static_cast<std::ptrdiff_t>(from);
    const auto typeTarget = types.begin() + static_cast<std::ptrdiff_t>(to);
    if (from < to) {
        std::rotate(source, source + 1, target + 1);
        std::rotate(typeSource, typeSource + 1, typeTarget + 1);
    } else {
        std::rotate(target, source, source + 1);
        std::rotate(typeTarget, typeSource, typeSource + 1);
    }
    changed.emit({.kind = Change::Kind::Moved, .index = from, .count = 1, .target = to});
}

// An item that keeps its id changes in place and keeps its cell, and an item with another id takes the place of the one it replaces.
void LuaItemSource::replace(lua_State* L, std::size_t index, int item) {
    auto [id, type] = readItem(L, item, index + 1);
    checkIds({id}, index, index + 1);
    pushList(L);
    lua_pushvalue(L, item);
    lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    lua_pop(L, 1);

    const bool same = ids[index] == id;
    ids[index] = std::move(id);
    types[index] = type;
    if (same) {
        changed.emit({.kind = Change::Kind::Changed, .index = index, .count = 1});
        return;
    }
    std::vector<std::size_t> previous(ids.size());
    for (std::size_t position = 0; position < previous.size(); ++position) {
        previous[position] = position == index ? Change::kNew : position;
    }
    changed.emit({.kind = Change::Kind::Replaced, .index = 0, .count = ids.size(), .target = 0, .previous = previous});
}

void LuaItemSource::reload(lua_State* L, std::size_t index) {
    pushItem(L, index);
    auto [id, type] = readItem(L, -1, index + 1);
    lua_pop(L, 1);
    checkIds({id}, index, index + 1);
    ids[index] = std::move(id);
    types[index] = type;
    changed.emit({.kind = Change::Kind::Changed, .index = index, .count = 1});
}

void LuaItemSource::pushItem(lua_State* L, std::size_t index) const {
    pushList(L);
    lua_rawgeti(L, -1, static_cast<lua_Integer>(index + 1));
    lua_remove(L, -2);
}

core::Json LuaItemSource::getValue(std::size_t index, std::string_view field) const {
    core::Json value;
    // clang-format off
    lua::Runtime::protectedRun(state, [&](lua_State* L) {
        pushItem(L, index);
        if (lua_istable(L, -1)) {
            lua_pushlstring(L, field.data(), field.size());
            lua_rawget(L, -2);
            value = lua::JsonConverter::read(L, -1);
        }
    });
    // clang-format on
    return value;
}

void LuaItemSource::setValue(std::size_t index, std::string_view field, const core::Json& value) {
    // clang-format off
    lua::Runtime::protectedRun(state, [&](lua_State* L) {
        pushItem(L, index);
        if (lua_istable(L, -1)) {
            lua_pushlstring(L, field.data(), field.size());
            lua::JsonConverter::push(L, value);
            lua_rawset(L, -3);
        }
    });
    // clang-format on
}

} // namespace haylen::ui
