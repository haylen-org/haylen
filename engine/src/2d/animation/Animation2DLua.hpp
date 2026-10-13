#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "haylen/2d/animation/SpriteFrame.hpp"
#include "haylen/2d/graphics/Sprite.hpp"

struct lua_State;

namespace haylen::animation2d {

// Installs `haylen.animation2d` with the `Animation`, `Animator` and `SpriteAtlas` classes. Frames count from one in Lua.
class Animation2DLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 8> kGridFields{"frameWidth", "frameHeight", "cells", "framesPerSecond", "loop", "margin", "spacing", "ticksPerFrame"};
    static constexpr std::array<std::string_view, 2> kTimingFields{"framesPerSecond", "loop"};
    static constexpr std::array<std::string_view, 7> kFrameFields{"source", "pivot", "duration", "ticks", "boxes", "points", "events"};
    static constexpr std::array<std::string_view, 5> kBoxFields{"name", "x", "y", "width", "height"};
    static constexpr std::array<std::string_view, 3> kPointFields{"name", "x", "y"};

    [[nodiscard]] static float readFramesPerSecond(lua_State* L, int index);

    // Reads a frame of `fromFrames`, a source rectangle or a table with its source and its data.
    [[nodiscard]] static SpriteFrame readFrame(lua_State* L, int index, float duration);
    static void pushFrame(lua_State* L, const SpriteFrame& frame);

    // Returns the sprite at the index, or a copy of the sprite a node shows.
    [[nodiscard]] static graphics2d::Sprite readSprite(lua_State* L, int index);
    static int fromGrid(lua_State* L);
    static int fromFrames(lua_State* L);

    static int animationDuration(lua_State* L);
    static int animationCycleDuration(lua_State* L);
    static int animationFrameCount(lua_State* L);
    static int animationTexture(lua_State* L);
    static int animationFrame(lua_State* L);
    static int animationFrameAt(lua_State* L);
    static int animationFrameData(lua_State* L);
    static int animationAddEvent(lua_State* L);
    static int animationCountsTicks(lua_State* L);

    static int newAnimator(lua_State* L);
    static int animatorAdd(lua_State* L);
    static int animatorPlay(lua_State* L);
    static int animatorQueue(lua_State* L);
    static int animatorClearQueue(lua_State* L);
    static int animatorQueuedCount(lua_State* L);
    static int animatorStop(lua_State* L);
    static int animatorHas(lua_State* L);
    static int animatorAnimation(lua_State* L);
    static void callCallback(lua_State* L, const char* name, std::string_view animation, std::optional<std::size_t> frame);
    static void callEvent(lua_State* L, std::string_view animation, std::string_view event, std::size_t frame);

    // Advances the animator at the index with its Lua callbacks in place.
    template <typename Advance> static void advance(lua_State* L, Advance&& body);
    static int animatorUpdate(lua_State* L);
    static int animatorStep(lua_State* L);
    static int animatorBox(lua_State* L);
    static int animatorPoint(lua_State* L);
    static int animatorApply(lua_State* L);
    static int animatorCurrent(lua_State* L);
    static int animatorFrame(lua_State* L);
    static int animatorTime(lua_State* L);
    static int animatorPlaying(lua_State* L);
    static int animatorFinished(lua_State* L);
    static int getCallback(lua_State* L, const char* name);
    static int setCallback(lua_State* L, const char* name);
    static int animatorGetOnFrame(lua_State* L);
    static int animatorSetOnFrame(lua_State* L);
    static int animatorGetOnFinish(lua_State* L);
    static int animatorSetOnFinish(lua_State* L);
    static int animatorGetOnEvent(lua_State* L);
    static int animatorSetOnEvent(lua_State* L);

    static int atlasTexture(lua_State* L);
    static int atlasFrameNames(lua_State* L);
    static int atlasAnimationNames(lua_State* L);
    static int atlasSliceNames(lua_State* L);
    static int atlasHasFrame(lua_State* L);
    static int atlasHasAnimation(lua_State* L);
    static int atlasHasSlice(lua_State* L);
    static int atlasFrame(lua_State* L);
    static int atlasAnimation(lua_State* L);
    static int atlasSlice(lua_State* L);
    static int atlasSource(lua_State* L);
    static int atlasApply(lua_State* L);
    static int atlasAnimationFromFrames(lua_State* L);

    static int open(lua_State* L);
};

} // namespace haylen::animation2d
