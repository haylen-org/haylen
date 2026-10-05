#include "ui/CollectionLua.hpp"

#include <lua.hpp>

#include <stdexcept>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/Gui.hpp"
#include "lua/StackScope.hpp"
#include "ui/LuaItemSource.hpp"
#include "ui/LuaPageSource.hpp"
#include "ui/TransformLua.hpp"

namespace haylen::ui {

bool CollectionLua::pushEntry(lua_State* L, const Gui& gui, const Collection& collection, bool create) {
    lua_getfield(L, LUA_REGISTRYINDEX, kValuesKey);
    if (lua_rawgetp(L, -1, &gui) != LUA_TTABLE) {
        lua_pop(L, 1);
        if (!create) {
            lua_pop(L, 1);
            return false;
        }
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_rawsetp(L, -3, &gui);
    }
    if (lua_rawgetp(L, -1, &collection) != LUA_TTABLE) {
        lua_pop(L, 1);
        if (!create) {
            lua_pop(L, 2);
            return false;
        }
        lua_createtable(L, 0, 6);
        lua::Stack::push(L, collection.getId());
        lua_setfield(L, -2, "id");
        lua_newtable(L);
        lua_setfield(L, -2, "binders");
        lua_pushvalue(L, -1);
        lua_rawsetp(L, -3, &collection);
    }
    lua_replace(L, -3);
    lua_pop(L, 1);
    return true;
}

void CollectionLua::pushValue(lua_State* L, const Gui& gui, const Collection& collection, const char* field) {
    if (!pushEntry(L, gui, collection, false)) {
        lua_pushnil(L);
        return;
    }
    lua_getfield(L, -1, field);
    lua_remove(L, -2);
}

void CollectionLua::setValue(lua_State* L, const Gui& gui, const Collection& collection, const char* field, int value) {
    const int stored = value == 0 ? 0 : lua_absindex(L, value);
    (void)pushEntry(L, gui, collection, true);
    if (stored == 0) {
        lua_pushnil(L);
    } else {
        lua_pushvalue(L, stored);
    }
    lua_setfield(L, -2, field);
    lua_pop(L, 1);
}

void CollectionLua::pushPage(lua_State* L, const Gui& gui, const Collection& collection, std::size_t page) {
    pushValue(L, gui, collection, "pages");
    if (lua_istable(L, -1)) {
        lua_rawgeti(L, -1, static_cast<lua_Integer>(page + 1));
        lua_remove(L, -2);
    }
}

void CollectionLua::setPage(lua_State* L, const Gui& gui, const Collection& collection, std::size_t page, int list) {
    const int stored = list == 0 ? 0 : lua_absindex(L, list);
    const lua::StackScope scope(L);
    pushValue(L, gui, collection, "pages");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        setValue(L, gui, collection, "pages", -1);
    }
    if (stored == 0) {
        lua_pushnil(L);
    } else {
        lua_pushvalue(L, stored);
    }
    lua_rawseti(L, -2, static_cast<lua_Integer>(page + 1));
}

std::pair<std::string, std::string> CollectionLua::readItem(lua_State* L, int table, std::size_t position, const Collection& collection) {
    const int item = lua_absindex(L, table);
    const std::string where = "The item at index " + std::to_string(position) + " of the collection \"" + collection.getId() + "\" ";
    if (lua_type(L, item) != LUA_TTABLE) {
        throw std::invalid_argument(where + "must be a table.");
    }
    lua_pushliteral(L, "id");
    const bool named = lua_rawget(L, item) == LUA_TSTRING && lua_rawlen(L, -1) > 0;
    std::string id = named ? lua_tostring(L, -1) : std::string();
    lua_pop(L, 1);
    if (!named) {
        throw std::invalid_argument(where + "needs a non-empty string id.");
    }

    lua_pushliteral(L, "type");
    const int kind = lua_rawget(L, item);
    if (kind != LUA_TNIL && kind != LUA_TSTRING) {
        throw std::invalid_argument("The type of the item \"" + id + "\" of the collection \"" + collection.getId() + "\" must be a string.");
    }
    std::string type = kind == LUA_TSTRING ? lua_tostring(L, -1) : std::string();
    lua_pop(L, 1);
    collection.checkType(id, type);
    return {std::move(id), std::move(type)};
}

