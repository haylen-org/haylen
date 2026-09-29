#include "debug/DebugLua.hpp"

#include <lua.hpp>

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/debug/ProfileScope.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/debug/Stats.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/DebugPlugin.hpp"
#include "haylen/plugins/HotReloadPlugin.hpp"
#include "lua/Owners.hpp"

namespace haylen::lua {

template <> struct EnumNames<debug::StatsDisplay::Mode> {
    static std::optional<debug::StatsDisplay::Mode> fromName(std::string_view name) {
        return debug::StatsDisplay::modeFromName(name);
    }
    static std::string_view name(debug::StatsDisplay::Mode value) {
        return debug::StatsDisplay::modeName(value);
    }
};

} // namespace haylen::lua

namespace haylen::debug {

plugins::DebugPlugin& DebugLua::getPlugin(lua_State* L) {
    return lua::Runtime::getEngine(L).getPlugin<plugins::DebugPlugin>();
}

// Picks what the statistics show with setStatsMode('off'), 'compact' or 'full'.
int DebugLua::setStatsMode(lua_State* L) {
    getPlugin(L).setStatsMode(lua::Stack::read<StatsDisplay::Mode>(L, 1));
    return 0;
}

int DebugLua::statsMode(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getStatsMode());
    return 1;
}

// Picks the key that cycles the statistics with setToggleKey(name), or turns the shortcut off with nil.
int DebugLua::setToggleKey(lua_State* L) {
    getPlugin(L).setToggleKey(lua_isnoneornil(L, 1) ? std::nullopt : std::optional<input::Key>(lua::Stack::read<input::Key>(L, 1)));
    return 0;
}

int DebugLua::toggleKey(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).getToggleKey());
    return 1;
}

int DebugLua::setObjectEvents(lua_State* L) {
    getPlugin(L).setObjectEvents(lua::Stack::read<bool>(L, 1));
    return 0;
}

int DebugLua::objectEvents(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).hasObjectEvents());
    return 1;
}

int DebugLua::hotReloadWatching(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPlugin<plugins::HotReloadPlugin>().isWatching());
    return 1;
}

