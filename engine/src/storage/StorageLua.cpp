#include "storage/StorageLua.hpp"

#include <lua.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/storage/UserStorage.hpp"

namespace haylen::storage {

UserStorage& StorageLua::getUserStorage(lua_State* L) {
    return lua::Runtime::getEngine(L).getStorage();
}

SaveSlots& StorageLua::getSaveSlots(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::StoragePlugin>().getSaveSlots();
}

void StorageLua::pushInfo(lua_State* L, const SaveSlots::Info& info) {
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, info.slot);
    lua_setfield(L, -2, "slot");
    lua::Stack::push(L, info.savedAt);
    lua_setfield(L, -2, "savedAt");
    lua::JsonConverter::push(L, info.summary);
    lua_setfield(L, -2, "summary");
}

int StorageLua::read(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).readText(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::write(lua_State* L) {
    getUserStorage(L).writeText(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int StorageLua::readJson(lua_State* L) {
    lua::JsonConverter::push(L, core::Json::parse(getUserStorage(L).readText(lua::Stack::read<std::string_view>(L, 1))));
    return 1;
}

int StorageLua::writeJson(lua_State* L) {
    getUserStorage(L).writeText(lua::Stack::read<std::string_view>(L, 1), lua::JsonConverter::read(L, 2).dump(2));
    return 0;
}

int StorageLua::exists(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).exists(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::remove(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).remove(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::list(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).list(lua_isnoneornil(L, 1) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::flush(lua_State* L) {
    getUserStorage(L).flush();
    return 0;
}

// Returns the absolute path of the storage folder, where Varn's fs module reads and writes asynchronously.
int StorageLua::root(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).getRoot().generic_string());
    return 1;
}

// Saves a slot with writeSlot(slot, data[, summary]), where the summary is a small table shown by load menus.
int StorageLua::writeSlot(lua_State* L) {
    const core::Json summary = lua_isnoneornil(L, 3) ? core::Json::object() : lua::JsonConverter::read(L, 3);
    getSaveSlots(L).write(lua::Stack::read<std::string_view>(L, 1), lua::JsonConverter::read(L, 2), summary);
    return 0;
}

int StorageLua::readSlot(lua_State* L) {
    const std::optional<core::Json> data = getSaveSlots(L).read(lua::Stack::read<std::string_view>(L, 1));
    if (!data) {
        lua_pushnil(L);
        return 1;
    }
    lua::JsonConverter::push(L, *data);
    return 1;
}

int StorageLua::slotInfo(lua_State* L) {
    const std::optional<SaveSlots::Info> info = getSaveSlots(L).getInfo(lua::Stack::read<std::string_view>(L, 1));
    if (!info) {
        lua_pushnil(L);
        return 1;
    }
    pushInfo(L, *info);
    return 1;
}

int StorageLua::slotExists(lua_State* L) {
    lua::Stack::push(L, getSaveSlots(L).exists(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::removeSlot(lua_State* L) {
    lua::Stack::push(L, getSaveSlots(L).remove(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::listSlots(lua_State* L) {
    const std::vector<SaveSlots::Info> saves = getSaveSlots(L).list();
    lua_createtable(L, static_cast<int>(saves.size()), 0);
    for (std::size_t index = 0; index < saves.size(); ++index) {
        pushInfo(L, saves[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int StorageLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"read", &lua::Binding::native<&read>}, {"write", &lua::Binding::native<&write>}, {"readJson", &lua::Binding::native<&readJson>}, {"writeJson", &lua::Binding::native<&writeJson>}, {"exists", &lua::Binding::native<&exists>}, {"remove", &lua::Binding::native<&remove>}, {"list", &lua::Binding::native<&list>}, {"flush", &lua::Binding::native<&flush>}, {"root", &lua::Binding::native<&root>}, {"writeSlot", &lua::Binding::native<&writeSlot>}, {"readSlot", &lua::Binding::native<&readSlot>}, {"slotInfo", &lua::Binding::native<&slotInfo>}, {"slotExists", &lua::Binding::native<&slotExists>}, {"removeSlot", &lua::Binding::native<&removeSlot>}, {"listSlots", &lua::Binding::native<&listSlots>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void StorageLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.storage", &open);
}

} // namespace haylen::storage
