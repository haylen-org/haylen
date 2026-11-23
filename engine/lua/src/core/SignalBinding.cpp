#include "core/SignalBinding.hpp"

#include <lua.hpp>

#include <memory>
#include <string>
#include <utility>

#include "core/ScriptedSignal.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FrameQueue.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua {

template <> struct Type<lua::core::ScriptedSignal> {
    static constexpr const char* name = "haylen.Signal";
    using Storage = std::shared_ptr<lua::core::ScriptedSignal>;
};

} // namespace haylen::lua

namespace haylen::lua::core {

void SignalBinding::pushArguments(lua_State* L, int first, int count) {
    luaL_checkstack(L, count + 1, "too many values to deliver");
    for (int index = 0; index < count; ++index) {
        lua_pushvalue(L, first + index);
    }
}

int SignalBinding::create(lua_State* L) {
    auto created = std::make_shared<ScriptedSignal>();
    if (!lua_isnoneornil(L, 1)) {
        created->name = lua::Stack::read<std::string>(L, 1);
    }

    lua::Userdata::emplace<ScriptedSignal>(L, created);

    // Named signals are listed for diagnostics without being kept alive by the list.
    if (!created->name.empty()) {
        if (lua_getfield(L, LUA_REGISTRYINDEX, kNamedSignals) != LUA_TTABLE) {
            lua_pop(L, 1);
            lua_newtable(L);
            lua_createtable(L, 0, 1);
            lua_pushliteral(L, "k");
            lua_setfield(L, -2, "__mode");
            lua_setmetatable(L, -2);
            lua_pushvalue(L, -1);
            lua_setfield(L, LUA_REGISTRYINDEX, kNamedSignals);
        }

        lua_pushvalue(L, -2);
        lua_pushboolean(L, 1);
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

    return 1;
}

haylen::core::Connection SignalBinding::connect(lua_State* L, int signal, int function, int options, int owner) {
    ScriptedSignal& target = lua::Userdata::check<ScriptedSignal>(L, signal);
    luaL_checktype(L, function, LUA_TFUNCTION);
    const int top = lua_gettop(L);
    haylen::core::Signal<const ScriptedSignal::Arguments&>::Options settings;
    bool deferred = false;
    int ownerIndex = owner;
    if (options != 0) {
        luaL_checktype(L, options, LUA_TTABLE);
        lua::Table::checkFields(L, options, {kConnectFields});
        lua::Table::readField(L, options, "priority", settings.priority);
        lua::Table::readField(L, options, "once", settings.once);
        lua::Table::readField(L, options, "deferred", deferred);
        if (ownerIndex == 0 && lua_getfield(L, options, "owner") != LUA_TNIL) {
            ownerIndex = lua_gettop(L);
        }
    }

    if (ownerIndex == 0) {
        ownerIndex = lua::Owners::pushDefault(L);
    }

    if (ownerIndex != 0) {
        settings.owner = lua::Owners::getLifetime(L, ownerIndex);
    }

    // A listener without an owner is held through the signal, so a listener that refers to the signal never keeps it alive.
    auto callback = std::make_shared<lua::Owners::Function>(L, function, ownerIndex != 0 ? ownerIndex : signal);

    haylen::core::FrameQueue& queue = lua::Runtime::getEngine(L).getFrameQueue();
    Slot slot = deferred ? makeDeferredSlot(callback, settings.once, queue) : makeSlot(callback);
    haylen::core::Connection connection = target.signal.connect(std::move(slot), settings);

    if (ownerIndex != 0) {
        lua::Owners::add(L, ownerIndex, connection);
    }

    lua_settop(L, top);
    return connection;
}

SignalBinding::Slot SignalBinding::makeSlot(std::shared_ptr<lua::Owners::Function> callback) {
    // clang-format off
    return [callback = std::move(callback)](const ScriptedSignal::Arguments& arguments) {
        lua_State* source = arguments.state;
        if (!callback->push(source)) {
            return;
        }

        pushArguments(source, arguments.first, arguments.count);
        lua_call(source, arguments.count, 0);
    };
    // clang-format on
}

// A deferred listener runs at the end of the frame with the values packed at emit time, and not at all once it disconnects before then. A once listener disconnects as it is called, so its queued call keeps it.
SignalBinding::Slot SignalBinding::makeDeferredSlot(std::shared_ptr<lua::Owners::Function> callback, bool once, haylen::core::FrameQueue& queue) {
    // clang-format off
    return [callback = std::move(callback), once, &queue](const ScriptedSignal::Arguments& arguments) {
        std::shared_ptr<lua::Reference> packed = packArguments(arguments);
        queue.post([weak = std::weak_ptr<lua::Owners::Function>(callback), held = once ? callback : nullptr, packed = std::move(packed), count = arguments.count] {
            deliverPacked(weak.lock(), *packed, count);
        });
    };
    // clang-format on
}

std::shared_ptr<lua::Reference> SignalBinding::packArguments(const ScriptedSignal::Arguments& arguments) {
    lua_State* source = arguments.state;
    lua_createtable(source, arguments.count, 0);

    for (int index = 0; index < arguments.count; ++index) {
        lua_pushvalue(source, arguments.first + index);
        lua_rawseti(source, -2, index + 1);
    }

    auto packed = std::make_shared<lua::Reference>(source, -1);
    lua_pop(source, 1);
    return packed;
}

void SignalBinding::deliverPacked(const std::shared_ptr<lua::Owners::Function>& listener, const lua::Reference& packed, int count) {
    lua_State* main = packed.getState();
    luaL_checkstack(main, count + 2, "too many values to deliver");

    if (!listener || !listener->push(main)) {
        return;
    }

    packed.push(main);
    const int table = lua_gettop(main);
    for (int index = 1; index <= count; ++index) {
        lua_rawgeti(main, table, index);
    }

    lua_remove(main, table);
    lua::Runtime::protectedCall(main, count, 0);
}

bool SignalBinding::isSignal(lua_State* L, int index) {
    return lua::Userdata::test<ScriptedSignal>(L, index) != nullptr;
}

int SignalBinding::connectFunction(lua_State* L) {
    lua::Userdata::emplace<haylen::core::Connection>(L, connect(L, 1, 2, lua_isnoneornil(L, 3) ? 0 : 3, 0));
    return 1;
}

// Calls every connected function with the values of `signal:emit(...)`. Errors reach the caller and skip the remaining listeners.
int SignalBinding::emit(lua_State* L) {
    ScriptedSignal& target = lua::Userdata::check<ScriptedSignal>(L, 1);
    target.signal.emit({.state = L, .first = 2, .count = lua_gettop(L) - 1});
    return 0;
}

int SignalBinding::clear(lua_State* L) {
    lua::Userdata::check<ScriptedSignal>(L, 1).signal.clear();
    return 0;
}

int SignalBinding::size(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedSignal>(L, 1).signal.size());
    return 1;
}

int SignalBinding::emissionCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedSignal>(L, 1).signal.getEmissionCount());
    return 1;
}

