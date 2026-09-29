#include "core/CoreLua.hpp"

#include <lua.hpp>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/ClassLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/Version.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/SafeAreaSimulation.hpp"
#include "lua/Autoloads.hpp"
#include "plugins/CorePlugin.hpp"

namespace haylen::lua {

template <> struct EnumNames<core::Log::Level> {
    static constexpr std::array<std::pair<std::string_view, core::Log::Level>, 4> kLevels{{{"debug", core::Log::Level::Debug}, {"info", core::Log::Level::Info}, {"warning", core::Log::Level::Warning}, {"error", core::Log::Level::Error}}};

    static std::optional<core::Log::Level> fromName(std::string_view name) {
        for (const auto& [candidate, level] : kLevels) {
            if (candidate == name) {
                return level;
            }
        }
        return std::nullopt;
    }

    static std::string_view name(core::Log::Level value) {
        for (const auto& [candidate, level] : kLevels) {
            if (level == value) {
                return candidate;
            }
        }
        return kLevels.front().first;
    }
};

} // namespace haylen::lua

namespace haylen::core {

std::string CoreLua::joinArguments(lua_State* L) {
    std::string line;
    const int count = lua_gettop(L);
    for (int index = 1; index <= count; ++index) {
        if (index > 1) {
            line.push_back('\t');
        }
        std::size_t length = 0;
        const char* text = luaL_tolstring(L, index, &length);
        line.append(text, length);
        lua_pop(L, 1);
    }
    return line;
}

template <Log::Level Level> int CoreLua::logMessage(lua_State* L) {
    Log::write(Level, joinArguments(L));
    return 0;
}

int CoreLua::openLog(lua_State* L) {
    const luaL_Reg functions[] = {
        {"debug", &logMessage<Log::Level::Debug>}, {"info", &logMessage<Log::Level::Info>}, {"warning", &logMessage<Log::Level::Warning>}, {"error", &logMessage<Log::Level::Error>}, {"level", &lua::Binding::function<&Log::getLevel>}, {"setLevel", &lua::Binding::function<&Log::setLevel>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

int CoreLua::rootQuit(lua_State* L) {
    lua::Runtime::getEngine(L).quit();
    return 0;
}

int CoreLua::rootTime(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getElapsed());
    return 1;
}

int CoreLua::rootDelta(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getDelta());
    return 1;
}

int CoreLua::rootUnscaledDelta(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getUnscaledDelta());
    return 1;
}

int CoreLua::rootFrame(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getFrameIndex());
    return 1;
}

int CoreLua::rootTimeScale(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getTimeScale());
    return 1;
}

int CoreLua::rootSetTimeScale(lua_State* L) {
    lua::Runtime::getEngine(L).getClock().setTimeScale(lua::Stack::read<double>(L, 1));
    return 0;
}

int CoreLua::rootFixedStep(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getFixedStep());
    return 1;
}

int CoreLua::rootInterpolation(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getClock().getInterpolation());
    return 1;
}

// Stops the app with the message and the stack of the Lua code that reported it.
int CoreLua::rootReportError(lua_State* L) {
    lua::Runtime::getEngine(L).reportError(lua::Runtime::captureError(L, lua::Stack::read<std::string>(L, 1), 1));
    return 0;
}

int CoreLua::rootPaused(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isPaused());
    return 1;
}

int CoreLua::rootSetPaused(lua_State* L) {
    lua::Runtime::getEngine(L).setPaused(lua::Stack::read<bool>(L, 1));
    return 0;
}

int CoreLua::rootAppState(lua_State* L) {
    switch (lua::Runtime::getEngine(L).getAppState()) {
    case Engine::AppState::Active:
        lua_pushliteral(L, "active");
        break;
    case Engine::AppState::Inactive:
        lua_pushliteral(L, "inactive");
        break;
    case Engine::AppState::Background:
        lua_pushliteral(L, "background");
        break;
    }
    return 1;
}

