#include "ui/LuaPageSource.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/ui/Collection.hpp"
#include "lua/StackScope.hpp"
#include "lua/Task.hpp"
#include "ui/CollectionLua.hpp"
#include "ui/components/collections/CollectionDiff.hpp"

namespace haylen::ui {

LuaPageSource::LuaPageSource(lua_State* L, const Gui& owner, Collection& shown) : state(lua::Runtime::getMainThread(L)), gui(owner), collection(shown) {}

// Pages keep their place while the page size stays, and a page whose length the new count changes loads again. The pages go before the count changes, so the collection hears of them while it still has their items.
void LuaPageSource::configure(const Options& value) {
    std::vector<std::size_t> dropped;
    for (const auto& [page, kept] : pages) {
        const std::size_t first = page * value.pageSize;
        const std::size_t length = first >= value.count ? 0 : std::min(value.pageSize, value.count - first);
        if (value.pageSize != options.pageSize || length != getPageLength(page)) {
            dropped.push_back(page);
        }
    }
    for (const std::size_t page : dropped) {
        drop(page);
    }
    const std::size_t before = options.count;
    options = value;
    std::vector<std::size_t> previous(options.count);
    for (std::size_t index = 0; index < previous.size(); ++index) {
        previous[index] = index < before ? index : Change::kNew;
    }
    changed.emit({.kind = Change::Kind::Replaced, .index = 0, .count = options.count, .target = 0, .previous = previous});
}

std::size_t LuaPageSource::getPageLength(std::size_t page) const noexcept {
    const std::size_t first = page * options.pageSize;
    return first >= options.count ? 0 : std::min(options.pageSize, options.count - first);
}

const LuaPageSource::Page* LuaPageSource::findLoaded(std::size_t index) const {
    const auto found = pages.find(index / options.pageSize);
    return found != pages.end() && found->second.loaded ? &found->second : nullptr;
}

std::string_view LuaPageSource::getId(std::size_t index) const {
    const Page* page = findLoaded(index);
    return page != nullptr ? std::string_view(page->ids[index % options.pageSize]) : std::string_view();
}

std::string_view LuaPageSource::getType(std::size_t index) const {
    const Page* page = findLoaded(index);
    return page != nullptr ? std::string_view(page->types[index % options.pageSize]) : std::string_view();
}

// Pages of the range that are neither loaded nor loading load now, and the pages farthest from the range go once more pages than `maxPages` are kept.
void LuaPageSource::request(std::size_t first, std::size_t count) {
    if (count == 0 || first >= options.count) {
        return;
    }
    const std::size_t firstPage = first / options.pageSize;
    const std::size_t lastPage = (std::min(first + count, options.count) - 1) / options.pageSize;
    for (std::size_t page = firstPage; page <= lastPage; ++page) {
        if (!pages.contains(page)) {
            load(page);
        }
    }
    dropFarthest(firstPage, lastPage);
}

// The loader returns the list of the page, which stores at once, or a promise of it, which a task awaits before it stores the list.
void LuaPageSource::load(std::size_t page) {
    const std::uint64_t ticket = nextTicket++;
    pages[page] = Page{.ids = {}, .types = {}, .ticket = ticket, .loaded = false};
    lua_State* L = state;
    const lua::StackScope scope(L);
    CollectionLua::pushValue(L, gui, collection, "load");
    lua_pushinteger(L, static_cast<lua_Integer>(page * options.pageSize + 1));
    lua_pushinteger(L, static_cast<lua_Integer>(getPageLength(page)));
    lua::Runtime::protectedCall(L, 2, 1);
    if (lua_istable(L, -1)) {
        store(L, page, ticket, -1);
        return;
    }
    if (!lua::Promise::isPromise(L, -1)) {
        throw std::invalid_argument("The page loader of the collection \"" + collection.getId() + "\" must return a list of items or a promise of one.");
    }
    const int promise = lua_gettop(L);
    new (lua_newuserdatauv(L, sizeof(Request), 0)) Request{.source = weak_from_this(), .page = page, .ticket = ticket};
    if (lua::Userdata::newMetatable(L, kRequestType)) {
        lua_pushcfunction(L, &collectRequest);
        lua_setfield(L, -2, "__gc");
    }
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, &awaitPage, 1);
    lua_pushvalue(L, promise);
    lua_State* main = state;
    // clang-format off
    lua::Task::start(L, lua_gettop(L) - 1, 1, 0, [main](const std::optional<lua::Error>& error) {
        if (error) {
            lua::Runtime::reportError(main, *error);
        }
    });
    // clang-format on
}

