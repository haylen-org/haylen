#pragma once

#include "haylen/storage/SaveSlots.hpp"

struct lua_State;

namespace haylen::storage {

class UserStorage;

// Installs haylen.storage, which reads and writes the private files of the user and the save slots kept among them.
class StorageLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static UserStorage& getUserStorage(lua_State* L);
    [[nodiscard]] static SaveSlots& getSaveSlots(lua_State* L);
    static void pushInfo(lua_State* L, const SaveSlots::Info& info);

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
    static int open(lua_State* L);
};

} // namespace haylen::storage