void CollectionLua::forget(lua_State* L, const Gui& gui) {
    lua_getfield(L, LUA_REGISTRYINDEX, kValuesKey);
    lua_pushnil(L);
    lua_rawsetp(L, -2, &gui);
    lua_pop(L, 1);
}

// The entries are keyed by the address of their collection, which the GUI finds again by id while the node lives.
void CollectionLua::prune(lua_State* L, const Gui& gui) {
    const lua::StackScope scope(L);
    lua_getfield(L, LUA_REGISTRYINDEX, kValuesKey);
    if (lua_rawgetp(L, -1, &gui) != LUA_TTABLE) {
        return;
    }
    const int entries = lua_gettop(L);
    std::vector<const void*> stale;
    lua_pushnil(L);
    while (lua_next(L, entries) != 0) {
        lua_getfield(L, -1, "id");
        const Component* found = gui.find(lua::Stack::read<std::string_view>(L, -1));
        if (found != lua_touserdata(L, -3)) {
            stale.push_back(lua_touserdata(L, -3));
        }
        lua_pop(L, 2);
    }
    for (const void* key : stale) {
        lua_pushnil(L);
        lua_rawsetp(L, entries, key);
    }
}

void CollectionLua::push(lua_State* L, const std::shared_ptr<Gui>& gui, Collection& collection) {
    (void)pushEntry(L, *gui, collection, true);
    if (lua_getfield(L, -1, "handle") == LUA_TUSERDATA) {
        lua_remove(L, -2);
        return;
    }
    lua_pop(L, 1);
    lua::Userdata::emplace<Handle>(L, Handle{.gui = gui, .collection = &collection, .id = collection.getId()});
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, "handle");
    lua_remove(L, -2);
}

std::shared_ptr<Gui> CollectionLua::checkGui(lua_State* L) {
    const Handle& handle = lua::Userdata::check<Handle>(L, 1);
    std::shared_ptr<Gui> gui = handle.gui.lock();
    if (!gui || !lua::Runtime::getEngine(L).getPlugin<plugins::UiPlugin>().isMounted(*gui)) {
        luaL_error(L, "The GUI is not mounted.");
    }
    if (gui->find(handle.id) != handle.collection) {
        luaL_error(L, "This \"%s\" was already released.", lua::Type<Handle>::name);
    }
    return gui;
}

Collection& CollectionLua::check(lua_State* L) {
    (void)checkGui(L);
    return *lua::Userdata::check<Handle>(L, 1).collection;
}

std::size_t CollectionLua::readIndex(lua_State* L, int index, const Collection& collection, std::size_t limit) {
    const auto value = static_cast<lua_Integer>(luaL_checkinteger(L, index));
    if (value < 1 || static_cast<std::size_t>(value) > limit) {
        luaL_error(L, "The index %d is outside the items of the collection \"%s\", which holds %d.", static_cast<int>(value), collection.getId().c_str(), static_cast<int>(collection.getCount()));
    }
    return static_cast<std::size_t>(value - 1);
}

// A target is the id of an item or its index from 1.
std::size_t CollectionLua::readTarget(lua_State* L, int index, const Collection& collection) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        return readIndex(L, index, collection, collection.getCount());
    }
    const std::string id = lua::Stack::read<std::string>(L, index);
    const std::optional<std::size_t> found = collection.findItem(id);
    if (!found) {
        luaL_error(L, "The collection \"%s\" has no item with the id \"%s\".", collection.getId().c_str(), id.c_str());
    }
    return *found;
}

void CollectionLua::checkUnbound(lua_State* L, const Collection& collection) {
    if (collection.isBinding()) {
        luaL_error(L, "The collection \"%s\" cannot change its items while it binds cells.", collection.getId().c_str());
    }
}

