#pragma once

struct lua_State;

namespace haylen::localization {

class Catalog;

// Installs haylen.localization, which adds languages, picks the current one and translates keys.
class LocalizationLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static Catalog& getCatalog(lua_State* L);

    static int add(lua_State* L);
    static int loadFolder(lua_State* L);
    static int setLanguage(lua_State* L);
    static int language(lua_State* L);
    static int setFallback(lua_State* L);
    static int fallback(lua_State* L);
    static int languages(lua_State* L);
    static int has(lua_State* L);
    static int text(lua_State* L);
    static int bestMatch(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::localization
