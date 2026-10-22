#include "storage/StorageLua.hpp"

#include <lua.hpp>

#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/storage/ResumeState.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "storage/StoredJson.hpp"

namespace haylen::storage {

UserStorage& StorageLua::getUserStorage(lua_State* L) {
    return lua::Runtime::getEngine(L).getStorage();
}

SaveSlots& StorageLua::getSaveSlots(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::StoragePlugin>().getSaveSlots();
}

core::Json StorageLua::readStorable(lua_State* L, int index) {
    core::Json value = lua::JsonConverter::read(L, index);
    StoredJson::validate(value);
    return value;
}

void StorageLua::pushInfo(lua_State* L, const SaveSlots::Info& info) {
    lua_createtable(L, 0, 6);
    lua_pushinteger(L, info.version);
    lua_setfield(L, -2, "version");
    lua_pushinteger(L, static_cast<lua_Integer>(info.revision));
    lua_setfield(L, -2, "revision");
    lua::Stack::push(L, info.recovered);
    lua_setfield(L, -2, "recovered");
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
    const std::string_view path = lua::Stack::read<std::string_view>(L, 1);
    getUserStorage(L).writeText(path, readStorable(L, 2).dump(2));
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

void StorageLua::migrate(lua_State* L, const std::string& slot, int fromVersion) {
    const int target = getSaveSlots(L).getVersion();
    if (fromVersion > target) {
        luaL_error(L, "The save slot \"%s\" has version %d, which is newer than the version %d this app reads.", slot.c_str(), fromVersion, target);
    }
    lua_getfield(L, LUA_REGISTRYINDEX, kMigrations);
    const int migrations = lua_gettop(L);
    for (int current = fromVersion; current < target; ++current) {
        if (!lua_istable(L, migrations) || lua_rawgeti(L, migrations, current) != LUA_TFUNCTION) {
            luaL_error(L, "The save slot \"%s\" has version %d and no migration reaches version %d.", slot.c_str(), fromVersion, target);
        }
        lua_pushvalue(L, migrations - 1);
        lua_call(L, 1, 1);
        lua_replace(L, migrations - 1);
    }
    lua_settop(L, migrations - 1);
}

void StorageLua::publishWrite(lua_State* L, const std::string& slot) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const std::optional<SaveSlots::Info> info = getSaveSlots(L).getInfo(slot);
    engine.getEvents().emit(core::LifecycleEvent::kSaveSlotWritten, {{"slot", slot}, {"revision", info ? info->revision : 0}, {"version", info ? info->version : 0}});
}

int StorageLua::setSaveVersion(lua_State* L) {
    getSaveSlots(L).setVersion(static_cast<int>(luaL_checkinteger(L, 1)));
    return 0;
}

int StorageLua::saveVersion(lua_State* L) {
    lua_pushinteger(L, getSaveSlots(L).getVersion());
    return 1;
}

// Registers a migration with `addMigration(fromVersion, function(data) return data end)`, which turns the data of a version into the data of the next one when a slot of that version is read.
int StorageLua::addMigration(lua_State* L) {
    const lua_Integer from = luaL_checkinteger(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    if (from < 1) {
        return luaL_error(L, "A save migration needs a version from 1 and a function.");
    }
    if (lua_getfield(L, LUA_REGISTRYINDEX, kMigrations) != LUA_TTABLE) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, kMigrations);
    }
    lua_pushvalue(L, 2);
    lua_rawseti(L, -2, from);
    return 0;
}

int StorageLua::exportSlot(lua_State* L) {
    const std::optional<std::string> bytes = getSaveSlots(L).exportSlot(lua::Stack::read<std::string_view>(L, 1));
    if (!bytes) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, bytes->data(), bytes->size());
    return 1;
}

// Takes the bytes of `exportSlot` with `importSlot(slot, bytes[, {policy = 'keepNewer'}])` and returns whether they replaced the slot.
int StorageLua::importSlot(lua_State* L) {
    SaveSlots::ImportPolicy policy = SaveSlots::ImportPolicy::KeepNewer;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kImportOptions});
        std::string name;
        lua::Table::readField(L, 3, "policy", name);
        if (!name.empty()) {
            const std::optional<SaveSlots::ImportPolicy> found = SaveSlots::importPolicyFromName(name);
            if (!found) {
                return luaL_error(L, "The policy of an imported save slot must be \"replace\" or \"keepNewer\".");
            }
            policy = *found;
        }
    }
    const bool imported = getSaveSlots(L).importSlot(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<std::string_view>(L, 2), policy);
    if (imported) {
        getUserStorage(L).flush();
    }
    lua::Stack::push(L, imported);
    return 1;
}

// Saves a slot with `writeSlot(slot, data[, summary])`, where the summary is a small table shown by load menus, and makes it durable at once.
int StorageLua::writeSlot(lua_State* L) {
    const core::Json summary = lua_isnoneornil(L, 3) ? core::Json::object() : lua::JsonConverter::read(L, 3);
    const std::string slot = lua::Stack::read<std::string>(L, 1);
    getSaveSlots(L).write(slot, lua::JsonConverter::read(L, 2), summary);
    getUserStorage(L).flush();
    publishWrite(L, slot);
    return 0;
}