LuaItemSource& CollectionLua::requireList(lua_State* L, Collection& collection) {
    checkUnbound(L, collection);
    if (std::dynamic_pointer_cast<LuaPageSource>(collection.getSource())) {
        luaL_error(L, "The collection \"%s\" pages its items, so it changes them with \"setPages\".", collection.getId().c_str());
    }
    auto* list = dynamic_cast<LuaItemSource*>(collection.getSource().get());
    if (list == nullptr) {
        luaL_error(L, "The collection \"%s\" has no list of items. Give it one with \"setItems\".", collection.getId().c_str());
    }
    return *list;
}

void CollectionLua::pushItem(lua_State* L, const Collection& collection, std::size_t index) {
    if (const auto* list = dynamic_cast<const LuaItemSource*>(collection.getSource().get())) {
        list->pushItem(L, index);
    } else if (const auto* pages = dynamic_cast<const LuaPageSource*>(collection.getSource().get())) {
        pages->pushItem(L, index);
    } else {
        lua_pushnil(L);
    }
}

void CollectionLua::pushCell(lua_State* L, CollectionCell& cell) {
    lua::Userdata::emplace<CellHandle>(L, CellHandle{.cell = cell.weak_from_this(), .generation = cell.getGeneration(), .item = cell.getItem()});
}

CollectionCell& CollectionLua::checkCell(lua_State* L) {
    const CellHandle& handle = lua::Userdata::check<CellHandle>(L, 1);
    const std::shared_ptr<CollectionCell> cell = handle.cell.lock();
    if (!cell || cell->getGeneration() != handle.generation) {
        luaL_error(L, "This cell no longer shows the item \"%s\".", handle.item.c_str());
    }
    return *cell;
}

void CollectionLua::callBinder(lua_State* L, const Gui& gui, const Collection& collection, const std::string& type, CollectionCell& cell, std::size_t index) {
    const lua::StackScope scope(L);
    pushValue(L, gui, collection, "binders");
    if (!lua_istable(L, -1) || lua_getfield(L, -1, type.c_str()) != LUA_TFUNCTION) {
        return;
    }
    pushCell(L, cell);
    pushItem(L, collection, index);
    lua_pushinteger(L, static_cast<lua_Integer>(index + 1));
    lua::Runtime::protectedCall(L, 3, 0);
}

// The same list changed in place, or another list, compares with the items shown before by id.
int CollectionLua::setItems(lua_State* L) {
    const std::shared_ptr<Gui> gui = checkGui(L);
    Collection& collection = check(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    checkUnbound(L, collection);
    if (const auto list = std::dynamic_pointer_cast<LuaItemSource>(collection.getSource())) {
        list->assign(L, 2);
        return 0;
    }
    auto created = std::make_shared<LuaItemSource>(L, *gui, collection);
    created->assign(L, 2);
    collection.setSource(created);
    setValue(L, *gui, collection, "load", 0);
    setValue(L, *gui, collection, "pages", 0);
    return 0;
}

// A second call with the same loader keeps the pages it loaded, and another loader starts over.
int CollectionLua::setPages(lua_State* L) {
    const std::shared_ptr<Gui> gui = checkGui(L);
    Collection& collection = check(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kPageFields});
    checkUnbound(L, collection);
    LuaPageSource::Options options;
    if (lua_getfield(L, 2, "count") == LUA_TNIL) {
        return luaL_error(L, "The option \"count\" is required.");
    }
    lua_pop(L, 1);
    lua_Integer count = 0;
    lua_Integer pageSize = static_cast<lua_Integer>(options.pageSize);
    lua_Integer maxPages = static_cast<lua_Integer>(options.maxPages);
    lua::Table::readField(L, 2, "count", count);
    lua::Table::readField(L, 2, "pageSize", pageSize);
    lua::Table::readField(L, 2, "maxPages", maxPages);
    if (count < 0) {
        return luaL_error(L, "The option \"count\" must be at least 0.");
    }
    if (pageSize < 1 || maxPages < 1) {
        return luaL_error(L, "The option \"%s\" must be at least 1.", pageSize < 1 ? "pageSize" : "maxPages");
    }
    options = {.count = static_cast<std::size_t>(count), .pageSize = static_cast<std::size_t>(pageSize), .maxPages = static_cast<std::size_t>(maxPages)};
    if (lua_getfield(L, 2, "load") != LUA_TFUNCTION) {
        return luaL_error(L, "The option \"load\" must be a function.");
    }
    const int load = lua_gettop(L);

    pushValue(L, *gui, collection, "load");
    const bool sameLoader = lua_rawequal(L, -1, load) != 0;
    lua_pop(L, 1);
    if (const auto pages = std::dynamic_pointer_cast<LuaPageSource>(collection.getSource()); pages && sameLoader) {
        pages->configure(options);
        return 0;
    }
    auto created = std::make_shared<LuaPageSource>(L, *gui, collection);
    setValue(L, *gui, collection, "load", load);
    lua_newtable(L);
    setValue(L, *gui, collection, "pages", -1);
    setValue(L, *gui, collection, "items", 0);
    created->configure(options);
    collection.setSource(created);
    return 0;
}

