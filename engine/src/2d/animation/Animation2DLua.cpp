#include "2d/animation/Animation2DLua.hpp"

#include <memory>
#include <string>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::animation2d {

float Animation2DLua::readFramesPerSecond(lua_State* L, int index) {
    float framesPerSecond = 10.0F;
    lua::Table::readField(L, index, "framesPerSecond", framesPerSecond);
    luaL_argcheck(L, framesPerSecond > 0.0F, index, "framesPerSecond must be positive");
    return framesPerSecond;
}

// Cuts a grid animation with `fromGrid(texture, {frameWidth, frameHeight, cells = {1, 2, 3}, framesPerSecond, loop, margin = {x, y}, spacing = {x, y}})`. Cells count from one, left to right and top to bottom.
int Animation2DLua::fromGrid(lua_State* L) {
    graphics::Texture texture = lua::Stack::read<graphics::Texture>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kGridFields});

    Animation::GridOptions options;
    lua::Table::readField(L, 2, "frameWidth", options.frameSize.x);
    lua::Table::readField(L, 2, "frameHeight", options.frameSize.y);
    lua::Table::readField(L, 2, "loop", options.loop);
    lua::Table::readField(L, 2, "margin", options.margin);
    lua::Table::readField(L, 2, "spacing", options.spacing);
    options.framesPerSecond = readFramesPerSecond(L, 2);
    std::vector<int> cells;
    lua::Table::readField(L, 2, "cells", cells);
    for (const int cell : cells) {
        options.cells.push_back(cell - 1);
    }
    lua::Userdata::emplace<Animation>(L, Animation::fromGrid(std::move(texture), options));
    return 1;
}

// Builds an animation from source rectangles with `fromFrames(texture, {rect, ...}, {framesPerSecond, loop})`.
int Animation2DLua::fromFrames(lua_State* L) {
    Animation animation{.texture = lua::Stack::read<graphics::Texture>(L, 1)};
    const std::vector<math::Rect> sources = lua::Stack::read<std::vector<math::Rect>>(L, 2);
    float fps = 10.0F;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kTimingFields});
        fps = readFramesPerSecond(L, 3);
        lua::Table::readField(L, 3, "loop", animation.loop);
    }
    luaL_argcheck(L, !sources.empty(), 2, "expected at least one frame");
    for (const math::Rect& source : sources) {
        animation.frames.push_back({.source = source, .originalSize = source.getSize(), .duration = 1.0F / fps});
    }
    lua::Userdata::emplace<Animation>(L, std::move(animation));
    return 1;
}

int Animation2DLua::animationDuration(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animation>(L, 1).getDuration());
    return 1;
}

int Animation2DLua::animationCycleDuration(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animation>(L, 1).getCycleDuration());
    return 1;
}

int Animation2DLua::animationFrameCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animation>(L, 1).frames.size());
    return 1;
}

int Animation2DLua::animationTexture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animation>(L, 1).texture);
    return 1;
}

int Animation2DLua::animationFrame(lua_State* L) {
    const Animation& animation = lua::Userdata::check<Animation>(L, 1);
    const lua_Integer index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, index >= 1 && static_cast<std::size_t>(index) <= animation.frames.size(), 2, "frame index out of range");
    lua::Stack::push(L, animation.frames[static_cast<std::size_t>(index - 1)].source);
    return 1;
}

int Animation2DLua::animationFrameAt(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animation>(L, 1).frameAt(lua::Stack::read<float>(L, 2)) + 1);
    return 1;
}

int Animation2DLua::newAnimator(lua_State* L) {
    lua::Userdata::emplace<Animator>(L, std::make_shared<Animator>());
    return 1;
}

int Animation2DLua::animatorAdd(lua_State* L) {
    lua::Userdata::check<Animator>(L, 1).add(lua::Stack::read<std::string>(L, 2), lua::Stack::read<Animation>(L, 3));
    return 0;
}

int Animation2DLua::animatorPlay(lua_State* L) {
    lua::Userdata::check<Animator>(L, 1).play(lua::Stack::read<std::string_view>(L, 2), lua_toboolean(L, 3) != 0);
    return 0;
}

int Animation2DLua::animatorQueue(lua_State* L) {
    lua::Userdata::check<Animator>(L, 1).queue(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

int Animation2DLua::animatorClearQueue(lua_State* L) {
    lua::Userdata::check<Animator>(L, 1).clearQueue();
    return 0;
}

int Animation2DLua::animatorQueuedCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animator>(L, 1).getQueuedCount());
    return 1;
}

