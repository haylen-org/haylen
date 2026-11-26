#pragma once

#include <array>
#include <memory>
#include <span>
#include <string_view>

#include "haylen/3d/animation/Track3D.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua::animation3d {

// A track that Lua holds: a track of its own, or a track of a clip, which it keeps alive.
struct ScriptedTrack3D {
    std::shared_ptr<const haylen::animation3d::Track3D> track;
};

} // namespace haylen::lua::animation3d

namespace haylen::lua {

template <> struct Type<lua::animation3d::ScriptedTrack3D> {
    static constexpr const char* name = "haylen.Track3D";
    using Storage = lua::animation3d::ScriptedTrack3D;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `Track3D` class of `haylen.animation3d`, the keys of one property of one target, with its keys read as float buffers and its value at a time.
class Track3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates a track with `newTrack({target, property, type, interpolation, times, values, handles, strings})`.
    static int create(lua_State* L);

    [[nodiscard]] static const haylen::animation3d::Track3D& check(lua_State* L, int index);
    static void push(lua_State* L, std::shared_ptr<const haylen::animation3d::Track3D> track);

    // Pushes the value of a track that a sample wrote: a number, a boolean, the string of a key, a vector, a quaternion, a color or a list of weights.
    static void pushValue(lua_State* L, const haylen::animation3d::Track3D& track, std::span<const float> value);

  private:
    static constexpr std::array<std::string_view, 8> kFields{"target", "property", "type", "interpolation", "times", "values", "handles", "strings"};

    // Pushes the numbers as a new float buffer.
    static void pushBuffer(lua_State* L, std::span<const float> values);

    static int target(lua_State* L);
    static int property(lua_State* L);
    static int type(lua_State* L);
    static int interpolation(lua_State* L);
    static int storage(lua_State* L);
    static int keyCount(lua_State* L);
    static int componentCount(lua_State* L);
    static int startTime(lua_State* L);
    static int endTime(lua_State* L);
    static int quantizationError(lua_State* L);
    static int memorySize(lua_State* L);
    static int times(lua_State* L);
    static int values(lua_State* L);
    static int handles(lua_State* L);
    static int strings(lua_State* L);
    static int sample(lua_State* L);
};

} // namespace haylen::lua::animation3d
