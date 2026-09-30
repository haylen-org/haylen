#pragma once

struct lua_State;

namespace haylen::plugins {
class StoragePlugin;
}

namespace haylen::storage {

// Installs `haylen.preferences`, which keeps the choices of the player between sessions.
class PreferencesLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static plugins::StoragePlugin& getPlugin(lua_State* L);

    static int get(lua_State* L);
    static int set(lua_State* L);
    static int has(lua_State* L);
    static int remove(lua_State* L);
    static int clear(lua_State* L);
    static int save(lua_State* L);
    static int load(lua_State* L);
    static int dirty(lua_State* L);
    static int values(lua_State* L);
    static int capture(lua_State* L);
    static int apply(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::storage
