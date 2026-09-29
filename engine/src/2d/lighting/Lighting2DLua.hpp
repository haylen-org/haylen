#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::lighting2d {

// Installs haylen.lighting2d with the Light and Occluder classes, the helpers that build occluders from physics bodies and Tiled maps, the illuminate query, the falloff curve of lights without a texture and the flicker function that animates light intensities.
class Lighting2DLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kFlickerFields{"speed", "amount", "seed"};

    static int flicker(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lighting2d