int CoreLua::rootNetworkState(lua_State* L) {
    switch (lua::Runtime::getEngine(L).getNetworkState()) {
    case Engine::NetworkState::Unknown:
        lua_pushliteral(L, "unknown");
        break;
    case Engine::NetworkState::Online:
        lua_pushliteral(L, "online");
        break;
    case Engine::NetworkState::Offline:
        lua_pushliteral(L, "offline");
        break;
    }
    return 1;
}

int CoreLua::rootHalted(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).isHalted());
    return 1;
}

int CoreLua::rootLifecycle(lua_State* L) {
    const AppConfig::Lifecycle& lifecycle = lua::Runtime::getEngine(L).getLifecycle();
    lua_createtable(L, 0, 3);
    lua::Stack::push(L, lifecycle.pauseOnBackground);
    lua_setfield(L, -2, "pauseOnBackground");
    lua::Stack::push(L, lifecycle.pauseOnFocusLoss);
    lua_setfield(L, -2, "pauseOnFocusLoss");
    lua::Stack::push(L, lifecycle.muteOnFocusLoss);
    lua_setfield(L, -2, "muteOnFocusLoss");
    return 1;
}

// Changes the lifecycle options that the table names and keeps the others.
int CoreLua::rootSetLifecycle(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kLifecycleFields});
    Engine& engine = lua::Runtime::getEngine(L);
    AppConfig::Lifecycle lifecycle = engine.getLifecycle();
    lua::Table::readField(L, 1, "pauseOnBackground", lifecycle.pauseOnBackground);
    lua::Table::readField(L, 1, "pauseOnFocusLoss", lifecycle.pauseOnFocusLoss);
    lua::Table::readField(L, 1, "muteOnFocusLoss", lifecycle.muteOnFocusLoss);
    engine.setLifecycle(lifecycle);
    return 0;
}

// Loads a module as an autoload with haylen.autoload(name, module) and returns its table. The name defaults to the last part of the module in camel case.
int CoreLua::rootAutoload(lua_State* L) {
    const std::string module = lua::Stack::read<std::string>(L, lua_isnoneornil(L, 2) ? 1 : 2);
    const std::string name = lua_isnoneornil(L, 2) ? lua::Autoloads::getName(module) : lua::Stack::read<std::string>(L, 1);
    lua::Runtime::getEngine(L).getPlugin<plugins::CorePlugin>().getAutoloads().add(L, name, module);
    lua::Autoloads::pushTable(L);
    lua_getfield(L, -1, name.c_str());
    return 1;
}

int CoreLua::openRoot(lua_State* L) {
    Engine& owner = lua::Runtime::getEngine(L);
    const luaL_Reg functions[] = {
        {"quit", &rootQuit}, {"time", &rootTime}, {"delta", &rootDelta}, {"unscaledDelta", &rootUnscaledDelta}, {"frame", &rootFrame}, {"timeScale", &rootTimeScale}, {"setTimeScale", &rootSetTimeScale}, {"fixedStep", &rootFixedStep}, {"interpolation", &rootInterpolation}, {"reportError", &lua::Binding::native<&rootReportError>}, {"paused", &rootPaused}, {"setPaused", &lua::Binding::native<&rootSetPaused>}, {"appState", &rootAppState}, {"networkState", &rootNetworkState}, {"halted", &rootHalted}, {"lifecycle", &rootLifecycle}, {"setLifecycle", &lua::Binding::native<&rootSetLifecycle>}, {"autoload", &lua::Binding::native<&rootAutoload>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    ClassLua::push(L);
    lua_setfield(L, -2, "class");
    lua::Autoloads::pushTable(L);
    lua_setfield(L, -2, "autoloads");
    lua::Stack::push(L, Version::kString);
    lua_setfield(L, -2, "version");
    lua::Stack::push(L, owner.getPlatformName());
    lua_setfield(L, -2, "platform");
    lua::Stack::push(L, owner.getGraphics().getBackendName());
    lua_setfield(L, -2, "backend");
    lua::JsonConverter::push(L, owner.getConfig().toJson());
    lua_setfield(L, -2, "config");
    return 1;
}

int CoreLua::viewportDesignSize(lua_State* L) {
    const math::Vec2 size = lua::Runtime::getEngine(L).getViewport().getDesignSize();
    lua::Stack::push(L, size.x);
    lua::Stack::push(L, size.y);
    return 2;
}

int CoreLua::viewportVisibleRect(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getVisibleRect());
    return 1;
}

int CoreLua::viewportSafeRect(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getSafeRect());
    return 1;
}