void DebugLua::pushStats(lua_State* L, const Stats& snapshot) {
    lua_createtable(L, 0, 7);

    lua_createtable(L, 0, 7);
    lua::Stack::push(L, snapshot.frame.fps);
    lua_setfield(L, -2, "fps");
    lua::Stack::push(L, snapshot.frame.milliseconds);
    lua_setfield(L, -2, "milliseconds");
    lua::Stack::push(L, snapshot.frame.average);
    lua_setfield(L, -2, "average");
    lua::Stack::push(L, snapshot.frame.minimum);
    lua_setfield(L, -2, "minimum");
    lua::Stack::push(L, snapshot.frame.maximum);
    lua_setfield(L, -2, "maximum");
    lua::Stack::push(L, snapshot.frame.onePercentLow);
    lua_setfield(L, -2, "onePercentLow");
    lua::Stack::push(L, snapshot.frame.fixedSteps);
    lua_setfield(L, -2, "fixedSteps");
    lua_setfield(L, -2, "frame");

    const graphics2d::Renderer::Stats& rendering = snapshot.rendering;
    lua_createtable(L, 0, 12);
    lua::Stack::push(L, rendering.canvases);
    lua_setfield(L, -2, "canvases");
    lua::Stack::push(L, rendering.passes);
    lua_setfield(L, -2, "passes");
    lua::Stack::push(L, rendering.drawCalls);
    lua_setfield(L, -2, "drawCalls");
    lua::Stack::push(L, rendering.sprites);
    lua_setfield(L, -2, "sprites");
    lua::Stack::push(L, rendering.instances);
    lua_setfield(L, -2, "instances");
    lua::Stack::push(L, rendering.vertices);
    lua_setfield(L, -2, "vertices");
    lua::Stack::push(L, rendering.indices);
    lua_setfield(L, -2, "indices");
    lua::Stack::push(L, rendering.lights);
    lua_setfield(L, -2, "lights");
    lua::Stack::push(L, rendering.occluders);
    lua_setfield(L, -2, "occluders");
    lua::Stack::push(L, rendering.shadows);
    lua_setfield(L, -2, "shadows");
    lua::Stack::push(L, rendering.textureSwitches);
    lua_setfield(L, -2, "textureSwitches");
    lua::Stack::push(L, rendering.uploadedBytes);
    lua_setfield(L, -2, "uploadedBytes");
    lua_setfield(L, -2, "rendering");

    lua_createtable(L, 0, 4);
    lua::Stack::push(L, snapshot.memory.lua);
    lua_setfield(L, -2, "lua");
    lua::Stack::push(L, snapshot.memory.textures);
    lua_setfield(L, -2, "textures");
    lua::Stack::push(L, snapshot.memory.targets);
    lua_setfield(L, -2, "targets");
    lua::Stack::push(L, snapshot.memory.sounds);
    lua_setfield(L, -2, "sounds");
    lua_setfield(L, -2, "memory");

    const Stats::Counts& counts = snapshot.counts;
    lua_createtable(L, 0, 10);
    lua::Stack::push(L, counts.scenes);
    lua_setfield(L, -2, "scenes");
    lua::Stack::push(L, counts.tweens);
    lua_setfield(L, -2, "tweens");
    lua::Stack::push(L, counts.timers);
    lua_setfield(L, -2, "timers");
    lua::Stack::push(L, counts.voices);
    lua_setfield(L, -2, "voices");
    lua::Stack::push(L, counts.assetsCached);
    lua_setfield(L, -2, "assetsCached");
    lua::Stack::push(L, counts.assetsPending);
    lua_setfield(L, -2, "assetsPending");
    lua::Stack::push(L, counts.sockets);
    lua_setfield(L, -2, "sockets");
    lua::Stack::push(L, counts.bodies);
    lua_setfield(L, -2, "bodies");
    lua::Stack::push(L, counts.contacts);
    lua_setfield(L, -2, "contacts");
    lua::Stack::push(L, counts.particles);
    lua_setfield(L, -2, "particles");
    lua_setfield(L, -2, "counts");

    lua_createtable(L, 0, static_cast<int>(snapshot.pools.size()));
    for (const graphics::Device::Pool& pool : snapshot.pools) {
        lua_createtable(L, 0, 2);
        lua::Stack::push(L, pool.used);
        lua_setfield(L, -2, "used");
        lua::Stack::push(L, pool.size);
        lua_setfield(L, -2, "size");
        lua_setfield(L, -2, std::string(pool.name).c_str());
    }
    lua_setfield(L, -2, "pools");

    lua_createtable(L, 0, static_cast<int>(snapshot.buses.size()));
    for (const audio::Mixer::BusStats& bus : snapshot.buses) {
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, bus.voices);
        lua_setfield(L, -2, "voices");
        lua::Stack::push(L, bus.playing);
        lua_setfield(L, -2, "playing");
        lua::Stack::push(L, bus.paused);
        lua_setfield(L, -2, "paused");
        lua::Stack::push(L, bus.processing);
        lua_setfield(L, -2, "processing");
        lua_setfield(L, -2, bus.name.c_str());
    }
    lua_setfield(L, -2, "buses");

    lua_createtable(L, 0, static_cast<int>(snapshot.objects.size()));
    for (const ObjectCounter::Snapshot& object : snapshot.objects) {
        lua_createtable(L, 0, 5);
        lua_pushstring(L, object.kind == ObjectCounter::Kind::Userdata ? "userdata" : "native");
        lua_setfield(L, -2, "kind");
        lua::Stack::push(L, object.created);
        lua_setfield(L, -2, "created");
        lua::Stack::push(L, object.alive);
        lua_setfield(L, -2, "alive");
        lua::Stack::push(L, object.destroyed);
        lua_setfield(L, -2, "destroyed");
        lua::Stack::push(L, object.bytes);
        lua_setfield(L, -2, "bytes");
        lua_setfield(L, -2, object.name.c_str());
    }
    lua_setfield(L, -2, "objects");
}

int DebugLua::stats(lua_State* L) {
    pushStats(L, getPlugin(L).captureStats(lua::Runtime::getEngine(L)));
    return 1;
}

