#include "core/TimerBinding.hpp"

#include <lua.hpp>

#include <memory>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/TimerScheduler.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "runtime/Owners.hpp"
#include "runtime/ScriptedScene.hpp"

namespace haylen::lua::core {

// Shared by `after` and `every`: `timer.after(seconds, fn, options)` and `timer.every(seconds, fn, options)`. A timer with an owner holds its function through the owner and ends with it, and one that inherits its process mode follows the mode of its owner.
int TimerBinding::start(lua_State* L, bool repeating) {
    const auto seconds = lua::Stack::read<float>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    haylen::core::TimerScheduler::Options options;
    int count = -1;
    int owner = 0;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        const std::span<const std::string_view> fields(kTimerFields);
        lua::Table::checkFields(L, 3, {repeating ? fields : fields.first(3)});
        lua::Table::readField(L, 3, "processMode", options.processMode);
        lua::Table::readField(L, 3, "unscaled", options.unscaled);
        lua::Table::readField(L, 3, "count", count);

        if (lua_getfield(L, 3, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }

    if (owner != 0 && options.processMode == haylen::core::ProcessMode::Inherit) {
        options.parentMode = lua::ScriptedScene::followOwnerMode(L, owner);
    }

    auto function = std::make_shared<lua::Owners::Function>(L, 2, owner);
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    auto callback = [function, main] {
        if (function->push(main)) {
            lua::Runtime::protectedCall(main, 0, 0);
        }
    };
    // clang-format on

    haylen::core::TimerScheduler& timers = lua::Runtime::getEngine(L).getTimers();
    const haylen::core::TimerScheduler::Id id = repeating ? timers.every(seconds, std::move(callback), count, options) : timers.after(seconds, std::move(callback), options);

    if (owner != 0) {
        lua::Owners::add(L, owner, timers.getConnection(id));
    }

    lua::Stack::push(L, id);
    return 1;
}

int TimerBinding::after(lua_State* L) {
    return start(L, false);
}

int TimerBinding::every(lua_State* L) {
    return start(L, true);
}

int TimerBinding::cancel(lua_State* L) {
    lua::Runtime::getEngine(L).getTimers().cancel(lua::Stack::read<haylen::core::TimerScheduler::Id>(L, 1));
    return 0;
}

int TimerBinding::pause(lua_State* L) {
    lua::Runtime::getEngine(L).getTimers().pause(lua::Stack::read<haylen::core::TimerScheduler::Id>(L, 1), lua_isnoneornil(L, 2) || lua_toboolean(L, 2) != 0);
    return 0;
}

int TimerBinding::active(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getTimers().isActive(lua::Stack::read<haylen::core::TimerScheduler::Id>(L, 1)));
    return 1;
}

int TimerBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"after", &lua::Binding::native<&after>}, {"every", &lua::Binding::native<&every>}, {"cancel", &cancel}, {"pause", &pause}, {"active", &active}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void TimerBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.timer", &open);
}

} // namespace haylen::lua::core