int CollectionLua::insert(lua_State* L) {
    Collection& collection = check(L);
    LuaItemSource& list = requireList(L, collection);
    const std::size_t index = readIndex(L, 2, collection, collection.getCount() + 1);
    luaL_checktype(L, 3, LUA_TTABLE);
    list.insert(L, index, 3);
    return 0;
}

int CollectionLua::remove(lua_State* L) {
    Collection& collection = check(L);
    LuaItemSource& list = requireList(L, collection);
    const std::size_t index = readIndex(L, 2, collection, collection.getCount());
    const lua_Integer count = luaL_optinteger(L, 3, 1);
    luaL_argcheck(L, count >= 1, 3, "the count must be at least 1");
    list.remove(L, index, static_cast<std::size_t>(count));
    return 0;
}

int CollectionLua::move(lua_State* L) {
    Collection& collection = check(L);
    LuaItemSource& list = requireList(L, collection);
    const std::size_t from = readIndex(L, 2, collection, collection.getCount());
    const std::size_t to = readIndex(L, 3, collection, collection.getCount());
    if (from != to) {
        list.move(L, from, to);
    }
    return 0;
}

int CollectionLua::replace(lua_State* L) {
    Collection& collection = check(L);
    LuaItemSource& list = requireList(L, collection);
    const std::size_t index = readIndex(L, 2, collection, collection.getCount());
    list.replace(L, index, 3);
    return 0;
}

// The target is an id, an index or a list of them.
int CollectionLua::reload(lua_State* L) {
    Collection& collection = check(L);
    LuaItemSource& list = requireList(L, collection);
    if (lua_type(L, 2) != LUA_TTABLE) {
        list.reload(L, readTarget(L, 2, collection));
        return 0;
    }
    const auto length = static_cast<lua_Integer>(lua_rawlen(L, 2));
    for (lua_Integer position = 1; position <= length; ++position) {
        lua_rawgeti(L, 2, position);
        list.reload(L, readTarget(L, -1, collection));
        lua_pop(L, 1);
    }
    return 0;
}