// Adds a monitor with addMonitor(name, fn, {owner}), where fn returns the number to show and runs once per frame, and returns its connection. An owner holds the function and removes the monitor when it ends.
int DebugLua::addMonitor(lua_State* L) {
    std::string name = lua::Stack::read<std::string>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kMonitorFields});
        if (lua_getfield(L, 3, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }

    auto function = std::make_shared<lua::Owners::Function>(L, 2, owner);
    lua_State* main = lua::Runtime::getMainThread(L);
    // clang-format off
    core::Connection connection = getPlugin(L).addMonitor(std::move(name), [function, main]() -> std::optional<double> {
        if (!function->push(main)) {
            return std::nullopt;
        }
        lua::Runtime::protectedCall(main, 0, 1);
        int isNumber = 0;
        const lua_Number value = lua_tonumberx(main, -1, &isNumber);
        lua_pop(main, 1);
        if (isNumber == 0) {
            throw std::runtime_error("A debug monitor function returns a number.");
        }
        return static_cast<double>(value);
    });
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

int DebugLua::removeMonitor(lua_State* L) {
    lua::Stack::push(L, getPlugin(L).removeMonitor(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

// Returns {name, value, history} for every monitor, with the history oldest first.
int DebugLua::monitors(lua_State* L) {
    const std::vector<std::shared_ptr<Monitor>> all = getPlugin(L).getMonitors();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, all[index]->getName());
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, all[index]->getValue());
        lua_setfield(L, -2, "value");
        lua::Stack::push(L, all[index]->getHistory());
        lua_setfield(L, -2, "history");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Returns the last finished frame as {milliseconds, average, fps, scopes = {{name, milliseconds, calls, depth}}}.
int DebugLua::frame(lua_State* L) {
    const Profiler& profiler = lua::Runtime::getEngine(L).getProfiler();
    const double average = profiler.getAverageFrameMilliseconds();
    lua_createtable(L, 0, 4);
    lua::Stack::push(L, profiler.getLastFrameMilliseconds());
    lua_setfield(L, -2, "milliseconds");
    lua::Stack::push(L, average);
    lua_setfield(L, -2, "average");
    lua::Stack::push(L, average > 0.0 ? 1000.0 / average : 0.0);
    lua_setfield(L, -2, "fps");

    const std::vector<ProfileSample>& samples = profiler.getLastFrame();
    lua_createtable(L, static_cast<int>(samples.size()), 0);
    for (std::size_t index = 0; index < samples.size(); ++index) {
        const ProfileSample& sample = samples[index];
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, sample.name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, sample.milliseconds);
        lua_setfield(L, -2, "milliseconds");
        lua::Stack::push(L, sample.calls);
        lua_setfield(L, -2, "calls");
        lua::Stack::push(L, sample.depth);
        lua_setfield(L, -2, "depth");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "scopes");
    return 1;
}

int DebugLua::frameTimes(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getProfiler().getFrameHistory());
    return 1;
}

int DebugLua::beginScope(lua_State* L) {
    lua::Runtime::getEngine(L).getProfiler().begin(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int DebugLua::endScope(lua_State* L) {
    lua::Runtime::getEngine(L).getProfiler().end();
    return 0;
}

// Runs fn(...) inside a profiler scope with profile(name, fn, ...) and returns what fn returns.
int DebugLua::profile(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    const ProfileScope scope(lua::Runtime::getEngine(L).getProfiler(), name);
    lua_call(L, lua_gettop(L) - 2, LUA_MULTRET);
    return lua_gettop(L) - 1;
}

// Returns up to count recent log lines, oldest first, as {level, text} tables.
int DebugLua::recentLog(lua_State* L) {
    const std::vector<LogLine> lines = getPlugin(L).getRecentLog();
    const std::size_t count = lua_isnoneornil(L, 1) ? lines.size() : lua::Stack::read<std::size_t>(L, 1);
    const std::size_t first = lines.size() > count ? lines.size() - count : 0;
    lua_createtable(L, static_cast<int>(lines.size() - first), 0);
    for (std::size_t index = first; index < lines.size(); ++index) {
        lua_createtable(L, 0, 2);
        lua_pushstring(L, kLevelNames[static_cast<std::size_t>(lines[index].level)]);
        lua_setfield(L, -2, "level");
        lua::Stack::push(L, lines[index].text);
        lua_setfield(L, -2, "text");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index - first + 1));
    }
    return 1;
}

int DebugLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"setStatsMode", &lua::Binding::native<&setStatsMode>}, {"statsMode", &lua::Binding::native<&statsMode>}, {"setToggleKey", &lua::Binding::native<&setToggleKey>}, {"toggleKey", &lua::Binding::native<&toggleKey>}, {"setObjectEvents", &lua::Binding::native<&setObjectEvents>}, {"objectEvents", &lua::Binding::native<&objectEvents>}, {"hotReloadWatching", &lua::Binding::native<&hotReloadWatching>}, {"stats", &lua::Binding::native<&stats>}, {"addMonitor", &lua::Binding::native<&addMonitor>}, {"removeMonitor", &lua::Binding::native<&removeMonitor>}, {"monitors", &lua::Binding::native<&monitors>}, {"frame", &lua::Binding::native<&frame>}, {"frameTimes", &lua::Binding::native<&frameTimes>}, {"beginScope", &lua::Binding::native<&beginScope>}, {"endScope", &lua::Binding::native<&endScope>}, {"profile", &lua::Binding::native<&profile>}, {"recentLog", &lua::Binding::native<&recentLog>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void DebugLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.debug", &open);
}

} // namespace haylen::debug
