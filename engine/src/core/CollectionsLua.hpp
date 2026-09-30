#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "haylen/core/RingBuffer.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::core {

// Installs `haylen.collections` with object pools that recycle Lua values, such as sprites and projectiles, ring buffers of Lua values over `core::RingBuffer` and float buffers shared with C++ over `core::FloatBuffer`.
class CollectionsLua final {
  public:
    static void install(lua_State* L);

    using ValueBuffer = RingBuffer<lua::Reference>;

    // The counts of a pool. Its user value holds the functions, the idle objects and the active objects with their slots.
    struct Pool {
        std::size_t capacity = 0;
        std::size_t active = 0;
        std::size_t idle = 0;
    };

  private:
    static constexpr std::array<std::string_view, 5> kPoolFields{"create", "reset", "release", "capacity", "prewarm"};
    static constexpr std::array<const char*, 2> kHooks{"reset", "release"};
    static constexpr std::array<const char*, 3> kLists{"idle", "active", "slots"};

    // Calls the pool function under `name` with the object and the arguments from `first` on, when the pool has that function.
    static void callHook(lua_State* L, const char* name, int object, int first, int count);
    // Pushes a new object from the `create` function, raising an error when it returns `nil`.
    static void create(lua_State* L);
    static void prewarm(lua_State* L, Pool& pool, std::size_t count);
    // Pushes a list of the active objects, which callbacks may change while it is walked.
    static void pushActive(lua_State* L, const Pool& pool);
    static bool releaseObject(lua_State* L, Pool& pool, int object);

    static int newPool(lua_State* L);
    static int poolAcquire(lua_State* L);
    static int poolRelease(lua_State* L);
    static int poolReleaseAll(lua_State* L);
    static int poolEach(lua_State* L);
    static int poolPrewarm(lua_State* L);
    static int poolActive(lua_State* L);
    static int poolIdle(lua_State* L);
    static int poolCapacity(lua_State* L);

    static int newRingBuffer(lua_State* L);
    static int bufferPush(lua_State* L);
    static int bufferPop(lua_State* L);
    static int bufferFront(lua_State* L);
    static int bufferBack(lua_State* L);
    static int bufferGet(lua_State* L);
    static int bufferValues(lua_State* L);
    static int bufferClear(lua_State* L);
    static int bufferSize(lua_State* L);
    static int bufferCapacity(lua_State* L);
    static int bufferFull(lua_State* L);

    static int open(lua_State* L);
};

} // namespace haylen::core