// The binder of a type runs with the cell, the item table and the index of every item the type binds, and `nil` removes it.
int CollectionLua::setBinder(lua_State* L) {
    const std::shared_ptr<Gui> gui = checkGui(L);
    Collection& collection = check(L);
    const std::string type = lua::Stack::read<std::string>(L, 2);
    if (lua_isnoneornil(L, 3)) {
        collection.setBinder(type, {});
        pushValue(L, *gui, collection, "binders");
        lua_pushnil(L);
        lua_setfield(L, -2, type.c_str());
        return 0;
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_State* main = lua::Runtime::getMainThread(L);
    const Gui* owner = gui.get();
    const Collection* shown = &collection;
    // clang-format off
    collection.setBinder(type, [main, owner, shown, type](CollectionCell& cell, std::size_t index) {
        callBinder(main, *owner, *shown, type, cell, index);
    });
    // clang-format on
    pushValue(L, *gui, collection, "binders");
    lua_pushvalue(L, 3);
    lua_setfield(L, -2, type.c_str());
    return 0;
}

int CollectionLua::scrollTo(lua_State* L) {
    Collection& collection = check(L);
    const std::size_t index = readTarget(L, 2, collection);
    Collection::ScrollRequest request;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kScrollFields});
        if (lua_getfield(L, 3, "align") != LUA_TNIL) {
            const auto found = lua_type(L, -1) == LUA_TSTRING ? std::ranges::find(Collection::kScrollAligns, lua::Stack::read<std::string_view>(L, -1), &std::pair<std::string_view, Collection::ScrollAlign>::first) : Collection::kScrollAligns.end();
            if (found == Collection::kScrollAligns.end()) {
                return luaL_error(L, "The option \"align\" must be \"nearest\", \"start\", \"center\" or \"end\".");
            }
            request.align = found->second;
        }
        lua_pop(L, 1);
        lua::Table::readField(L, 3, "offset", request.offset);
        lua::Table::readField(L, 3, "animated", request.animated);
    }
    collection.scrollTo(index, request);
    return 0;
}

int CollectionLua::scrollBy(lua_State* L) {
    Collection& collection = check(L);
    const double distance = luaL_checknumber(L, 2);
    bool animated = true;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kAnimationFields});
        lua::Table::readField(L, 3, "animated", animated);
    }
    collection.scrollBy(distance, animated);
    return 0;
}

int CollectionLua::focus(lua_State* L) {
    Collection& collection = check(L);
    collection.focusItem(readTarget(L, 2, collection));
    return 0;
}

int CollectionLua::indexOf(lua_State* L) {
    const Collection& collection = check(L);
    const std::optional<std::size_t> index = collection.findItem(lua::Stack::read<std::string_view>(L, 2));
    if (index) {
        lua_pushinteger(L, static_cast<lua_Integer>(*index + 1));
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int CollectionLua::cellOf(lua_State* L) {
    const Collection& collection = check(L);
    if (CollectionCell* cell = collection.findCell(readTarget(L, 2, collection))) {
        pushCell(L, *cell);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int CollectionLua::visibleRange(lua_State* L) {
    const std::optional<std::pair<std::size_t, std::size_t>> range = check(L).getVisibleRange();
    if (!range) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(range->first + 1));
    lua_pushinteger(L, static_cast<lua_Integer>(range->second + 1));
    return 2;
}

int CollectionLua::saveState(lua_State* L) {
    const Collection::State state = check(L).saveState();
    lua_createtable(L, 0, 3);
    if (!state.item.empty()) {
        lua::Stack::push(L, state.item);
        lua_setfield(L, -2, "item");
    }
    lua_pushnumber(L, state.distance);
    lua_setfield(L, -2, "distance");
    if (!state.focused.empty()) {
        lua::Stack::push(L, state.focused);
        lua_setfield(L, -2, "focused");
    }
    return 1;
}

int CollectionLua::restoreState(lua_State* L) {
    Collection& collection = check(L);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kStateFields});
    Collection::State state;
    lua::Table::readField(L, 2, "item", state.item);
    lua::Table::readField(L, 2, "distance", state.distance);
    lua::Table::readField(L, 2, "focused", state.focused);
    collection.restoreState(state);
    return 0;
}

int CollectionLua::count(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(check(L).getCount()));
    return 1;
}

int CollectionLua::scrollOffset(lua_State* L) {
    lua_pushnumber(L, check(L).getScrollOffset());
    return 1;
}

int CollectionLua::setScrollOffset(lua_State* L) {
    check(L).setScrollOffset(luaL_checknumber(L, 3));
    return 0;
}

