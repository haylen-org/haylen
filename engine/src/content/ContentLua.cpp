#include "content/ContentLua.hpp"

#include <lua.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "content/LuaPackSource.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Owners.hpp"

namespace haylen::content {

ContentManager& ContentLua::getManager(lua_State* L) {
    return lua::Runtime::getEngine(L).getContent();
}

void ContentLua::pushError(lua_State* L, const Error& failure) {
    lua_createtable(L, 0, 8);
    lua::Stack::push(L, Error::getCodeName(failure.getCode()));
    lua_setfield(L, -2, "code");
    lua::Stack::push(L, std::string(failure.what()));
    lua_setfield(L, -2, "message");
    lua_pushboolean(L, failure.isRetryable() ? 1 : 0);
    lua_setfield(L, -2, "retryable");
    const Error::Details& details = failure.getDetails();
    if (details.httpStatus) {
        lua_pushinteger(L, *details.httpStatus);
        lua_setfield(L, -2, "httpStatus");
    }
    if (details.requiredBytes) {
        setInteger(L, "requiredBytes", *details.requiredBytes);
    }
    if (details.availableBytes) {
        setInteger(L, "availableBytes", *details.availableBytes);
    }
    for (const auto& [field, value] : {std::pair{"path", &details.path}, {"manifestId", &details.manifestId}, {"packId", &details.packId}, {"contentId", &details.contentId}, {"storedId", &details.storedId}, {"keyId", &details.keyId}, {"platform", &details.platform}, {"platformMessage", &details.platformMessage}}) {
        if (!value->empty()) {
            lua::Stack::push(L, *value);
            lua_setfield(L, -2, field);
        }
    }
}

// The app reads the failure of a content promise like the one of `content.error`, as the third value of its await.
void ContentLua::reject(lua_State* L, const lua::Promise& promise, const Error& failure) {
    pushError(L, failure);
    lua::Stack::push(L, std::string(failure.what()));
    lua_setfield(L, -2, "error");
    promise.rejectHandledFailure(L);
}

void ContentLua::setInteger(lua_State* L, const char* field, std::uint64_t value) {
    lua_pushinteger(L, static_cast<lua_Integer>(value));
    lua_setfield(L, -2, field);
}

void ContentLua::pushProgress(lua_State* L, const ContentManager::Progress& value) {
    lua_createtable(L, 0, 2);
    setInteger(L, "receivedBytes", value.receivedBytes);
    setInteger(L, "totalBytes", value.totalBytes);
}

ContentManager::ProgressCallback ContentLua::readProgress(lua_State* L, int index) {
    if (!lua_isnoneornil(L, index)) {
        luaL_checktype(L, index, LUA_TFUNCTION);
    }
    int owner = 0;
    if (!lua_isnoneornil(L, index + 1)) {
        luaL_checktype(L, index + 1, LUA_TTABLE);
        lua::Table::checkFields(L, index + 1, {kOptions});
        if (lua_getfield(L, index + 1, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (!lua_isfunction(L, index)) {
        return {};
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    auto function = std::make_shared<lua::Owners::Function>(L, index, owner);
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    return [function, main](const ContentManager::Progress& value) {
        lua::Runtime::runReporting(main, [&] {
            if (function->push(main)) {
                pushProgress(main, value);
                lua::Runtime::protectedCall(main, 1, 0);
            }
        });
    };
    // clang-format on
}

int ContentLua::isProtected(lua_State* L) {
    lua_pushboolean(L, getManager(L).isProtected() ? 1 : 0);
    return 1;
}

int ContentLua::generation(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(getManager(L).getGeneration()));
    return 1;
}

int ContentLua::channel(lua_State* L) {
    const std::string name = getManager(L).getChannel();
    if (name.empty()) {
        lua_pushnil(L);
    } else {
        lua::Stack::push(L, name);
    }
    return 1;
}

int ContentLua::status(lua_State* L) {
    lua::Stack::push(L, ContentManager::getStatusName(getManager(L).getStatus()));
    return 1;
}

int ContentLua::progress(lua_State* L) {
    pushProgress(L, getManager(L).getProgress());
    return 1;
}

int ContentLua::error(lua_State* L) {
    const std::optional<Error>& failure = getManager(L).getError();
    if (failure) {
        pushError(L, *failure);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

int ContentLua::update(lua_State* L) {
    const std::optional<ContentManager::Update> found = getManager(L).getUpdate();
    if (!found) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 3);
    setInteger(L, "generation", found->generation);
    setInteger(L, "downloadBytes", found->downloadBytes);
    setInteger(L, "reusedBytes", found->reusedBytes);
    return 1;
}

int ContentLua::isLocal(lua_State* L) {
    lua_pushboolean(L, getManager(L).isLocal(lua::Stack::read<std::string_view>(L, 1)) ? 1 : 0);
    return 1;
}

int ContentLua::isGroupLocal(lua_State* L) {
    lua_pushboolean(L, getManager(L).isGroupLocal(lua::Stack::read<std::string_view>(L, 1)) ? 1 : 0);
    return 1;
}

int ContentLua::addPackFolder(lua_State* L) {
    getManager(L).addPackFolder(std::filesystem::path(lua::Stack::read<std::string_view>(L, 1)));
    return 0;
}

int ContentLua::addPackSource(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kOptions});
        if (lua_getfield(L, 2, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    core::Connection connection = getManager(L).addPackSource(std::make_shared<LuaPackSource>(L, 1, owner));
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, connection);
    return 1;
}

int ContentLua::prefetch(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    std::vector<std::string> paths;
    if (lua_type(L, 1) == LUA_TSTRING) {
        paths = engine.getAssets().getGroupPaths(lua::Stack::read<std::string_view>(L, 1));
    } else {
        luaL_checktype(L, 1, LUA_TTABLE);
        paths = lua::Stack::read<std::vector<std::string>>(L, 1);
    }
    ContentManager::ProgressCallback onProgress = readProgress(L, 2);

    const lua::Promise promise(engine);
    // clang-format off
    engine.getContent().prefetch(paths, std::move(onProgress), [&engine, promise](std::optional<Error> failure) {
        if (failure) {
            reject(engine.getLuaState(), promise, *failure);
            return;
        }
        promise.resolve(true);
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int ContentLua::check(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    // clang-format off
    engine.getContent().check([&engine, promise](std::optional<ContentManager::Update> found, std::optional<Error> failure) {
        if (failure) {
            reject(engine.getLuaState(), promise, *failure);
            return;
        }
        promise.resolve(found ? core::Json{{"generation", found->generation}, {"downloadBytes", found->downloadBytes}} : core::Json());
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int ContentLua::download(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    ContentManager::ProgressCallback onProgress = readProgress(L, 1);
    const lua::Promise promise(engine);
    // clang-format off
    engine.getContent().download(std::move(onProgress), [&engine, promise](std::optional<Error> failure) {
        if (failure) {
            reject(engine.getLuaState(), promise, *failure);
            return;
        }
        promise.resolve(true);
    });
    // clang-format on
    promise.push(L);
    return 1;
}

int ContentLua::apply(lua_State* L) {
    getManager(L).apply();
    return 0;
}

int ContentLua::cancel(lua_State* L) {
    getManager(L).cancel();
    return 0;
}

int ContentLua::stats(lua_State* L) {
    const ContentManager::Statistics statistics = getManager(L).getStatistics();
    lua_createtable(L, 0, 21);
    setInteger(L, "generation", statistics.generation);
    setInteger(L, "catalogFiles", statistics.catalogFiles);
    setInteger(L, "storedRecords", statistics.storedRecords);
    setInteger(L, "storedBytes", statistics.storedBytes);
    setInteger(L, "openShards", statistics.openShards);
    setInteger(L, "decodedChunks", statistics.decodedChunks);
    setInteger(L, "decodedBytes", statistics.decodedBytes);
    lua_pushnumber(L, statistics.decodeMilliseconds);
    lua_setfield(L, -2, "decodeMilliseconds");
    setInteger(L, "activeReads", statistics.activeReads);
    setInteger(L, "downloadedBytes", statistics.downloadedBytes);
    setInteger(L, "requests", statistics.requests);
    setInteger(L, "resumedRecords", statistics.resumedRecords);
    setInteger(L, "pendingDownloads", statistics.pendingDownloads);
    setInteger(L, "wantedRecords", statistics.wantedRecords);
    setInteger(L, "activeTransfers", statistics.activeTransfers);
    lua_pushnumber(L, statistics.downloadRate);
    lua_setfield(L, -2, "downloadRate");
    setInteger(L, "reusedBytes", statistics.reusedBytes);
    setInteger(L, "installedShards", statistics.installedShards);
    setInteger(L, "remoteShards", statistics.remoteShards);
    setInteger(L, "missingChunks", statistics.missingChunks);
    setInteger(L, "pendingPrefetch", statistics.pendingPrefetch);
    return 1;
}

int ContentLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"isProtected", &lua::Binding::native<&isProtected>}, {"generation", &lua::Binding::native<&generation>}, {"channel", &lua::Binding::native<&channel>}, {"status", &lua::Binding::native<&status>}, {"progress", &lua::Binding::native<&progress>}, {"error", &lua::Binding::native<&error>}, {"update", &lua::Binding::native<&update>}, {"isLocal", &lua::Binding::native<&isLocal>}, {"isGroupLocal", &lua::Binding::native<&isGroupLocal>}, {"addPackFolder", &lua::Binding::native<&addPackFolder>}, {"addPackSource", &lua::Binding::native<&addPackSource>}, {"prefetch", &lua::Binding::native<&prefetch>}, {"check", &lua::Binding::native<&check>}, {"download", &lua::Binding::native<&download>}, {"apply", &lua::Binding::native<&apply>}, {"cancel", &lua::Binding::native<&cancel>}, {"stats", &lua::Binding::native<&stats>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void ContentLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.content", &open);
}

} // namespace haylen::content