int SignalBinding::name(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedSignal>(L, 1).name);
    return 1;
}

int SignalBinding::blocked(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedSignal>(L, 1).signal.isBlocked());
    return 1;
}

int SignalBinding::setBlocked(lua_State* L) {
    lua::Userdata::check<ScriptedSignal>(L, 1).signal.setBlocked(lua::Stack::read<bool>(L, 3));
    return 0;
}

void SignalBinding::readNamedSignals(lua_State* L, std::vector<haylen::core::EventBus::Topic>& signals) {
    std::size_t count = 0;
    if (lua_getfield(L, LUA_REGISTRYINDEX, kNamedSignals) != LUA_TTABLE) {
        lua_pop(L, 1);
        signals.clear();
        return;
    }

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        lua_pop(L, 1);
        const ScriptedSignal* signal = lua::Userdata::test<ScriptedSignal>(L, -1);
        if (signal == nullptr) {
            continue;
        }

        if (count == signals.size()) {
            signals.emplace_back();
        }

        haylen::core::EventBus::Topic& topic = signals[count++];
        topic.name = signal->name;
        topic.listeners = signal->signal.size();
        topic.emissions = signal->signal.getEmissionCount();
        topic.stale = signal->signal.getStaleCount();
    }

    lua_pop(L, 1);
    signals.resize(count);
}

// Returns `{name, listeners, emissions, stale}` for every named signal that is still alive.
int SignalBinding::list(lua_State* L) {
    std::vector<haylen::core::EventBus::Topic> signals;
    readNamedSignals(L, signals);
    lua_createtable(L, static_cast<int>(signals.size()), 0);
    lua_Integer count = 0;
    for (const haylen::core::EventBus::Topic& signal : signals) {
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, signal.name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, signal.listeners);
        lua_setfield(L, -2, "listeners");
        lua::Stack::push(L, signal.emissions);
        lua_setfield(L, -2, "emissions");
        lua::Stack::push(L, signal.stale);
        lua_setfield(L, -2, "stale");
        lua_rawseti(L, -2, ++count);
    }

    return 1;
}

int SignalBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"new", &lua::Binding::native<&create>},
        {"list", &list},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

int SignalBinding::connectionDisconnect(lua_State* L) {
    lua::Userdata::check<haylen::core::Connection>(L, 1).disconnect();
    return 0;
}

int SignalBinding::connectionConnected(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::core::Connection>(L, 1).isConnected());
    return 1;
}

int SignalBinding::connectionBlocked(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::core::Connection>(L, 1).isBlocked());
    return 1;
}

int SignalBinding::connectionSetBlocked(lua_State* L) {
    lua::Userdata::check<haylen::core::Connection>(L, 1).setBlocked(lua::Stack::read<bool>(L, 3));
    return 0;
}

void SignalBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::core::Connection>(L).function("disconnect", &lua::Binding::native<&connectionDisconnect>).property("connected", &connectionConnected).property("blocked", &connectionBlocked, &lua::Binding::native<&connectionSetBlocked>).install();
    lua::ClassBuilder<ScriptedSignal>(L).function("connect", &lua::Binding::native<&connectFunction>).function("emit", &lua::Binding::native<&emit>).function("clear", &clear).property("size", &size).property("emissionCount", &emissionCount).property("name", &name).property("blocked", &blocked, &setBlocked).install();
    lua::Binding::preload(L, "haylen.signal", &open);
}

} // namespace haylen::lua::core
