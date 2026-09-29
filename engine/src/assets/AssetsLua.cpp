#include "assets/AssetsLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "graphics/TextureResource.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/plugins/AssetsPlugin.hpp"
#include "varn/async/Promise.h"

namespace haylen::assets {

Manager& AssetsLua::getAssets(lua_State* L) {
    return lua::Runtime::getEngine(L).getAssets();
}

core::Json AssetsLua::readOptions(lua_State* L, int index) {
    return lua_isnoneornil(L, index) ? core::Json::object() : lua::JsonConverter::read(L, index);
}

std::string AssetsLua::readType(lua_State* L, int typeIndex, std::string_view path) {
    return lua_isnoneornil(L, typeIndex) ? getAssets(L).getTypeForPath(path) : lua::Stack::read<std::string>(L, typeIndex);
}

int AssetsLua::texture(lua_State* L) {
    lua::Stack::push(L, graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(getAssets(L).load("texture", lua::Stack::read<std::string_view>(L, 1), readOptions(L, 2)))));
    return 1;
}

// Loads a TrueType or OpenType font, or a BMFont from a .fnt file, by the extension of the path.
int AssetsLua::font(lua_State* L) {
    const std::string_view path = lua::Stack::read<std::string_view>(L, 1);
    const std::string type = getAssets(L).getTypeForPath(path);
    luaL_argcheck(L, type == "font" || type == "bitmapFont", 1, "expected a .ttf, .otf or .fnt file");
    lua::Stack::push(L, std::static_pointer_cast<text::Font>(getAssets(L).load(type, path, readOptions(L, 2))));
    return 1;
}

int AssetsLua::shader(lua_State* L) {
    lua::Stack::push(L, getAssets(L).shader(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::json(lua_State* L) {
    lua::JsonConverter::push(L, getAssets(L).json(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::text(lua_State* L) {
    lua::Stack::push(L, getAssets(L).text(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// Returns the raw file as a Lua string, byte for byte.
int AssetsLua::bytes(lua_State* L) {
    const std::vector<std::uint8_t> data = getAssets(L).bytes(lua::Stack::read<std::string_view>(L, 1));
    lua_pushlstring(L, reinterpret_cast<const char*>(data.data()), data.size());
    return 1;
}

int AssetsLua::exists(lua_State* L) {
    lua::Stack::push(L, getAssets(L).exists(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::typeForPath(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getTypeForPath(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::hasType(lua_State* L) {
    lua::Stack::push(L, getAssets(L).hasType(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::list(lua_State* L) {
    lua::Stack::push(L, getAssets(L).list(lua_isnoneornil(L, 1) ? std::string_view{} : lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// Loads any registered asset type synchronously with load(path, type?, options?).
int AssetsLua::load(lua_State* L) {
    const std::string path = lua::Stack::read<std::string>(L, 1);
    const std::string type = readType(L, 2, path);
    const std::shared_ptr<void> asset = getAssets(L).load(type, path, readOptions(L, 3));
    lua::Runtime::getEngine(L).getPlugin<plugins::AssetsPlugin>().pushAsset(L, type, asset);
    return 1;
}

// Returns a promise that resolves with the asset, for use with :await() inside async.run or async.spawn.
int AssetsLua::loadAsync(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string path = lua::Stack::read<std::string>(L, 1);
    const std::string type = readType(L, 2, path);
    auto promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());

    // clang-format off
    owner.getAssets().loadAsync(type, path, [promise, type, plugin = &owner.getPlugin<plugins::AssetsPlugin>()](std::shared_ptr<void> asset, std::string error) {
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

int AssetsLua::defineGroups(lua_State* L) {
    Manager& manager = getAssets(L);
    manager.defineGroups(lua_type(L, 1) == LUA_TSTRING ? manager.json(lua::Stack::read<std::string_view>(L, 1)) : lua::JsonConverter::read(L, 1));
    return 0;
}

int AssetsLua::defineGroup(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    const core::Json entries = lua::JsonConverter::read(L, 2);
    getAssets(L).defineGroups(core::Json{{"groups", {{name, entries}}}});
    return 0;
}

// Starts loading a group and returns a promise that resolves with the list of failures. The optional function receives progress from 0 to 1.
int AssetsLua::preload(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const std::string name = lua::Stack::read<std::string>(L, 1);
    auto promise = std::make_shared<varn::async::Promise>(owner.getScriptRuntime());

    Manager::GroupProgress onProgress;
    if (lua_isfunction(L, 2)) {
        auto function = std::make_shared<lua::Reference>(L, 2);
        // clang-format off
        onProgress = [function](float fraction) {
            lua_State* main = function->getState();
            lua::Runtime::runReporting(main, [&] {
                function->push(main);
                lua_pushnumber(main, fraction);
                lua::Runtime::protectedCall(main, 1, 0);
            });
        };
        // clang-format on
    }

    // clang-format off
    owner.getAssets().preload(name, std::move(onProgress), [promise](std::vector<std::string> errors) {
        promise->resolveCustom([errors = std::move(errors)](lua_State* state) { lua::Stack::push(state, errors); });
    });
    // clang-format on

    varn::async::Promise::push(L, promise);
    return 1;
}

int AssetsLua::unloadGroup(lua_State* L) {
    getAssets(L).unloadGroup(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int AssetsLua::groupProgress(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getGroupProgress(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::groupLoaded(lua_State* L) {
    lua::Stack::push(L, getAssets(L).isGroupLoaded(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int AssetsLua::groups(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getGroups());
    return 1;
}

int AssetsLua::cachedCount(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getCachedCount());
    return 1;
}

int AssetsLua::pendingCount(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getPendingCount());
    return 1;
}

int AssetsLua::releaseUnused(lua_State* L) {
    lua::Stack::push(L, getAssets(L).releaseUnused());
    return 1;
}

int AssetsLua::setUploadBudget(lua_State* L) {
    getAssets(L).setUploadBudget(lua::Stack::read<double>(L, 1));
    return 0;
}

int AssetsLua::uploadBudget(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getUploadBudget());
    return 1;
}

int AssetsLua::uploadCount(lua_State* L) {
    lua::Stack::push(L, getAssets(L).getUploadCount());
    return 1;
}

int AssetsLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"texture", &lua::Binding::native<&texture>}, {"font", &lua::Binding::native<&font>}, {"shader", &lua::Binding::native<&shader>}, {"json", &lua::Binding::native<&json>}, {"text", &lua::Binding::native<&text>}, {"bytes", &lua::Binding::native<&bytes>}, {"exists", &lua::Binding::native<&exists>}, {"list", &lua::Binding::native<&list>}, {"typeForPath", &lua::Binding::native<&typeForPath>}, {"hasType", &hasType}, {"load", &lua::Binding::native<&load>}, {"loadAsync", &lua::Binding::native<&loadAsync>}, {"defineGroups", &lua::Binding::native<&defineGroups>}, {"defineGroup", &lua::Binding::native<&defineGroup>}, {"preload", &lua::Binding::native<&preload>}, {"unloadGroup", &lua::Binding::native<&unloadGroup>}, {"groupProgress", &lua::Binding::native<&groupProgress>}, {"groupLoaded", &lua::Binding::native<&groupLoaded>}, {"groups", &groups}, {"cachedCount", &cachedCount}, {"pendingCount", &pendingCount}, {"releaseUnused", &releaseUnused}, {"setUploadBudget", &lua::Binding::native<&setUploadBudget>}, {"uploadBudget", &uploadBudget}, {"uploadCount", &uploadCount}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void AssetsLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.assets", &open);
}

} // namespace haylen::assets