int LuaPageSource::awaitPage(lua_State* L) {
    lua_getfield(L, 1, "await");
    lua_pushvalue(L, 1);
    lua_callk(L, 1, 2, 0, &finishPage);
    return finishPage(L, LUA_OK, 0);
}

// The promise answers with the list, or with nothing and the reason it failed, which stops the app with that reason.
int LuaPageSource::finishPage(lua_State* L, int, lua_KContext) {
    // clang-format off
    return lua::Binding::guarded(L, [L] {
        const Request& request = *static_cast<const Request*>(lua_touserdata(L, lua_upvalueindex(1)));
        const std::shared_ptr<LuaPageSource> source = request.source.lock();
        if (!source) {
            return 0;
        }
        if (!lua_isnil(L, -1)) {
            throw std::runtime_error("The page loader of the collection \"" + source->collection.getId() + "\" failed. " + luaL_tolstring(L, -1, nullptr));
        }
        source->store(L, request.page, request.ticket, -2);
        return 0;
    });
    // clang-format on
}

int LuaPageSource::collectRequest(lua_State* L) {
    static_cast<Request*>(lua_touserdata(L, 1))->~Request();
    lua_pushnil(L);
    lua_setmetatable(L, 1);
    return 0;
}

void LuaPageSource::store(lua_State* L, std::size_t page, std::uint64_t ticket, int list) {
    const auto found = pages.find(page);
    if (found == pages.end() || found->second.ticket != ticket || found->second.loaded) {
        return;
    }
    const int table = lua_absindex(L, list);
    const std::size_t length = getPageLength(page);
    if (!lua_istable(L, table)) {
        throw std::invalid_argument("The page loader of the collection \"" + collection.getId() + "\" must return a list of items or a promise of one.");
    }
    if (lua_rawlen(L, table) != length) {
        throw std::invalid_argument("The page loader of the collection \"" + collection.getId() + "\" returned " + std::to_string(lua_rawlen(L, table)) + " items for a page of " + std::to_string(length) + ".");
    }

    Page loaded{.ids = std::vector<std::string>(length), .types = std::vector<std::string>(length), .ticket = ticket, .loaded = true};
    const std::size_t first = page * options.pageSize;
    for (std::size_t offset = 0; offset < length; ++offset) {
        lua_rawgeti(L, table, static_cast<lua_Integer>(offset + 1));
        std::tie(loaded.ids[offset], loaded.types[offset]) = CollectionLua::readItem(L, -1, first + offset + 1, collection);
        lua_pop(L, 1);
    }
    std::vector<std::string_view> ids;
    for (const auto& [index, kept] : pages) {
        ids.insert(ids.end(), kept.ids.begin(), kept.ids.end());
    }
    ids.insert(ids.end(), loaded.ids.begin(), loaded.ids.end());
    CollectionDiff::checkUnique(ids, collection.getId());

    CollectionLua::setPage(L, gui, collection, page, table);
    found->second = std::move(loaded);
    changed.emit({.kind = Change::Kind::Changed, .index = first, .count = length});
    dropFarthest(page, page);
}

void LuaPageSource::drop(std::size_t page) {
    const auto found = pages.find(page);
    const bool loaded = found->second.loaded;
    pages.erase(found);
    CollectionLua::setPage(state, gui, collection, page, 0);
    if (loaded) {
        changed.emit({.kind = Change::Kind::Changed, .index = page * options.pageSize, .count = getPageLength(page)});
    }
}

// The pages of the range a view needs stay, and the others go from the farthest one while more than `maxPages` are kept.
void LuaPageSource::dropFarthest(std::size_t firstPage, std::size_t lastPage) {
    const auto distance = [&](std::size_t page) { return page < firstPage ? firstPage - page : page > lastPage ? page - lastPage : 0; };
    while (pages.size() > options.maxPages) {
        const auto farthest = std::ranges::max_element(pages, {}, [&](const auto& entry) { return distance(entry.first); });
        if (distance(farthest->first) == 0) {
            return;
        }
        drop(farthest->first);
    }
}

void LuaPageSource::pushItem(lua_State* L, std::size_t index) const {
    if (findLoaded(index) == nullptr) {
        lua_pushnil(L);
        return;
    }
    CollectionLua::pushPage(L, gui, collection, index / options.pageSize);
    lua_rawgeti(L, -1, static_cast<lua_Integer>(index % options.pageSize + 1));
    lua_remove(L, -2);
}

core::Json LuaPageSource::getValue(std::size_t index, std::string_view field) const {
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

void LuaPageSource::setValue(std::size_t index, std::string_view field, const core::Json& value) {
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
