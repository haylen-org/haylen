#include "storage/StorageLua.hpp"

#include <lua.hpp>

#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
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

int StorageLua::readText(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).readText(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int StorageLua::writeText(lua_State* L) {
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

// Returns the absolute path of the storage folder, where Varn's `fs` module reads and writes asynchronously.
int StorageLua::root(lua_State* L) {
    lua::Stack::push(L, getUserStorage(L).getRoot().generic_string());
    return 1;
}

// Saves a slot with `writeSlot(slot, data[, summary])`, where the summary is a small table shown by load menus, and makes it durable at once.
int StorageLua::writeSlot(lua_State* L) {
    const core::Json summary = lua_isnoneornil(L, 3) ? core::Json::object() : lua::JsonConverter::read(L, 3);
    getSaveSlots(L).write(lua::Stack::read<std::string_view>(L, 1), lua::JsonConverter::read(L, 2), summary);
    getUserStorage(L).flush();
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
    const bool removed = getSaveSlots(L).remove(lua::Stack::read<std::string_view>(L, 1));
    if (removed) {
        getUserStorage(L).flush();
    }
    lua::Stack::push(L, removed);
    return 1;
}

int StorageLua::listSlots(lua_State* L) {
    pushInfos(getSaveSlots(L).list())(L);
    return 1;
}

// Lua cannot hold every JSON value, so the operation checks it on the I/O pool before the frame thread pushes it.
StorageLua::Pusher StorageLua::pushJson(core::Json value) {
    lua::JsonConverter::validate(value);
    return [value = std::move(value)](lua_State* state) { lua::JsonConverter::push(state, value); };
}

StorageLua::Pusher StorageLua::pushInfos(std::vector<SaveSlots::Info> infos) {
    for (const SaveSlots::Info& info : infos) {
        lua::JsonConverter::validate(info.summary);
    }
    // clang-format off
    return [infos = std::move(infos)](lua_State* state) {
        lua_createtable(state, static_cast<int>(infos.size()), 0);
        for (std::size_t index = 0; index < infos.size(); ++index) {
            pushInfo(state, infos[index]);
            lua_rawseti(state, -2, static_cast<lua_Integer>(index + 1));
        }
    };
    // clang-format on
}

int StorageLua::queue(lua_State* L, bool durable, std::function<Pusher()> work) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    core::JobSystem& jobs = engine.getJobs();
    UserStorage& storage = engine.getStorage();
    // clang-format off
    engine.getPlugin<plugins::StoragePlugin>().queueOperation([promise, &jobs, &storage, durable, work = std::move(work)] {
        try {
            Pusher pushResult = work();

            // Flushing belongs to the frame thread, so the promise settles there once the change is durable.
            jobs.postToFrame([promise, &storage, durable, pushResult = std::move(pushResult)] {
                if (durable) {
                    storage.flush();
                }
                promise.resolveWith(pushResult);
            });
        } catch (const std::exception& error) {
            promise.reject(error.what());
        }
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int StorageLua::readTextAsync(lua_State* L) {
    UserStorage& storage = getUserStorage(L);
    // clang-format off
    return queue(L, false, [&storage, path = lua::Stack::read<std::string>(L, 1)] {
        return Pusher([text = storage.readText(path)](lua_State* state) { lua_pushlstring(state, text.data(), text.size()); });
    });
    // clang-format on
}

int StorageLua::writeTextAsync(lua_State* L) {
    UserStorage& storage = getUserStorage(L);
    // clang-format off
    return queue(L, false, [&storage, path = lua::Stack::read<std::string>(L, 1), text = lua::Stack::read<std::string>(L, 2)] {
        storage.writeText(path, text);
        return Pusher([](lua_State* state) { lua_pushboolean(state, 1); });
    });
    // clang-format on
}

int StorageLua::readJsonAsync(lua_State* L) {
    UserStorage& storage = getUserStorage(L);
    return queue(L, false, [&storage, path = lua::Stack::read<std::string>(L, 1)] { return pushJson(core::Json::parse(storage.readText(path))); });
}

// The value becomes JSON on the frame thread, which owns the Lua state, and its text reaches the disk on the I/O pool.
int StorageLua::writeJsonAsync(lua_State* L) {
    UserStorage& storage = getUserStorage(L);
    // clang-format off
    return queue(L, false, [&storage, path = lua::Stack::read<std::string>(L, 1), value = lua::JsonConverter::read(L, 2)] {
        storage.writeText(path, value.dump(2));
        return Pusher([](lua_State* state) { lua_pushboolean(state, 1); });
    });
    // clang-format on
}

int StorageLua::removeAsync(lua_State* L) {
    UserStorage& storage = getUserStorage(L);
    // clang-format off
    return queue(L, false, [&storage, path = lua::Stack::read<std::string>(L, 1)] {
        return Pusher([removed = storage.remove(path)](lua_State* state) { lua_pushboolean(state, removed ? 1 : 0); });
    });
    // clang-format on
}

int StorageLua::listAsync(lua_State* L) {
    UserStorage& storage = getUserStorage(L);
    std::string directory = lua_isnoneornil(L, 1) ? std::string() : lua::Stack::read<std::string>(L, 1);
    // clang-format off
    return queue(L, false, [&storage, directory = std::move(directory)] {
        return Pusher([files = storage.list(directory)](lua_State* state) { lua::Stack::push(state, files); });
    });
    // clang-format on
}

int StorageLua::writeSlotAsync(lua_State* L) {
    SaveSlots& slots = getSaveSlots(L);
    core::Json summary = lua_isnoneornil(L, 3) ? core::Json::object() : lua::JsonConverter::read(L, 3);
    // clang-format off
    return queue(L, true, [&slots, slot = lua::Stack::read<std::string>(L, 1), data = lua::JsonConverter::read(L, 2), summary = std::move(summary)] {
        slots.write(slot, data, summary);
        return Pusher([](lua_State* state) { lua_pushboolean(state, 1); });
    });
    // clang-format on
}

int StorageLua::readSlotAsync(lua_State* L) {
    SaveSlots& slots = getSaveSlots(L);
    // clang-format off
    return queue(L, false, [&slots, slot = lua::Stack::read<std::string>(L, 1)] {
        std::optional<core::Json> data = slots.read(slot);
        return data ? pushJson(std::move(*data)) : Pusher([](lua_State* state) { lua_pushnil(state); });
    });
    // clang-format on
}

int StorageLua::slotInfoAsync(lua_State* L) {
    SaveSlots& slots = getSaveSlots(L);
    // clang-format off
    return queue(L, false, [&slots, slot = lua::Stack::read<std::string>(L, 1)] {
        std::optional<SaveSlots::Info> info = slots.getInfo(slot);
        if (!info) {
            return Pusher([](lua_State* state) { lua_pushnil(state); });
        }
        lua::JsonConverter::validate(info->summary);
        return Pusher([info = std::move(*info)](lua_State* state) { pushInfo(state, info); });
    });
    // clang-format on
}

int StorageLua::removeSlotAsync(lua_State* L) {
    SaveSlots& slots = getSaveSlots(L);
    // clang-format off
    return queue(L, true, [&slots, slot = lua::Stack::read<std::string>(L, 1)] {
        return Pusher([removed = slots.remove(slot)](lua_State* state) { lua_pushboolean(state, removed ? 1 : 0); });
    });
    // clang-format on
}

int StorageLua::listSlotsAsync(lua_State* L) {
    SaveSlots& slots = getSaveSlots(L);
    return queue(L, false, [&slots] { return pushInfos(slots.list()); });
}

int StorageLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"readText", &lua::Binding::native<&readText>}, {"writeText", &lua::Binding::native<&writeText>}, {"readJson", &lua::Binding::native<&readJson>}, {"writeJson", &lua::Binding::native<&writeJson>}, {"exists", &lua::Binding::native<&exists>}, {"remove", &lua::Binding::native<&remove>}, {"list", &lua::Binding::native<&list>}, {"flush", &lua::Binding::native<&flush>}, {"root", &lua::Binding::native<&root>}, {"writeSlot", &lua::Binding::native<&writeSlot>}, {"readSlot", &lua::Binding::native<&readSlot>}, {"slotInfo", &lua::Binding::native<&slotInfo>}, {"slotExists", &lua::Binding::native<&slotExists>}, {"removeSlot", &lua::Binding::native<&removeSlot>}, {"listSlots", &lua::Binding::native<&listSlots>}, {"readTextAsync", &lua::Binding::native<&readTextAsync>}, {"writeTextAsync", &lua::Binding::native<&writeTextAsync>}, {"readJsonAsync", &lua::Binding::native<&readJsonAsync>}, {"writeJsonAsync", &lua::Binding::native<&writeJsonAsync>}, {"removeAsync", &lua::Binding::native<&removeAsync>}, {"listAsync", &lua::Binding::native<&listAsync>}, {"writeSlotAsync", &lua::Binding::native<&writeSlotAsync>}, {"readSlotAsync", &lua::Binding::native<&readSlotAsync>}, {"slotInfoAsync", &lua::Binding::native<&slotInfoAsync>}, {"removeSlotAsync", &lua::Binding::native<&removeSlotAsync>}, {"listSlotsAsync", &lua::Binding::native<&listSlotsAsync>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void StorageLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.storage", &open);
}

} // namespace haylen::storage