int Animation2DLua::animatorStop(lua_State* L) {
    lua::Userdata::check<Animator>(L, 1).stop();
    return 0;
}

int Animation2DLua::animatorHas(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animator>(L, 1).has(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Animation2DLua::animatorAnimation(lua_State* L) {
    lua::Userdata::emplace<Animation>(L, lua::Userdata::check<Animator>(L, 1).getAnimation(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

void Animation2DLua::callCallback(lua_State* L, const char* name, std::string_view animation, std::optional<std::size_t> frame) {
    if (!lua::Userdata::pushFunction(L, 1, name)) {
        return;
    }
    lua::Stack::push(L, animation);
    if (frame) {
        lua::Stack::push(L, *frame + 1);
    }
    lua_call(L, frame ? 2 : 1, 0);
}

// Callbacks live in the user value of the animator, so a callback that captures its own animator never keeps it alive forever.
int Animation2DLua::animatorUpdate(lua_State* L) {
    Animator& animator = lua::Userdata::check<Animator>(L, 1);
    const float delta = lua::Stack::read<float>(L, 2);

    struct CallbackScope {
        Animator& animator;
        ~CallbackScope() {
            animator.onFrame = {};
            animator.onFinish = {};
        }
    } scope{animator};

    animator.onFrame = [L](std::string_view name, std::size_t frame) { callCallback(L, "onFrame", name, frame); };
    animator.onFinish = [L](std::string_view name) { callCallback(L, "onFinish", name, std::nullopt); };
    animator.update(delta);
    return 0;
}

int Animation2DLua::animatorApply(lua_State* L) {
    lua::Userdata::check<Animator>(L, 1).apply(lua::Userdata::check<graphics2d::Sprite>(L, 2));
    return 0;
}

int Animation2DLua::animatorCurrent(lua_State* L) {
    const std::string& current = lua::Userdata::check<Animator>(L, 1).getCurrent();
    if (current.empty()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, current);
    return 1;
}

int Animation2DLua::animatorFrame(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animator>(L, 1).getFrame() + 1);
    return 1;
}

int Animation2DLua::animatorTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animator>(L, 1).getTime());
    return 1;
}

int Animation2DLua::animatorPlaying(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animator>(L, 1).isPlaying());
    return 1;
}

int Animation2DLua::animatorFinished(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Animator>(L, 1).isFinished());
    return 1;
}

int Animation2DLua::getCallback(lua_State* L, const char* name) {
    (void)lua::Userdata::check<Animator>(L, 1);
    lua::Userdata::pushField(L, 1, name);
    return 1;
}

int Animation2DLua::setCallback(lua_State* L, const char* name) {
    (void)lua::Userdata::check<Animator>(L, 1);
    if (!lua_isnil(L, 3)) {
        luaL_checktype(L, 3, LUA_TFUNCTION);
    }
    lua::Userdata::setField(L, 1, name, 3);
    return 0;
}

int Animation2DLua::animatorGetOnFrame(lua_State* L) {
    return getCallback(L, "onFrame");
}

int Animation2DLua::animatorSetOnFrame(lua_State* L) {
    return setCallback(L, "onFrame");
}

int Animation2DLua::animatorGetOnFinish(lua_State* L) {
    return getCallback(L, "onFinish");
}

int Animation2DLua::animatorSetOnFinish(lua_State* L) {
    return setCallback(L, "onFinish");
}

int Animation2DLua::atlasTexture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).getTexture());
    return 1;
}

int Animation2DLua::atlasFrameNames(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).getFrameNames());
    return 1;
}

int Animation2DLua::atlasAnimationNames(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).getAnimationNames());
    return 1;
}

int Animation2DLua::atlasSliceNames(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).getSliceNames());
    return 1;
}

