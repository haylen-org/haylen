#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/animation/Skeleton.hpp"
#include "haylen/2d/animation/SkeletonData.hpp"
#include "haylen/lua/Type.hpp"
#include "runtime/Owners.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<haylen::animation2d::SkeletonData> {
    static constexpr const char* name = "haylen.SkeletonData";
    using Storage = std::shared_ptr<haylen::animation2d::SkeletonData>;
};

template <> struct Type<haylen::animation2d::Skeleton> {
    static constexpr const char* name = "haylen.Skeleton2D";
    using Storage = std::shared_ptr<haylen::animation2d::Skeleton>;
};

} // namespace haylen::lua

namespace haylen::lua::animation2d {

// Installs the `SkeletonData` and `Skeleton2D` classes of `haylen.animation2d`.
class SkeletonBinding final {
  public:
    static void install(lua_State* L);

    // Pushes loaded skeleton data, which the asset loader of `.skeleton` files hands to Lua.
    static void pushData(lua_State* L, std::shared_ptr<haylen::animation2d::SkeletonData> data);

    // Creates data with `animation2d.newSkeletonData(description, atlas)` and a skeleton with `animation2d.newSkeleton(data, options)`.
    static int newData(lua_State* L);
    static int newSkeleton(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 7> kSkeletonFields{"skin", "x", "y", "rotation", "scaleX", "scaleY", "color"};
    static constexpr std::array<std::string_view, 3> kPlayFields{"track", "loop", "fade"};

    // The state of the Lua call that updates a skeleton, where its event listeners run.
    inline static lua_State* updating = nullptr;

    [[nodiscard]] static haylen::animation2d::Skeleton& check(lua_State* L);
    [[nodiscard]] static haylen::animation2d::Skeleton::PlayOptions readPlayOptions(lua_State* L, int index);
    [[nodiscard]] static std::size_t readTrack(lua_State* L, int index);
    static void notifyEvent(const lua::Owners::Function& function, lua_State* main, const haylen::animation2d::Skeleton::Event& fired);
    static void callEvent(const lua::Owners::Function& function, lua_State* state, const haylen::animation2d::Skeleton::Event& fired, bool reported);

    static int dataNames(lua_State* L, int kind);
    static int dataBones(lua_State* L);
    static int dataSlots(lua_State* L);
    static int dataSkins(lua_State* L);
    static int dataAnimations(lua_State* L);
    static int dataEvents(lua_State* L);
    static int dataDuration(lua_State* L);

    static int play(lua_State* L);
    static int crossFade(lua_State* L);
    static int stop(lua_State* L);
    static int setTrackWeight(lua_State* L);
    static int trackAnimation(lua_State* L);
    static int trackTime(lua_State* L);
    static int getSkin(lua_State* L);
    static int setSkin(lua_State* L);
    static int setAttachment(lua_State* L);
    static int bone(lua_State* L);
    static int setIkTarget(lua_State* L);
    static int clearIkTarget(lua_State* L);
    static int setIkMix(lua_State* L);
    static int update(lua_State* L);
    static int pose(lua_State* L);
    static int point(lua_State* L);
    static int bounds(lua_State* L);
    static int draw(lua_State* L);
    static int on(lua_State* L);
    static int data(lua_State* L);
};

} // namespace haylen::lua::animation2d