int CollectionLua::contentLength(lua_State* L) {
    lua_pushnumber(L, check(L).getContentLength());
    return 1;
}

int CollectionLua::viewportLength(lua_State* L) {
    lua_pushnumber(L, check(L).getViewportLength());
    return 1;
}

int CollectionLua::focusedItem(lua_State* L) {
    const Collection& collection = check(L);
    const std::optional<std::size_t> index = collection.getFocusedIndex();
    if (!index || *index >= collection.getCount()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, collection.getSource()->getId(*index));
    return 1;
}

int CollectionLua::selected(lua_State* L) {
    const Collection& collection = check(L);
    const std::vector<std::size_t> indices = collection.getSelected();
    lua_createtable(L, static_cast<int>(indices.size()), 0);
    lua_Integer position = 0;
    for (const std::size_t index : indices) {
        lua::Stack::push(L, collection.getSource()->getId(index));
        lua_rawseti(L, -2, ++position);
    }
    return 1;
}

int CollectionLua::cellSet(lua_State* L) {
    CollectionCell& cell = checkCell(L);
    luaL_checktype(L, 3, LUA_TTABLE);
    cell.set(lua::Stack::read<std::string_view>(L, 2), lua::JsonConverter::read(L, 3));
    return 0;
}

int CollectionLua::cellTransform(lua_State* L) {
    CollectionCell& cell = checkCell(L);
    TransformLua::push(L, cell.getTransform(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int CollectionLua::cellItem(lua_State* L) {
    lua::Stack::push(L, checkCell(L).getItem());
    return 1;
}

int CollectionLua::cellIndex(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(checkCell(L).getIndex() + 1));
    return 1;
}

int CollectionLua::cellType(lua_State* L) {
    lua::Stack::push(L, checkCell(L).getType());
    return 1;
}

int CollectionLua::cellBound(lua_State* L) {
    const CellHandle& handle = lua::Userdata::check<CellHandle>(L, 1);
    const std::shared_ptr<CollectionCell> cell = handle.cell.lock();
    lua::Stack::push(L, cell != nullptr && cell->getGeneration() == handle.generation);
    return 1;
}

void CollectionLua::install(lua_State* L) {
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kValuesKey);
    lua::ClassBuilder<Handle>(L).function("setItems", &lua::Binding::native<&setItems>).function("setPages", &lua::Binding::native<&setPages>).function("insert", &lua::Binding::native<&insert>).function("remove", &lua::Binding::native<&remove>).function("move", &lua::Binding::native<&move>).function("replace", &lua::Binding::native<&replace>).function("reload", &lua::Binding::native<&reload>).function("setBinder", &lua::Binding::native<&setBinder>).function("scrollTo", &lua::Binding::native<&scrollTo>).function("scrollBy", &lua::Binding::native<&scrollBy>).function("focus", &lua::Binding::native<&focus>).function("indexOf", &lua::Binding::native<&indexOf>).function("cellOf", &lua::Binding::native<&cellOf>).function("visibleRange", &lua::Binding::native<&visibleRange>).function("saveState", &lua::Binding::native<&saveState>).function("restoreState", &lua::Binding::native<&restoreState>).property("count", &lua::Binding::native<&count>).property("scrollOffset", &lua::Binding::native<&scrollOffset>, &lua::Binding::native<&setScrollOffset>).property("contentLength", &lua::Binding::native<&contentLength>).property("viewportLength", &lua::Binding::native<&viewportLength>).property("focusedItem", &lua::Binding::native<&focusedItem>).property("selected", &lua::Binding::native<&selected>).install();
    lua::ClassBuilder<CellHandle>(L).function("set", &lua::Binding::native<&cellSet>).function("transform", &lua::Binding::native<&cellTransform>).property("item", &lua::Binding::native<&cellItem>).property("index", &lua::Binding::native<&cellIndex>).property("type", &lua::Binding::native<&cellType>).property("bound", &cellBound).install();
}

} // namespace haylen::ui
