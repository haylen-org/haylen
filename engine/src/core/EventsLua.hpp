#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string_view>
#include <typeindex>
#include <unordered_map>

#include "haylen/core/Connection.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/lua/Reference.hpp"
#include "lua/Owners.hpp"

struct lua_State;

namespace haylen::core {

// Installs haylen.events, the event bus of the engine for Lua: listeners by name with channels, priorities, filters, owners and consumption, and events delivered at once or at the end of the frame. Lifecycle events that the engine publishes reach Lua listeners with their JSON data as a table, or with the Lua value of the native value they carry, such as the table of a scene.
class EventsLua final {
  public:
    static void install(lua_State* L);

    // Subscribes the function at index to the event named at index with the listen options at index, or 0 for none, and ties it to the owner at index, or 0 for none. Scenes listen through it.
    static Connection subscribe(lua_State* L, int name, int function, int options, int owner);

    // Lets Lua listeners receive the native value of type T that engine events carry, which the function pushes as one Lua value. A context adds the types it publishes when it installs its Lua module.
    template <typename T> static void addPayload(void (*push)(lua_State* L, const T& value)) {
        payloads.insert_or_assign(std::type_index(typeid(T)), [push](lua_State* L, const EventBus::Event& event) { push(L, *event.get<T>()); });
    }

  private:
    using Payload = std::function<void(lua_State*, const EventBus::Event&)>;

    static constexpr std::array<std::string_view, 5> kListenFields{"channel", "priority", "once", "owner", "filter"};

    static std::unordered_map<std::type_index, Payload>& payloads;

    // The values of an event emitted from Lua, where they sit on the stack of the emitting thread.
    struct StackArguments {
        lua_State* state = nullptr;
        int first = 0;
        int count = 0;
    };

    // The values of an event queued from Lua, packed in a table until the end of the frame.
    struct StoredArguments {
        std::shared_ptr<lua::Reference> table;
        int count = 0;
    };

    // Pushes the values of the event onto L and returns how many there are.
    static int pushPayload(lua_State* L, const EventBus::Event& event);

    // Calls the function with the values of the event and returns whether it returned a true value. Events emitted from Lua run the function on the emitting thread, where errors reach the emitter, and other events run it on the main thread in a protected call.
    [[nodiscard]] static bool invoke(const lua::Owners::Function& function, lua_State* main, const EventBus::Event& event);
    static int emitOn(lua_State* L, std::string_view channel, int first);
    static int postOn(lua_State* L, std::string_view channel, int first);

    static int on(lua_State* L);
    static int emit(lua_State* L);
    static int emitTo(lua_State* L);
    static int post(lua_State* L);
    static int postTo(lua_State* L);
    static int topics(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::core
