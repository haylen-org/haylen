#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>

#include "haylen/2d/animation/AnimatedBatch.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<animation2d::AnimatedBatch> {
    static constexpr const char* name = "haylen.AnimatedBatch";
    using Storage = std::shared_ptr<animation2d::AnimatedBatch>;
};

} // namespace haylen::lua

namespace haylen::animation2d {

// Installs the `AnimatedBatch` class of `haylen.animation2d`, whose sprites are numbered from one.
class AnimatedBatchLua final {
  public:
    static void install(lua_State* L);

    // Creates a batch with `animation2d.newAnimatedBatch(texture, animations)`, where `animations` maps names to animations.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kAnimationFields{"animation", "speed", "scaleX", "scaleY", "partColors"};
    static constexpr std::array<std::string_view, 1> kListenerFields{"owner"};

    // The state of the Lua call that advances a batch, where its complete listeners run.
    inline static lua_State* advancing = nullptr;

    [[nodiscard]] static AnimatedBatch& check(lua_State* L);
    [[nodiscard]] static std::size_t checkIndex(lua_State* L, int index, const AnimatedBatch& batch);

    // Applies the animation keys of a sprite table to the sprite at the index.
    static void readAnimationFields(lua_State* L, int table, AnimatedBatch& batch, std::size_t index);
    template <typename Advance> static void advance(lua_State* L, Advance&& body);

    static int add(lua_State* L);
    static int set(lua_State* L);
    static int get(lua_State* L);
    static int play(lua_State* L);
    static int animationOf(lua_State* L);
    static int frameOf(lua_State* L);
    static int remove(lua_State* L);
    static int clear(lua_State* L);
    static int reserve(lua_State* L);
    static int size(lua_State* L);
    static int animationNames(lua_State* L);
    static int writeFields(lua_State* L);
    static int readFields(lua_State* L);
    static int update(lua_State* L);
    static int step(lua_State* L);
    static int finished(lua_State* L);
    static int draw(lua_State* L);
    static int on(lua_State* L);
    static int texture(lua_State* L);
};

} // namespace haylen::animation2d
