#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation3d::Clip3D> {
    static constexpr const char* name = "haylen.Clip3D";
    using Storage = std::shared_ptr<haylen::animation3d::Clip3D>;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `Clip3D` class of `haylen.animation3d` and the functions that make clips: from tracks and events, from the animations of a glTF document and from a sequence of morph targets. A clip samples every track into one float buffer, and its edits return new clips.
class Clip3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates a clip with `newClip({name, tracks, events, duration})`.
    static int create(lua_State* L);

    // Makes clips with `clipFromGltf(document, animation)`, `clipsFromGltf(document)` and `clipFromMorphSequence(target, targetCount[, options])`.
    static int fromGltf(lua_State* L);
    static int allFromGltf(lua_State* L);
    static int fromMorphSequence(lua_State* L);

    static void push(lua_State* L, std::shared_ptr<const haylen::animation3d::Clip3D> clip);

  private:
    static constexpr std::array<std::string_view, 4> kFields{"name", "tracks", "events", "duration"};
    static constexpr std::array<std::string_view, 3> kEventFields{"time", "name", "data"};
    static constexpr std::array<std::string_view, 3> kSequenceFields{"name", "framesPerSecond", "loop"};
    static constexpr std::array<std::string_view, 1> kAdditiveFields{"time"};

    // The cursors of a sample from Lua, which every sample reuses.
    inline static std::vector<std::size_t> cursors;

    [[nodiscard]] static const std::shared_ptr<haylen::animation3d::Clip3D>& check(lua_State* L, int index);
    [[nodiscard]] static std::vector<haylen::animation3d::Clip3D::Event> readEvents(lua_State* L, int index);

    static int name(lua_State* L);
    static int duration(lua_State* L);
    static int valueCount(lua_State* L);
    static int memorySize(lua_State* L);
    static int tracks(lua_State* L);
    static int events(lua_State* L);
    static int track(lua_State* L);
    static int offset(lua_State* L);
    static int sample(lua_State* L);
    static int renamed(lua_State* L);
    static int trimmed(lua_State* L);
    static int joined(lua_State* L);
    static int resampled(lua_State* L);
    static int optimized(lua_State* L);
    static int compressed(lua_State* L);
    static int toAdditive(lua_State* L);
};

} // namespace haylen::lua::animation3d