int Animation2DLua::atlasHasFrame(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).hasFrame(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Animation2DLua::atlasHasAnimation(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).hasAnimation(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Animation2DLua::atlasHasSlice(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).hasSlice(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

// Describes a frame with `frame(name)` as a table with its source, trim offset, original size and duration.
int Animation2DLua::atlasFrame(lua_State* L) {
    const SpriteFrame& frame = lua::Userdata::check<SpriteAtlas>(L, 1).getFrame(lua::Stack::read<std::string_view>(L, 2));
    lua_createtable(L, 0, 4);
    lua::Stack::push(L, frame.source);
    lua_setfield(L, -2, "source");
    lua::Stack::push(L, frame.offset);
    lua_setfield(L, -2, "offset");
    lua::Stack::push(L, frame.originalSize);
    lua_setfield(L, -2, "originalSize");
    lua::Stack::push(L, frame.duration);
    lua_setfield(L, -2, "duration");
    return 1;
}

int Animation2DLua::atlasAnimation(lua_State* L) {
    lua::Userdata::emplace<Animation>(L, lua::Userdata::check<SpriteAtlas>(L, 1).getAnimation(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Animation2DLua::atlasSlice(lua_State* L) {
    lua::Userdata::emplace<graphics2d::NineSlice>(L, lua::Userdata::check<SpriteAtlas>(L, 1).getSlice(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int Animation2DLua::atlasSource(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteAtlas>(L, 1).getFrame(lua::Stack::read<std::string_view>(L, 2)).source);
    return 1;
}

// Shows one frame on a sprite with `apply(sprite, name)`, keeping the sprite pivot at the center of the untrimmed frame.
int Animation2DLua::atlasApply(lua_State* L) {
    const SpriteAtlas& atlas = lua::Userdata::check<SpriteAtlas>(L, 1);
    graphics2d::Sprite& sprite = lua::Userdata::check<graphics2d::Sprite>(L, 2);
    const SpriteFrame& frame = atlas.getFrame(lua::Stack::read<std::string_view>(L, 3));
    sprite.texture = atlas.getTexture();
    sprite.source = frame.source;
    sprite.size = frame.source.getSize();
    sprite.pivot = (math::Vec2{0.5F, 0.5F} * frame.originalSize - frame.offset) / frame.source.getSize();
    return 0;
}

int Animation2DLua::atlasAnimationFromFrames(lua_State* L) {
    const SpriteAtlas& atlas = lua::Userdata::check<SpriteAtlas>(L, 1);
    const std::vector<std::string> names = lua::Stack::read<std::vector<std::string>>(L, 2);
    float fps = 10.0F;
    Animation::Loop loop = Animation::Loop::Loop;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kTimingFields});
        fps = readFramesPerSecond(L, 3);
        lua::Table::readField(L, 3, "loop", loop);
    }
    lua::Userdata::emplace<Animation>(L, atlas.animationFromFrames(names, fps, loop));
    return 1;
}

int Animation2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"fromGrid", &lua::Binding::native<&fromGrid>},
        {"fromFrames", &lua::Binding::native<&fromFrames>},
        {"newAnimator", &newAnimator},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Animation2DLua::install(lua_State* L) {
    lua::ClassBuilder<Animation>(L).property("duration", &animationDuration).property("cycleDuration", &animationCycleDuration).property("frameCount", &animationFrameCount).property("texture", &animationTexture).field<&Animation::loop>("loop").function("frame", &animationFrame).function("frameAt", &animationFrameAt).install();

    lua::ClassBuilder<Animator>(L).function("add", &lua::Binding::native<&animatorAdd>).function("play", &lua::Binding::native<&animatorPlay>).function("queue", &lua::Binding::native<&animatorQueue>).function("clearQueue", &lua::Binding::native<&animatorClearQueue>).property("queuedCount", &animatorQueuedCount).function("stop", &animatorStop).function("has", &animatorHas).function("animation", &lua::Binding::native<&animatorAnimation>).function("update", &lua::Binding::native<&animatorUpdate>).function("apply", &lua::Binding::native<&animatorApply>).property("current", &animatorCurrent).property("frame", &animatorFrame).property("time", &animatorTime).property("playing", &animatorPlaying).property("finished", &animatorFinished).field<&Animator::speed>("speed").nestedField<&Animator::pivot, &math::Vec2::x>("pivotX").nestedField<&Animator::pivot, &math::Vec2::y>("pivotY").property("onFrame", &animatorGetOnFrame, &animatorSetOnFrame).property("onFinish", &animatorGetOnFinish, &animatorSetOnFinish).install();

    lua::ClassBuilder<SpriteAtlas>(L).property("texture", &atlasTexture).function("hasFrame", &atlasHasFrame).function("hasAnimation", &atlasHasAnimation).function("hasSlice", &atlasHasSlice).function("frame", &lua::Binding::native<&atlasFrame>).function("frameNames", &atlasFrameNames).function("animationNames", &atlasAnimationNames).function("sliceNames", &atlasSliceNames).function("animation", &lua::Binding::native<&atlasAnimation>).function("slice", &lua::Binding::native<&atlasSlice>).function("source", &lua::Binding::native<&atlasSource>).function("apply", &lua::Binding::native<&atlasApply>).function("animationFromFrames", &lua::Binding::native<&atlasAnimationFromFrames>).meta("__eq", &lua::Userdata::equal<SpriteAtlas>).install();

    lua::Binding::preload(L, "haylen.animation2d", &open);
}

} // namespace haylen::animation2d
