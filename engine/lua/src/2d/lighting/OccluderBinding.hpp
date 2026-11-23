#pragma once

#include <lua.hpp>

#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

template <> struct Type<haylen::lighting2d::Occluder> {
    static constexpr const char* name = "haylen.Occluder";
    using Storage = haylen::lighting2d::Occluder;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::lighting2d::Occluder::Cull> {
    static std::optional<haylen::lighting2d::Occluder::Cull> fromName(std::string_view name) {
        return haylen::lighting2d::Occluder::cullFromName(name);
    }
    static std::string_view name(haylen::lighting2d::Occluder::Cull value) {
        return haylen::lighting2d::Occluder::cullName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::lighting2d {

// Installs the `Occluder` class of `haylen.lighting2d`, whose properties write straight into the occluder that `graphics2d.drawOccluder` draws.
class OccluderBinding final {
  public:
    static void install(lua_State* L);

    // Reads an `Occluder`, or a table with its properties over the defaults, at index into `occluder`, whose points keep their storage, so an occluder read every frame allocates nothing once it has grown.
    static void read(lua_State* L, int index, haylen::lighting2d::Occluder& occluder);

    // Creates an occluder with `newOccluder({points = {...}, closed = true, cull = 'disabled', mask = 1, x, y, rotation, scaleX, scaleY})`.
    static int newOccluder(lua_State* L);

    // Returns a list of occluders built from the shapes of a physics body with `occludersFromBody(world, body)`.
    static int fromBody(lua_State* L);

    // Returns a list of occluders built from the objects of a Tiled map with `occludersFromMap(map, layer)`.
    static int fromMap(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 10> kFields{"points", "closed", "cull", "mask", "x", "y", "position", "rotation", "scaleX", "scaleY"};

    // Points come as a list of vectors or as a flat list of numbers, two per point.
    static void readPoints(lua_State* L, int index, std::vector<haylen::math::Vec2>& points);
    static void pushList(lua_State* L, std::vector<haylen::lighting2d::Occluder> occluders);

    static int getPoints(lua_State* L);
    static int setPoints(lua_State* L);
    static int worldPoints(lua_State* L);
};

} // namespace haylen::lua::lighting2d
