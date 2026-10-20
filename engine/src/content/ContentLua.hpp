#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/content/ContentManager.hpp"
#include "haylen/content/Error.hpp"

struct lua_State;

namespace haylen::lua {
class Promise;
}

namespace haylen::content {

// Installs `haylen.content`, which tells which content plays and which assets are on the device, brings assets down ahead of time without loading them, and checks, downloads and applies the updates of the content. Its promises reject with the message of the error and the table that describes it, and `content.error` gives the same table for the last failure of the manager.
class ContentLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kOptions{"owner"};

    [[nodiscard]] static ContentManager& getManager(lua_State* L);
    static void pushError(lua_State* L, const Error& error);
    static void reject(lua_State* L, const lua::Promise& promise, const Error& failure);
    static void pushProgress(lua_State* L, const ContentManager::Progress& progress);

    // Sets a field of the table on top of the stack to a count, which Lua keeps as an integer.
    static void setInteger(lua_State* L, const char* field, std::uint64_t value);

    // Reads the optional progress function at index and the options table after it, and returns the callback that calls the function on the frame thread for as long as its owner lives.
    [[nodiscard]] static ContentManager::ProgressCallback readProgress(lua_State* L, int index);

    static int isProtected(lua_State* L);
    static int generation(lua_State* L);
    static int channel(lua_State* L);
    static int status(lua_State* L);
    static int progress(lua_State* L);
    static int error(lua_State* L);
    static int update(lua_State* L);
    static int isLocal(lua_State* L);
    static int addPackFolder(lua_State* L);
    static int prefetch(lua_State* L);
    static int check(lua_State* L);
    static int download(lua_State* L);
    static int apply(lua_State* L);
    static int cancel(lua_State* L);
    static int stats(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::content
