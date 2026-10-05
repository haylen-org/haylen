#pragma once

#include <lua.hpp>

#include <memory>
#include <vector>

#include "haylen/core/Connection.hpp"
#include "haylen/core/ConnectionScope.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::core {
class FrameQueue;
}

namespace haylen::lua {

class Task;

// Ties listeners, tweens, timers, tasks and GUIs to the Lua value that owns them, such as a scene table, an autoload, a GUI or a game object. Everything an owner holds ends when the owner is released, which scenes do when they unload, or when the owner is garbage collected, which ends it at the end of the frame. An owner keeps the functions of its listeners, so a function that refers to its owner never keeps the owner alive.
class Owners final {
  public:
    // Holds the Lua function of a listener: strongly without an owner, or through the owner, in which case the function goes away with it.
    class Function final {
      public:
        // An owner index of 0 means no owner.
        Function(lua_State* L, int function, int owner);
        ~Function();

        Function(const Function&) = delete;
        Function& operator=(const Function&) = delete;

        // Pushes the function and returns `true`, or pushes nothing and returns `false` once its owner is gone.
        [[nodiscard]] bool push(lua_State* L) const;

      private:
        lua_State* state;
        Reference strong;
        lua_Integer key = 0;
    };

    // Raises a Lua error unless the value at index can own things, which tables and userdata can.
    static void checkOwner(lua_State* L, int index);

    // Returns a pointer that expires as soon as the owner at index is collected. Listeners take it as their owner, so they count as stale from the collection until they are removed.
    [[nodiscard]] static std::weak_ptr<const void> getLifetime(lua_State* L, int owner);

    // Ends the connection when the owner at index is released or collected.
    static void add(lua_State* L, int owner, core::Connection connection);

    // Holds the link, which nothing else keeps alive, and disconnects it when the owner at index is released or collected.
    static void add(lua_State* L, int owner, std::shared_ptr<core::Connection::Link> link);

    // Cancels the task when the owner at index is released or collected.
    static void addTask(lua_State* L, int owner, const std::shared_ptr<Task>& task);

    // Ends everything the owner at index holds right now.
    static void release(lua_State* L, int owner);

  private:
    static constexpr const char* kScopes = "haylen.owners";
    static constexpr const char* kFunctions = "haylen.ownedFunctions";
    static constexpr const char* kFunctionScopes = "haylen.ownedFunctionScopes";
    static constexpr const char* kScopeType = "haylen.OwnerScope";

    struct Scope {
        core::ConnectionScope connections;
        std::vector<std::shared_ptr<core::Connection::Link>> links;
        std::vector<std::weak_ptr<Task>> tasks;
        core::FrameQueue* queue = nullptr;
        std::shared_ptr<const void> lifetime = std::make_shared<bool>();
    };

    // Stops the tasks first, so no code of the owner resumes while its links and connections end.
    static void endHeld(std::vector<std::weak_ptr<Task>> tasks, std::vector<std::shared_ptr<core::Connection::Link>> links, core::ConnectionScope& connections);

    static void pushWeakTable(lua_State* L, const char* name, const char* mode);

    // Pushes the scope userdata of the owner and returns its scope, creating it on the first use.
    static Scope& pushScope(lua_State* L, int owner);

    // Returns the scope of the owner without creating one, or null.
    [[nodiscard]] static Scope* findScope(lua_State* L, int owner);

    static int collectScope(lua_State* L);
};

} // namespace haylen::lua