int StorageLua::readSlot(lua_State* L) {
    const std::string slot = lua::Stack::read<std::string>(L, 1);
    std::optional<SaveSlots::Stored> stored = getSaveSlots(L).readStored(slot);
    if (!stored) {
        lua_pushnil(L);
        return 1;
    }
    lua::JsonConverter::push(L, stored->data);
    migrate(L, slot, stored->info.version);
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

// The value goes on the stack in a protected call, so a failure there, such as a migration that raises, rejects the promise instead of escaping into the coroutine that awaits it.
void StorageLua::settle(lua_State* L, const lua::Promise& promise, const Pusher& pushResult) {
    std::shared_ptr<lua::Reference> value;
    try {
        // clang-format off
        lua::Runtime::protectedRun(L, [&pushResult, &value](lua_State* state) {
            pushResult(state);
            value = std::make_shared<lua::Reference>(state, -1);
        });
        // clang-format on
    } catch (const std::exception& error) {
        promise.reject(error.what());
        return;
    }
    promise.resolveWith([value](lua_State* state) { value->push(state); });
}

int StorageLua::queue(lua_State* L, bool durable, std::function<Pusher()> work) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    lua::Promise promise(engine);
    core::JobSystem& jobs = engine.getJobs();
    UserStorage& storage = engine.getStorage();
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    engine.getPlugin<plugins::StoragePlugin>().queueOperation([promise, main, &jobs, &storage, durable, work = std::move(work)]() mutable {
        try {
            Pusher pushResult = work();

            // Flushing belongs to the frame thread, so the promise settles there once the change is durable. The promise moves there, so the value it keeps is released on the frame thread.
            jobs.postToFrame([promise = std::move(promise), main, &storage, durable, pushResult = std::move(pushResult)] {
                if (durable) {
                    storage.flush();
                }
                settle(main, promise, pushResult);
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
    return queue(L, false, [&storage, path = lua::Stack::read<std::string>(L, 1), value = readStorable(L, 2)] {
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
    core::Json summary = lua_isnoneornil(L, 3) ? core::Json::object() : readStorable(L, 3);
    // clang-format off
    return queue(L, true, [&slots, slot = lua::Stack::read<std::string>(L, 1), data = readStorable(L, 2), summary = std::move(summary)] {
        slots.write(slot, data, summary);
        return Pusher([slot](lua_State* state) {
            publishWrite(state, slot);
            lua_pushboolean(state, 1);
        });
    });
    // clang-format on
}

int StorageLua::readSlotAsync(lua_State* L) {
    SaveSlots& slots = getSaveSlots(L);
    // clang-format off
    return queue(L, false, [&slots, slot = lua::Stack::read<std::string>(L, 1)] {
        std::optional<SaveSlots::Stored> stored = slots.readStored(slot);
        if (!stored) {
            return Pusher([](lua_State* state) { lua_pushnil(state); });
        }
        lua::JsonConverter::validate(stored->data);
        return Pusher([slot, data = std::move(stored->data), version = stored->info.version](lua_State* state) {
            lua::JsonConverter::push(state, data);
            migrate(state, slot, version);
        });
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

// Keeps a value that only a launch after the system ended the app in the background reads, or nothing for `nil`.
int StorageLua::setResumeState(lua_State* L) {
    luaL_checkany(L, 1);
    ResumeState& state = lua::Runtime::getEngine(L).getPlugin<plugins::StoragePlugin>().getResumeState();
    state.set(lua_isnil(L, 1) ? core::Json() : readStorable(L, 1));
    return 0;
}

// Returns the value the run before kept and how many seconds ago the app went to the background with it, or nothing when that run ended normally or never reached the background.
int StorageLua::resumeState(lua_State* L) {
    const std::optional<ResumeState::Restored>& restored = lua::Runtime::getEngine(L).getPlugin<plugins::StoragePlugin>().getResumeState().getRestored();
    if (!restored) {
        return 0;
    }
    lua::JsonConverter::push(L, restored->value);
    lua_pushnumber(L, restored->age);
    return 2;
}

int StorageLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"readText", &lua::Binding::native<&readText>}, {"writeText", &lua::Binding::native<&writeText>}, {"readJson", &lua::Binding::native<&readJson>}, {"writeJson", &lua::Binding::native<&writeJson>}, {"exists", &lua::Binding::native<&exists>}, {"remove", &lua::Binding::native<&remove>}, {"list", &lua::Binding::native<&list>}, {"flush", &lua::Binding::native<&flush>}, {"root", &lua::Binding::native<&root>}, {"setSaveVersion", &lua::Binding::native<&setSaveVersion>}, {"saveVersion", &saveVersion}, {"addMigration", &lua::Binding::native<&addMigration>}, {"exportSlot", &lua::Binding::native<&exportSlot>}, {"importSlot", &lua::Binding::native<&importSlot>}, {"writeSlot", &lua::Binding::native<&writeSlot>}, {"readSlot", &lua::Binding::native<&readSlot>}, {"slotInfo", &lua::Binding::native<&slotInfo>}, {"slotExists", &lua::Binding::native<&slotExists>}, {"removeSlot", &lua::Binding::native<&removeSlot>}, {"listSlots", &lua::Binding::native<&listSlots>}, {"readTextAsync", &lua::Binding::native<&readTextAsync>}, {"writeTextAsync", &lua::Binding::native<&writeTextAsync>}, {"readJsonAsync", &lua::Binding::native<&readJsonAsync>}, {"writeJsonAsync", &lua::Binding::native<&writeJsonAsync>}, {"removeAsync", &lua::Binding::native<&removeAsync>}, {"listAsync", &lua::Binding::native<&listAsync>}, {"writeSlotAsync", &lua::Binding::native<&writeSlotAsync>}, {"readSlotAsync", &lua::Binding::native<&readSlotAsync>}, {"slotInfoAsync", &lua::Binding::native<&slotInfoAsync>}, {"removeSlotAsync", &lua::Binding::native<&removeSlotAsync>}, {"listSlotsAsync", &lua::Binding::native<&listSlotsAsync>}, {"setResumeState", &lua::Binding::native<&setResumeState>}, {"resumeState", &lua::Binding::native<&resumeState>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void StorageLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.storage", &open);
}

} // namespace haylen::storage
