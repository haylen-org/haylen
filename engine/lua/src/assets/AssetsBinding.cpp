#include "assets/AssetsBinding.hpp"

#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/modules/AssetsModule.hpp"
#include "runtime/Owners.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua::assets {

haylen::assets::Manager& AssetsBinding::getAssets(lua_State* L) {
    return lua::Runtime::getEngine(L).getAssets();
}

haylen::core::Json AssetsBinding::readOptions(lua_State* L, int index) {
    if (lua_isnoneornil(L, index)) {
        return haylen::core::Json::object();
    }

    luaL_checktype(L, index, LUA_TTABLE);
    return lua::JsonConverter::read(L, index);
}

std::string AssetsBinding::readType(lua_State* L, int typeIndex, std::string_view path) {
    return lua_isnoneornil(L, typeIndex) ? getAssets(L).getTypeForPath(path) : lua::Stack::read<std::string>(L, typeIndex);
}

int AssetsBinding::texture(lua_State* L) {
    lua::Stack::push(L, haylen::graphics::Texture(std::static_pointer_cast<haylen::graphics::TextureResource>(getAssets(L).load("texture", lua::Stack::read<std::string_view>(L, 1), readOptions(L, 2)))));
    return 1;
}

// Loads a TrueType or OpenType font, or a BMFont from a `.fnt` file, by the extension of the path.
int AssetsBinding::font(lua_State* L) {
    const std::string_view path = lua::Stack::read<std::string_view>(L, 1);
    const std::string type = getAssets(L).getTypeForPath(path);
    luaL_argcheck(L, type == "font" || type == "bitmapFont", 1, "expected a .ttf, .otf, .ttc or .fnt file");
    lua::Stack::push(L, std::static_pointer_cast<haylen::text::Font>(getAssets(L).load(type, path, readOptions(L, 2))));
    return 1;
}

