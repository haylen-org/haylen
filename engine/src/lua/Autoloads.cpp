#include "lua/Autoloads.hpp"

#include <lua.hpp>

#include <cctype>
#include <exception>
#include <stdexcept>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/FrameClock.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/platform/Event.hpp"
#include "input/InputLua.hpp"
#include "lua/ScriptedScene.hpp"

namespace haylen::lua {

std::string Autoloads::getName(std::string_view module) {
    const std::size_t dot = module.rfind('.');
    const std::string_view last = dot == std::string_view::npos ? module : module.substr(dot + 1);
    std::string name;
    bool upper = false;
    for (const char character : last) {
        if (character == '-' || character == '_') {
            upper = !name.empty();
            continue;
        }
        name.push_back(upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(character))) : character);
        upper = false;
    }
    return name;
}

void Autoloads::pushList(lua_State* L) const {
    lua_createtable(L, static_cast<int>(entries.size()), 0);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        entries[index].table.push(L);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
}

void Autoloads::pushTable(lua_State* L) {
    if (lua_getfield(L, LUA_REGISTRYINDEX, kTable) == LUA_TTABLE) {
        return;
    }
    lua_pop(L, 1);
    lua_newtable(L);
    lua_pushvalue(L, -1);
    lua_setfield(L, LUA_REGISTRYINDEX, kTable);
}

void Autoloads::add(lua_State* L, const std::string& name, const std::string& module) {
    if (name.empty()) {
        throw std::invalid_argument("An autoload needs a name.");
    }
    // Adding the same module under the same name again keeps the autoload, so a module that adds one at its top level can run again when it reloads.
    for (const Entry& entry : entries) {
        if (entry.name == name && entry.module == module) {
            return;
        }
        if (entry.name == name) {
            throw std::invalid_argument("An autoload named \"" + name + "\" already exists.");
        }
    }

    // The module loads on the main thread in a protected call, so a failure shows the stack of the module that failed.
    lua_State* main = Runtime::getMainThread(L);
    const core::Engine::PhaseScope scope(Runtime::getEngine(main), core::Engine::Phase::Lifecycle);
    lua_getglobal(main, "require");
    lua_pushstring(main, module.c_str());
    Runtime::protectedCall(main, 1, 1);
    if (!lua_istable(main, -1)) {
        lua_pop(main, 1);
        throw std::invalid_argument("The autoload module \"" + module + "\" must return a table.");
    }

    // One table under two names would receive every callback twice.
    for (const Entry& entry : entries) {
        entry.table.push(main);
        const bool same = lua_rawequal(main, -1, -2) != 0;
        lua_pop(main, 1);
        if (same) {
            lua_pop(main, 1);
            throw std::invalid_argument("The module \"" + module + "\" is already the autoload \"" + entry.name + "\".");
        }
    }
    pushTable(main);
    lua_pushvalue(main, -2);
    lua_setfield(main, -2, name.c_str());
    lua_pop(main, 1);
    entries.push_back({.name = name, .module = module, .table = Reference(main, -1)});
    lua_pop(main, 1);

    const Entry& added = entries.back();
    call(added, "start");
    Runtime::getEngine(main).getEvents().emit(core::LifecycleEvent::kAutoloadStarted, {{"name", name}});
}

void Autoloads::call(const Entry& entry, const char* method, int arguments, const std::function<void(lua_State*)>& pushArguments) {
    // clang-format off
    Runtime::protectedRun(entry.table.getState(), [&](lua_State* L) {
        entry.table.push(L);
        lua_getfield(L, -1, method);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return;
        }
        lua_insert(L, -2);
        if (pushArguments) {
            pushArguments(L);
        }
        lua_call(L, 1 + arguments, 0);
    });
    // clang-format on
}

bool Autoloads::canProcess(core::Engine& engine, const Entry& entry) {
    core::ProcessMode mode = core::ProcessMode::Inherit;
    // clang-format off
    Runtime::protectedRun(entry.table.getState(), [&](lua_State* L) {
        entry.table.push(L);
        mode = ScriptedScene::readProcessMode(L, -1);
        lua_pop(L, 1);
    });
    // clang-format on
    return engine.getClock().canProcess(mode);
}

// Autoloads are visited by index, since a callback may add another autoload, and an entry is only read before its callback runs. Input reaches only autoloads that run in the current pause state, like scenes.
void Autoloads::event(core::Engine& engine, const platform::Event& event) {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (!event.isInput() || canProcess(engine, entries[index])) {
            call(entries[index], "event", 1, [&event](lua_State* L) { input::InputLua::pushEvent(L, event); });
        }
    }
}

void Autoloads::fixedUpdate(core::Engine& engine, float stepSeconds) {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (canProcess(engine, entries[index])) {
            call(entries[index], "fixedUpdate", 1, [stepSeconds](lua_State* L) { lua_pushnumber(L, stepSeconds); });
        }
    }
}

void Autoloads::update(core::Engine& engine, float deltaSeconds) {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (canProcess(engine, entries[index])) {
            call(entries[index], "update", 1, [deltaSeconds](lua_State* L) { lua_pushnumber(L, deltaSeconds); });
        }
    }
}

void Autoloads::render() {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        call(entries[index], "render");
    }
}

void Autoloads::renderUi() {
    for (std::size_t index = 0; index < entries.size(); ++index) {
        call(entries[index], "renderUi");
    }
}

// Every autoload stops even when an earlier one fails, and the first failure is raised again at the end.
void Autoloads::stop(core::Engine& engine) {
    const core::Engine::PhaseScope scope(engine, core::Engine::Phase::Lifecycle);
    std::vector<Entry> stopping = std::exchange(entries, {});
    std::exception_ptr failure;
    // clang-format off
    const auto attempt = [&failure](const auto& step) {
        try {
            step();
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
    };
    // clang-format on
    for (auto entry = stopping.rbegin(); entry != stopping.rend(); ++entry) {
        attempt([&entry] { call(*entry, "stop"); });
        attempt([&engine, &entry] { engine.getEvents().emit(core::LifecycleEvent::kAutoloadStopped, {{"name", entry->name}}); });
    }
    if (failure) {
        std::rethrow_exception(failure);
    }
}

} // namespace haylen::lua
