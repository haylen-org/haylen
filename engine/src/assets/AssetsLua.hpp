#pragma once

#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"

struct lua_State;

namespace haylen::assets {

class Manager;

// Installs haylen.assets, which loads assets synchronously or through promises, manages preload groups and sets the upload budget of a frame.
class AssetsLua final {
  public:
    static void install(lua_State* L);

  private:
    [[nodiscard]] static Manager& getAssets(lua_State* L);
    [[nodiscard]] static core::Json readOptions(lua_State* L, int index);
    [[nodiscard]] static std::string readType(lua_State* L, int typeIndex, std::string_view path);

    static int texture(lua_State* L);
    static int font(lua_State* L);
    static int shader(lua_State* L);
    static int json(lua_State* L);
    static int text(lua_State* L);
    static int bytes(lua_State* L);
    static int exists(lua_State* L);
    static int typeForPath(lua_State* L);
    static int hasType(lua_State* L);
    static int list(lua_State* L);
    static int load(lua_State* L);
    static int loadAsync(lua_State* L);
    static int defineGroups(lua_State* L);
    static int defineGroup(lua_State* L);
    static int preload(lua_State* L);
    static int unload(lua_State* L);
    static int progress(lua_State* L);
    static int loaded(lua_State* L);
    static int groups(lua_State* L);
    static int cachedCount(lua_State* L);
    static int pendingCount(lua_State* L);
    static int releaseUnused(lua_State* L);
    static int setUploadBudget(lua_State* L);
    static int uploadBudget(lua_State* L);
    static int uploadCount(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::assets
