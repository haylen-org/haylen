#include "core/EventsBinding.hpp"

#include <lua.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::core {

std::unordered_map<std::type_index, EventsBinding::Payload>& EventsBinding::payloads = *new std::unordered_map<std::type_index, Payload>();

int EventsBinding::pushPayload(lua_State* L, const haylen::core::EventBus::Event& event) {
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

bool EventsBinding::invoke(const lua::Owners::Function& function, lua_State* main, const haylen::core::EventBus::Event& event) {
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

haylen::core::Connection EventsBinding::subscribe(lua_State* L, int name, int function, int options, int owner) {
    const std::string eventName = lua::Stack::read<std::string>(L, name);
    luaL_checktype(L, function, LUA_TFUNCTION);
    const int top = lua_gettop(L);
    haylen::core::EventBus::Options settings;
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
            lua::Table::requireType(L, "filter", LUA_TFUNCTION);
            filterIndex = lua_gettop(L);
        }
    }

    if (ownerIndex == 0) {
        ownerIndex = lua::Owners::pushDefault(L);
    }

    if (ownerIndex != 0) {
        settings.owner = lua::Owners::getLifetime(L, ownerIndex);
    }

    lua_State* main = lua::Runtime::getMainThread(L);
    auto callback = std::make_shared<lua::Owners::Function>(L, function, ownerIndex);

    if (filterIndex != 0) {
        auto filter = std::make_shared<lua::Owners::Function>(L, filterIndex, ownerIndex);
        settings.filter = [filter, main](const haylen::core::EventBus::Event& event) { return invoke(*filter, main, event); };
    }

    // A listener that returns `true` consumes the event, so listeners after it skip it.
    // clang-format off
    haylen::core::Connection connection = lua::Runtime::getEngine(L).getEvents().on(eventName, [callback, main](haylen::core::EventBus::Event& event) {
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

int EventsBinding::on(lua_State* L) {
    lua::Userdata::emplace<haylen::core::Connection>(L, subscribe(L, 1, 2, lua_isnoneornil(L, 3) ? 0 : 3, 0));
    return 1;
}

int EventsBinding::emitOn(lua_State* L, std::string_view channel, int first) {
    const std::string name = lua::Stack::read<std::string>(L, first);
    const StackArguments arguments{.state = L, .first = first + 1, .count = lua_gettop(L) - first};
    lua::Stack::push(L, lua::Runtime::getEngine(L).getEvents().emitWith(name, arguments, {}, channel));
    return 1;
}

int EventsBinding::postOn(lua_State* L, std::string_view channel, int first) {
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

// Delivers the event right away with `events.emit(name, ...)` and returns whether a listener consumed it.
int EventsBinding::emit(lua_State* L) {
    return emitOn(L, {}, 1);
}

int EventsBinding::emitTo(lua_State* L) {
    const std::string channel = lua::Stack::read<std::string>(L, 1);
    return emitOn(L, channel, 2);
}

// Queues the event with `events.post(name, ...)` for the end of the frame.
int EventsBinding::post(lua_State* L) {
    return postOn(L, {}, 1);
}

int EventsBinding::postTo(lua_State* L) {
    const std::string channel = lua::Stack::read<std::string>(L, 1);
    return postOn(L, channel, 2);
}

// Returns `{name, listeners, emissions, stale}` for every event that has ever had a listener.
int EventsBinding::topics(lua_State* L) {
    std::vector<haylen::core::EventBus::Topic> topics;
    lua::Runtime::getEngine(L).getEvents().readTopics(topics);
    lua_createtable(L, static_cast<int>(topics.size()), 0);
    lua_Integer index = 0;
    for (const haylen::core::EventBus::Topic& topic : topics) {
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

int EventsBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"on", &lua::Binding::native<&on>}, {"emit", &lua::Binding::native<&emit>}, {"emitTo", &lua::Binding::native<&emitTo>}, {"post", &lua::Binding::native<&post>}, {"postTo", &lua::Binding::native<&postTo>}, {"topics", &lua::Binding::native<&topics>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void EventsBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.events", &open);
}

} // namespace haylen::lua::core
