#pragma once

#include <functional>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/storage/SaveSlots.hpp"

struct lua_State;

namespace haylen::storage {

class UserStorage;

// Installs haylen.storage, which reads and writes the private files of the user and the save slots kept among them, on the frame thread or, with the functions whose names end in Async, on the I/O pool.
class StorageLua final {
  public:
    static void install(lua_State* L);

  private:
    // Pushes the result of an operation on the frame thread.
    using Pusher = std::function<void(lua_State*)>;

    [[nodiscard]] static UserStorage& getUserStorage(lua_State* L);
    [[nodiscard]] static SaveSlots& getSaveSlots(lua_State* L);
    static void pushInfo(lua_State* L, const SaveSlots::Info& info);
    [[nodiscard]] static Pusher pushJson(core::Json value);
    [[nodiscard]] static Pusher pushInfos(std::vector<SaveSlots::Info> infos);

    // Queues work with the other storage operations on the I/O pool and pushes a promise that settles on the frame thread with what work returns, or rejects with what it threw. A durable operation flushes user storage before it settles.
    static int queue(lua_State* L, bool durable, std::function<Pusher()> work);

    static int read(lua_State* L);
    static int write(lua_State* L);
    static int readJson(lua_State* L);
    static int writeJson(lua_State* L);
    static int exists(lua_State* L);
    static int remove(lua_State* L);
    static int list(lua_State* L);
    static int flush(lua_State* L);
    static int root(lua_State* L);
    static int writeSlot(lua_State* L);
    static int readSlot(lua_State* L);
    static int slotInfo(lua_State* L);
    static int slotExists(lua_State* L);
    static int removeSlot(lua_State* L);
    static int listSlots(lua_State* L);
    static int readAsync(lua_State* L);
    static int writeAsync(lua_State* L);
    static int readJsonAsync(lua_State* L);
    static int writeJsonAsync(lua_State* L);
    static int removeAsync(lua_State* L);
    static int listAsync(lua_State* L);
    static int writeSlotAsync(lua_State* L);
    static int readSlotAsync(lua_State* L);
    static int slotInfoAsync(lua_State* L);
    static int removeSlotAsync(lua_State* L);
    static int listSlotsAsync(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::storage