int CoreLua::viewportPixelRect(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getPixelRect());
    return 1;
}

int CoreLua::viewportPixelsPerUnit(lua_State* L) {
    const math::Vec2 scale = lua::Runtime::getEngine(L).getViewport().getPixelsPerUnit();
    lua::Stack::push(L, scale.x);
    lua::Stack::push(L, scale.y);
    return 2;
}

int CoreLua::viewportToDesign(lua_State* L) {
    const math::Vec2 point = lua::Runtime::getEngine(L).getViewport().toDesign({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int CoreLua::viewportToFramebuffer(lua_State* L) {
    const math::Vec2 point = lua::Runtime::getEngine(L).getViewport().toFramebuffer({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

int CoreLua::viewportScaling(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getViewport().getPolicy());
    return 1;
}

int CoreLua::viewportSetScaling(lua_State* L) {
    lua::Runtime::getEngine(L).setScaling(lua::Stack::read<graphics::Viewport::ScalingPolicy>(L, 1));
    return 0;
}

int CoreLua::viewportSetDesignSize(lua_State* L) {
    lua::Runtime::getEngine(L).setDesignSize({lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2)});
    return 0;
}

// Returns the simulated safe area as a device name or insets from the top clockwise, or nil while the device reports its own.
int CoreLua::viewportSafeAreaSimulation(lua_State* L) {
    const std::optional<platform::SafeAreaSimulation>& simulation = lua::Runtime::getEngine(L).getSafeAreaSimulation();
    if (!simulation) {
        lua_pushnil(L);
        return 1;
    }
    lua::JsonConverter::push(L, simulation->toJson());
    return 1;
}

// Simulates the safe area of a device by name or of insets in window points with setSafeAreaSimulation(value), and nil goes back to the safe area of the device.
int CoreLua::viewportSetSafeAreaSimulation(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    if (lua_isnoneornil(L, 1)) {
        engine.setSafeAreaSimulation(std::nullopt);
        return 0;
    }
    engine.setSafeAreaSimulation(platform::SafeAreaSimulation::fromJson(lua::JsonConverter::read(L, 1)));
    return 0;
}

int CoreLua::openViewport(lua_State* L) {
    const luaL_Reg functions[] = {
        {"designSize", &viewportDesignSize}, {"visibleRect", &viewportVisibleRect}, {"safeRect", &viewportSafeRect}, {"pixelRect", &viewportPixelRect}, {"pixelsPerUnit", &viewportPixelsPerUnit}, {"toDesign", &viewportToDesign}, {"toFramebuffer", &viewportToFramebuffer}, {"scaling", &viewportScaling}, {"setScaling", &viewportSetScaling}, {"setDesignSize", &lua::Binding::native<&viewportSetDesignSize>}, {"safeAreaSimulation", &viewportSafeAreaSimulation}, {"setSafeAreaSimulation", &lua::Binding::native<&viewportSetSafeAreaSimulation>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void CoreLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen", &lua::Binding::native<&openRoot>);
    lua::Binding::preload(L, "haylen.log", &openLog);
    lua::Binding::preload(L, "haylen.viewport", &openViewport);
}

} // namespace haylen::core
