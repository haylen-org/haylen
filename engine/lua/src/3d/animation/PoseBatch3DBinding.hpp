#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>

#include "haylen/3d/animation/PoseBatch3D.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation3d::PoseBatch3D> {
    static constexpr const char* name = "haylen.PoseBatch3D";
    using Storage = std::shared_ptr<haylen::animation3d::PoseBatch3D>;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `PoseBatch3D` class of `haylen.animation3d`: a crowd of characters of one skin that loop clips at their own times and speeds, updated on the job system in one call, with the palettes of all of them read into one float buffer.
class PoseBatch3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates a batch with `newPoseBatch(skin)`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kFields{"time", "speed"};

    [[nodiscard]] static haylen::animation3d::PoseBatch3D& check(lua_State* L);

    // Reads the character at the stack index, counted from 1.
    [[nodiscard]] static std::size_t readCharacter(lua_State* L, int index);

    static int add(lua_State* L);
    static int remove(lua_State* L);
    static int setClip(lua_State* L);
    static int setTime(lua_State* L);
    static int setSpeed(lua_State* L);
    static int time(lua_State* L);
    static int setTimes(lua_State* L);
    static int update(lua_State* L);
    static int readPalettes(lua_State* L);
    static int count(lua_State* L);
    static int paletteSize(lua_State* L);
};

} // namespace haylen::lua::animation3d
