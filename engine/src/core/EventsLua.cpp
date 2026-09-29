#include "core/EventsLua.hpp"

#include <lua.hpp>

#include <string>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::core {

std::unordered_map<std::type_index, EventsLua::Payload> EventsLua::payloads;

int EventsLua::pushPayload(lua_State* L, const EventBus::Event& event) {
    if (const StackArguments* arguments = event.get<StackArguments>()) {
        luaL_checkstack(L, arguments->count + 1, "too many values to deliver");
        for (int index = 0; index < arguments->count; ++index) {
            lua_pushvalue(L, arguments->first + index);
        }
        return arguments->count;
    }
    if (const StoredArguments* arguments = event.get<StoredArguments>()) {
        luaL_checkstack(L, arguments->count + 2, "too many values to deliver");
        arguments->table->push(L);
        const int table = lua_gettop(L);
        for (int index = 1; index <= arguments->count; ++index) {
            lua_rawgeti(L, table, index);
        }
        lua_remove(L, table);
        return arguments->count;
    }
    if (const auto payload = payloads.find(event.getType()); payload != payloads.end()) {
        payload->second(L, event);
        return 1;
    }
    if (event.getData().is_null()) {
        return 0;
    }
    lua::JsonConverter::push(L, event.getData());
    return 1;
}

bool EventsLua::invoke(const lua::Owners::Function& function, lua_State* main, const EventBus::Event& event) {
    const StackArguments* arguments = event.get<StackArguments>();
    lua_State* L = arguments != nullptr ? arguments->state : main;
    if (!function.push(L)) {
        return false;
    }
    const int count = pushPayload(L, event);
    if (arguments != nullptr) {
        lua_call(L, count, 1);
    } else {
        lua::Runtime::protectedCall(L, count, 1);
    }
    const bool result = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    return result;
}

Connection EventsLua::subscribe(lua_State* L, int name, int function, int options, int owner) {
    const std::string eventName = lua::Stack::read<std::string>(L, name);
    luaL_checktype(L, function, LUA_TFUNCTION);
    const int top = lua_gettop(L);
    EventBus::Options settings;
    int ownerIndex = owner;
    int filterIndex = 0;
    if (options != 0) {
        luaL_checktype(L, options, LUA_TTABLE);
        lua::Table::checkFields(L, options, {kListenFields});
        lua::Table::readField(L, options, "channel", settings.channel);
        lua::Table::readField(L, options, "priority", settings.priority);
        lua::Table::readField(L, options, "once", settings.once);
        if (ownerIndex == 0 && lua_getfield(L, options, "owner") != LUA_TNIL) {
            ownerIndex = lua_gettop(L);
        }
        if (lua_getfield(L, options, "filter") != LUA_TNIL) {
            luaL_checktype(L, -1, LUA_TFUNCTION);
            filterIndex = lua_gettop(L);
        }
    }

    if (ownerIndex != 0) {
        settings.owner = lua::Owners::getLifetime(L, ownerIndex);
    }

    lua_State* main = lua::Runtime::getMainThread(L);
    auto callback = std::make_shared<lua::Owners::Function>(L, function, ownerIndex);
    if (filterIndex != 0) {
        auto filter = std::make_shared<lua::Owners::Function>(L, filterIndex, ownerIndex);
        settings.filter = [filter, main](const EventBus::Event& event) { return invoke(*filter, main, event); };
    }

    // A listener that returns true consumes the event, so listeners after it skip it.
    // clang-format off
    Connection connection = lua::Runtime::getEngine(L).getEvents().on(eventName, [callback, main](EventBus::Event& event) {
        if (invoke(*callback, main, event)) {
            event.consume();
        }
    }, std::move(settings));
    // clang-format on
    if (ownerIndex != 0) {
        lua::Owners::add(L, ownerIndex, connection);
    }
    lua_settop(L, top);
    return connection;
}

int EventsLua::on(lua_State* L) {
    lua::Userdata::emplace<Connection>(L, subscribe(L, 1, 2, lua_isnoneornil(L, 3) ? 0 : 3, 0));
    return 1;
}

int EventsLua::emitOn(lua_State* L, std::string_view channel, int first) {
    const std::string name = lua::Stack::read<std::string>(L, first);
    const StackArguments arguments{.state = L, .first = first + 1, .count = lua_gettop(L) - first};
    lua::Stack::push(L, lua::Runtime::getEngine(L).getEvents().emitWith(name, arguments, {}, channel));
    return 1;
}

int EventsLua::postOn(lua_State* L, std::string_view channel, int first) {
    std::string name = lua::Stack::read<std::string>(L, first);
    const int count = lua_gettop(L) - first;
    lua_createtable(L, count, 0);
    for (int index = 1; index <= count; ++index) {
        lua_pushvalue(L, first + index);
        lua_rawseti(L, -2, index);
    }
    StoredArguments arguments{.table = std::make_shared<lua::Reference>(L, -1), .count = count};
    lua_pop(L, 1);
    lua::Runtime::getEngine(L).getEvents().postWith(std::move(name), std::move(arguments), std::string(channel));
    return 0;
}

// Delivers the event right away with events.emit(name, ...) and returns whether a listener consumed it.
int EventsLua::emit(lua_State* L) {
    return emitOn(L, {}, 1);
}

int EventsLua::emitTo(lua_State* L) {
    const std::string channel = lua::Stack::read<std::string>(L, 1);
    return emitOn(L, channel, 2);
}

// Queues the event with events.post(name, ...) for the end of the frame.
int EventsLua::post(lua_State* L) {
    return postOn(L, {}, 1);
}

int EventsLua::postTo(lua_State* L) {
    const std::string channel = lua::Stack::read<std::string>(L, 1);
    return postOn(L, channel, 2);
}

// Returns {name, listeners, emissions, stale} for every event that has ever had a listener.
int EventsLua::stats(lua_State* L) {
    const std::vector<EventBus::Topic> topics = lua::Runtime::getEngine(L).getEvents().getTopics();
    lua_createtable(L, static_cast<int>(topics.size()), 0);
    lua_Integer index = 0;
    for (const EventBus::Topic& topic : topics) {
        lua_createtable(L, 0, 4);
        lua::Stack::push(L, topic.name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, topic.listeners);
        lua_setfield(L, -2, "listeners");
        lua::Stack::push(L, topic.emissions);
        lua_setfield(L, -2, "emissions");
        lua::Stack::push(L, topic.stale);
        lua_setfield(L, -2, "stale");
        lua_rawseti(L, -2, ++index);
    }
    return 1;
}

int EventsLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"on", &lua::Binding::native<&on>}, {"emit", &lua::Binding::native<&emit>}, {"emitTo", &lua::Binding::native<&emitTo>}, {"post", &lua::Binding::native<&post>}, {"postTo", &lua::Binding::native<&postTo>}, {"stats", &lua::Binding::native<&stats>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void EventsLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.events", &open);
}

} // namespace haylen::core
