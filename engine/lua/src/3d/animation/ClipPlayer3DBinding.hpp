#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/3d/animation/ClipPlayer3D.hpp"
#include "haylen/lua/Type.hpp"
#include "runtime/Owners.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation3d::ClipPlayer3D> {
    static constexpr const char* name = "haylen.ClipPlayer3D";
    using Storage = std::shared_ptr<haylen::animation3d::ClipPlayer3D>;
};

} // namespace haylen::lua

namespace haylen::lua::animation3d {

// Installs the `ClipPlayer3D` class of `haylen.animation3d`, which plays one clip with its time, speed and loop mode, reads its values into a float buffer or one by one, and reports the events it passes and the end of a clip that plays once to listeners with owners.
class ClipPlayer3DBinding final {
  public:
    static void install(lua_State* L);

    // Creates a player with `newClipPlayer(clip[, {loop, speed, paused, time}])`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 4> kFields{"loop", "speed", "paused", "time"};

    // The thread whose update the listeners run on, so an error of a listener reaches the caller of `update`.
    inline static lua_State* updating = nullptr;

    [[nodiscard]] static haylen::animation3d::ClipPlayer3D& check(lua_State* L);

    static int getClip(lua_State* L);
    static int setClip(lua_State* L);
    static int getTime(lua_State* L);
    static int setTime(lua_State* L);
    static int getSpeed(lua_State* L);
    static int setSpeed(lua_State* L);
    static int getPaused(lua_State* L);
    static int setPaused(lua_State* L);
    static int getLoop(lua_State* L);
    static int setLoop(lua_State* L);
    static int finished(lua_State* L);
    static int update(lua_State* L);
    static int readValues(lua_State* L);
    static int value(lua_State* L);
    static int on(lua_State* L);

    // Calls a listener on the thread that updates the player, or reports its error on the main thread when nothing updates it.
    static void notify(const lua::Owners::Function& function, lua_State* main, const haylen::animation3d::Clip3D::Event* event);
    static void call(const lua::Owners::Function& function, lua_State* state, const haylen::animation3d::Clip3D::Event* event, bool reported);
};

} // namespace haylen::lua::animation3d