int AssetsBinding::shader(lua_State* L) {
    lua::Stack::push(L, getAssets(L).shader(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::vectorImage(lua_State* L) {
    lua::Stack::push(L, getAssets(L).vectorImage(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::json(lua_State* L) {
    lua::JsonConverter::push(L, getAssets(L).json(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::text(lua_State* L) {
    lua::Stack::push(L, getAssets(L).text(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::bytes(lua_State* L) {
    const std::string_view path = lua::Stack::read<std::string_view>(L, 1);
    const std::vector<std::uint8_t> data = lua_isnoneornil(L, 2) ? getAssets(L).bytes(path) : getAssets(L).bytes(path, lua::Stack::read<std::uint64_t>(L, 2), lua::Stack::read<std::size_t>(L, 3));
    lua_pushlstring(L, reinterpret_cast<const char*>(data.data()), data.size());
    return 1;
}

// Reads a file, or a range of it, off the frame thread and returns a promise of its bytes as a string, which rejects with the reason of a file that is missing or does not come down.
int AssetsBinding::bytesAsync(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const std::string path = lua::Stack::read<std::string>(L, 1);
    const lua::Promise promise(engine);
    // clang-format off
    auto settle = [promise](std::vector<std::uint8_t> data, std::string error) {
        haylen::core::JobSystem::Result<std::vector<std::uint8_t>> result{.error = std::move(error)};
        if (result.error.empty()) {
            result.value = std::move(data);
        }

        promise.settle(std::move(result), &pushBytes);
    };
    // clang-format on
    if (lua_isnoneornil(L, 2)) {
        engine.getAssets().bytesAsync(path, std::move(settle));
    } else {
        engine.getAssets().bytesAsync(path, lua::Stack::read<std::uint64_t>(L, 2), lua::Stack::read<std::size_t>(L, 3), std::move(settle));
    }

    promise.push(L);
    return 1;
}

void AssetsBinding::pushBytes(lua_State* L, const std::vector<std::uint8_t>& data) {
    lua_pushlstring(L, reinterpret_cast<const char*>(data.data()), data.size());
}

int AssetsBinding::fileSize(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getFileSize(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::exists(lua_State* L) {
    lua::Stack::push(L, getAssets(L).exists(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::typeForPath(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getTypeForPath(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::hasType(lua_State* L) {
    lua::Stack::push(L, getAssets(L).hasType(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::list(lua_State* L) {
    lua::Stack::push(L, getAssets(L).list(lua_isnoneornil(L, 1) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::load(lua_State* L) {
    const std::string path = lua::Stack::read<std::string>(L, 1);
    const std::string type = readType(L, 2, path);
    const std::shared_ptr<void> asset = getAssets(L).load(type, path, readOptions(L, 3));
    lua::Runtime::getEngine(L).getPlugin<lua::ScriptPlugin>().getModule<lua::modules::AssetsModule>().pushAsset(L, type, asset);
    return 1;
}

// Returns a promise that resolves with the asset, for use with `:await()` inside `async.run` or `async.spawn`.
int AssetsBinding::loadAsync(lua_State* L) {
    haylen::core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string path = lua::Stack::read<std::string>(L, 1);
    const std::string type = readType(L, 2, path);
    auto promise = std::make_shared<varn::async::Promise>(owner.getPlugin<lua::ScriptPlugin>().getRuntime());

    // clang-format off
    owner.getAssets().loadAsync(type, path, [promise, type, plugin = &owner.getPlugin<lua::ScriptPlugin>().getModule<lua::modules::AssetsModule>()](std::shared_ptr<void> asset, std::string error) {
        if (!asset) {
            promise->reject(error);
            return;
        }

        promise->resolveCustom([plugin, type, asset](lua_State* state) { plugin->pushAsset(state, type, asset); });
    }, readOptions(L, 3));
    // clang-format on

    varn::async::Promise::push(L, promise);
    return 1;
}

int AssetsBinding::defineGroups(lua_State* L) {
    haylen::assets::Manager& manager = getAssets(L);
    manager.defineGroups(lua_type(L, 1) == LUA_TSTRING ? manager.json(lua::Stack::read<std::string_view>(L, 1)) : lua::JsonConverter::read(L, 1));
    return 0;
}

int AssetsBinding::defineGroup(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    const haylen::core::Json entries = lua::JsonConverter::read(L, 2);
    getAssets(L).defineGroups(haylen::core::Json{{"groups", {{name, entries}}}});
    return 0;
}

// Starts loading a group and returns a promise that resolves with the list of failures. The optional function receives progress from 0 to 1 while its owner lives, and without an `owner` it takes the default owner, which in development is the module that preloads at its top level.
int AssetsBinding::preload(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const std::string name = lua::Stack::read<std::string>(L, 1);

    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
    }

    int owner = 0;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kPreloadOptions});
        if (lua_getfield(L, 3, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    haylen::assets::Manager::GroupProgress onProgress;
    if (lua_isfunction(L, 2)) {
        if (owner == 0) {
            owner = lua::Owners::pushDefault(L);
        }

        auto function = std::make_shared<lua::Owners::Function>(L, 2, owner);
        lua_State* main = lua::Runtime::getMainThread(L);
        // clang-format off
        onProgress = [function, main](float fraction) {
            lua::Runtime::runReporting(main, [&] {
                if (function->push(main)) {
                    lua_pushnumber(main, fraction);
                    lua::Runtime::protectedCall(main, 1, 0);
                }
            });
        };
        // clang-format on
    }

    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getAssets().preload(name, std::move(onProgress), [promise](std::vector<std::string> errors) {
        promise->resolveCustom([errors = std::move(errors)](lua_State* state) { lua::Stack::push(state, errors); });
    });
    // clang-format on

    varn::async::Promise::push(L, promise);
    return 1;
}

int AssetsBinding::unloadGroup(lua_State* L) {
    getAssets(L).unloadGroup(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int AssetsBinding::groupProgress(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getGroupProgress(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::groupLoaded(lua_State* L) {
    lua::Stack::push(L, getAssets(L).isGroupLoaded(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsBinding::groups(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getGroups());
    return 1;
}

int AssetsBinding::cachedCount(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getCachedCount());
    return 1;
}

int AssetsBinding::pendingCount(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getPendingCount());
    return 1;
}

int AssetsBinding::releaseUnused(lua_State* L) {
    lua::Stack::push(L, getAssets(L).releaseUnused());
    return 1;
}

int AssetsBinding::setUploadBudget(lua_State* L) {
    getAssets(L).setUploadBudget(lua::Stack::read<double>(L, 1));
    return 0;
}

int AssetsBinding::uploadBudget(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getUploadBudget());
    return 1;
}

int AssetsBinding::uploadCount(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getUploadCount());
    return 1;
}

int AssetsBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"texture", &lua::Binding::native<&texture>}, {"font", &lua::Binding::native<&font>}, {"shader", &lua::Binding::native<&shader>}, {"vectorImage", &lua::Binding::native<&vectorImage>}, {"json", &lua::Binding::native<&json>}, {"text", &lua::Binding::native<&text>}, {"bytes", &lua::Binding::native<&bytes>}, {"bytesAsync", &lua::Binding::native<&bytesAsync>}, {"fileSize", &lua::Binding::native<&fileSize>}, {"exists", &lua::Binding::native<&exists>}, {"list", &lua::Binding::native<&list>}, {"typeForPath", &lua::Binding::native<&typeForPath>}, {"hasType", &hasType}, {"load", &lua::Binding::native<&load>}, {"loadAsync", &lua::Binding::native<&loadAsync>}, {"defineGroups", &lua::Binding::native<&defineGroups>}, {"defineGroup", &lua::Binding::native<&defineGroup>}, {"preload", &lua::Binding::native<&preload>}, {"unloadGroup", &lua::Binding::native<&unloadGroup>}, {"groupProgress", &lua::Binding::native<&groupProgress>}, {"groupLoaded", &lua::Binding::native<&groupLoaded>}, {"groups", &lua::Binding::native<&groups>}, {"cachedCount", &cachedCount}, {"pendingCount", &pendingCount}, {"releaseUnused", &releaseUnused}, {"setUploadBudget", &lua::Binding::native<&setUploadBudget>}, {"uploadBudget", &uploadBudget}, {"uploadCount", &uploadCount}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void AssetsBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.assets", &open);
}

} // namespace haylen::lua::assets
