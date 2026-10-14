#include "2d/animation/AnimatedBatchLua.hpp"

#include <string>
#include <utility>
#include <vector>

#include "2d/graphics/SpriteBatchLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Owners.hpp"

namespace haylen::animation2d {

AnimatedBatch& AnimatedBatchLua::check(lua_State* L) {
    return lua::Userdata::check<AnimatedBatch>(L, 1);
}

std::size_t AnimatedBatchLua::checkIndex(lua_State* L, int index, const AnimatedBatch& batch) {
    const lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= 1 && static_cast<std::size_t>(value) <= batch.size(), index, "sprite index out of range");
    return static_cast<std::size_t>(value - 1);
}

int AnimatedBatchLua::create(lua_State* L) {
    auto batch = std::make_shared<AnimatedBatch>(lua::Stack::read<graphics::Texture>(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        batch->addAnimation(lua::Stack::read<std::string>(L, -2), lua::Stack::read<Animation>(L, -1));
        lua_pop(L, 1);
    }
    lua::Userdata::emplace<AnimatedBatch>(L, std::move(batch));
    return 1;
}

void AnimatedBatchLua::readAnimationFields(lua_State* L, int table, AnimatedBatch& batch, std::size_t index) {
    if (lua_getfield(L, table, "animation") != LUA_TNIL) {
        batch.play(index, lua::Stack::read<std::string_view>(L, -1));
    }
    lua_pop(L, 1);
    float speed = batch.getSpeed(index);
    lua::Table::readField(L, table, "speed", speed);
    batch.setSpeed(index, speed);
    math::Vec2 scale = batch.getScale(index);
    lua::Table::readField(L, table, "scaleX", scale.x);
    lua::Table::readField(L, table, "scaleY", scale.y);
    batch.setScale(index, scale);
    if (lua_getfield(L, table, "partColors") != LUA_TNIL) {
        batch.setPartColors(index, lua::Stack::read<graphics2d::PartColors>(L, -1));
    }
    lua_pop(L, 1);
}

// A sprite starts with the animation its table names, or with the first animation of the batch by name.
int AnimatedBatchLua::add(lua_State* L) {
    AnimatedBatch& batch = check(L);
    const graphics2d::SpriteInstance sprite = lua::TypeConverter::readSpriteInstance(L, 2, batch.getTexture(), {}, {kAnimationFields});
    std::string animation;
    lua::Table::readField(L, 2, "animation", animation);
    if (animation.empty()) {
        const std::vector<std::string> names = batch.getAnimationNames();
        if (names.empty()) {
            return luaL_error(L, "The animated batch has no animation to play.");
        }
        animation = names.front();
    }
    const std::size_t index = batch.add(sprite, animation);
    readAnimationFields(L, 2, batch, index);
    lua::Stack::push(L, index + 1);
    return 1;
}

int AnimatedBatchLua::set(lua_State* L) {
    AnimatedBatch& batch = check(L);
    const std::size_t index = checkIndex(L, 2, batch);
    batch.set(index, lua::TypeConverter::readSpriteInstance(L, 3, batch.getTexture(), batch.get(index), {kAnimationFields}));
    readAnimationFields(L, 3, batch, index);
    return 0;
}

int AnimatedBatchLua::get(lua_State* L) {
    const AnimatedBatch& batch = check(L);
    const std::size_t index = checkIndex(L, 2, batch);
    graphics2d::SpriteBatchLua::pushSpriteInstance(L, batch.get(index), batch.getPartColors(index), batch.getData(index));
    lua::Stack::push(L, batch.getAnimation(index));
    lua_setfield(L, -2, "animation");
    lua::Stack::push(L, batch.getSpeed(index));
    lua_setfield(L, -2, "speed");
    lua::Stack::push(L, batch.getScale(index).x);
    lua_setfield(L, -2, "scaleX");
    lua::Stack::push(L, batch.getScale(index).y);
    lua_setfield(L, -2, "scaleY");
    lua::Stack::push(L, batch.getFrame(index) + 1);
    lua_setfield(L, -2, "frame");
    lua::Stack::push(L, batch.getTime(index));
    lua_setfield(L, -2, "time");
    return 1;
}

int AnimatedBatchLua::play(lua_State* L) {
    AnimatedBatch& batch = check(L);
    batch.play(checkIndex(L, 2, batch), lua::Stack::read<std::string_view>(L, 3), lua_toboolean(L, 4) != 0);
    return 0;
}

int AnimatedBatchLua::animationOf(lua_State* L) {
    const AnimatedBatch& batch = check(L);
    lua::Stack::push(L, batch.getAnimation(checkIndex(L, 2, batch)));
    return 1;
}

int AnimatedBatchLua::frameOf(lua_State* L) {
    const AnimatedBatch& batch = check(L);
    lua::Stack::push(L, batch.getFrame(checkIndex(L, 2, batch)) + 1);
    return 1;
}

int AnimatedBatchLua::remove(lua_State* L) {
    AnimatedBatch& batch = check(L);
    batch.remove(checkIndex(L, 2, batch));
    return 0;
}

int AnimatedBatchLua::clear(lua_State* L) {
    check(L).clear();
    return 0;
}

int AnimatedBatchLua::reserve(lua_State* L) {
    check(L).reserve(lua::Stack::read<std::size_t>(L, 2));
    return 0;
}

int AnimatedBatchLua::size(lua_State* L) {
    lua::Stack::push(L, check(L).size());
    return 1;
}

int AnimatedBatchLua::animationNames(lua_State* L) {
    lua::Stack::push(L, check(L).getAnimationNames());
    return 1;
}

int AnimatedBatchLua::writeFields(lua_State* L) {
    AnimatedBatch& batch = check(L);
    const core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 2);
    const lua_Integer first = luaL_optinteger(L, 4, 1);
    luaL_argcheck(L, first >= 1, 4, "sprites are numbered from one");
    batch.writeFields(buffer.getValues(), graphics2d::SpriteBatchLua::readLayout(L, 3), static_cast<std::size_t>(first - 1));
    return 0;
}

int AnimatedBatchLua::readFields(lua_State* L) {
    const AnimatedBatch& batch = check(L);
    core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 2);
    const lua_Integer first = luaL_optinteger(L, 4, 1);
    luaL_argcheck(L, first >= 1, 4, "sprites are numbered from one");
    batch.readFields(buffer.getValues(), graphics2d::SpriteBatchLua::readLayout(L, 3), static_cast<std::size_t>(first - 1));
    return 0;
}

// The complete listeners of the batch run in the state of the call that advances it.
template <typename Advance> void AnimatedBatchLua::advance(lua_State* L, Advance&& body) {
    AnimatedBatch& batch = check(L);
    lua_State* previous = advancing;
    advancing = L;
    struct Restore {
        lua_State* previous;
        ~Restore() {
            advancing = previous;
        }
    } restore{previous};
    body(batch, lua::Runtime::getEngine(L).getJobs());
}

int AnimatedBatchLua::update(lua_State* L) {
    const float delta = lua::Stack::read<float>(L, 2);
    advance(L, [delta](AnimatedBatch& batch, core::JobSystem& jobs) { batch.update(delta, &jobs); });
    return 0;
}

int AnimatedBatchLua::step(lua_State* L) {
    const lua_Integer ticks = lua_isnoneornil(L, 2) ? 1 : luaL_checkinteger(L, 2);
    luaL_argcheck(L, ticks >= 0, 2, "ticks must not be negative");
    advance(L, [ticks](AnimatedBatch& batch, core::JobSystem& jobs) { batch.step(static_cast<std::uint32_t>(ticks), &jobs); });
    return 0;
}

int AnimatedBatchLua::finished(lua_State* L) {
    const std::span<const std::size_t> ended = check(L).getFinished();
    lua_createtable(L, static_cast<int>(ended.size()), 0);
    for (std::size_t place = 0; place < ended.size(); ++place) {
        lua_pushinteger(L, static_cast<lua_Integer>(ended[place] + 1));
        lua_rawseti(L, -2, static_cast<lua_Integer>(place + 1));
    }
    return 1;
}

int AnimatedBatchLua::draw(lua_State* L) {
    check(L).draw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int AnimatedBatchLua::texture(lua_State* L) {
    lua::Stack::push(L, check(L).getTexture());
    return 1;
}

// Listens with `on('complete', function, {owner = owner})`, called with the list of the sprites whose one-shot animation ended in an update. A listener ends with its owner, and without one it takes the default owner.
int AnimatedBatchLua::on(lua_State* L) {
    AnimatedBatch& batch = check(L);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "complete") {
        return luaL_error(L, "Unknown animated batch event \"%s\". Animated batches report \"complete\".", std::string(event).c_str());
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kListenerFields});
        if (lua_getfield(L, 4, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    const std::weak_ptr<const void> lifetime = owner != 0 ? lua::Owners::getLifetime(L, owner) : std::weak_ptr<const void>();
    auto function = std::make_shared<lua::Owners::Function>(L, 3, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection = batch.completed.connect([function, main](std::span<const std::size_t> ended) {
        const auto call = [&](lua_State* state, bool reported) {
            if (!function->push(state)) {
                return;
            }
            lua_createtable(state, static_cast<int>(ended.size()), 0);
            for (std::size_t place = 0; place < ended.size(); ++place) {
                lua_pushinteger(state, static_cast<lua_Integer>(ended[place] + 1));
                lua_rawseti(state, -2, static_cast<lua_Integer>(place + 1));
            }
            if (reported) {
                lua::Runtime::protectedCall(state, 1, 0);
            } else {
                lua_call(state, 1, 0);
            }
        };
        if (advancing != nullptr) {
            call(advancing, false);
            return;
        }
        lua::Runtime::runReporting(main, [&] { call(main, true); });
    }, {.owner = lifetime});
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

void AnimatedBatchLua::install(lua_State* L) {
    lua::ClassBuilder<AnimatedBatch>(L).function("add", &lua::Binding::native<&add>).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("play", &lua::Binding::native<&play>).function("animation", &lua::Binding::native<&animationOf>).function("frame", &lua::Binding::native<&frameOf>).function("remove", &lua::Binding::native<&remove>).function("clear", &clear).function("reserve", &lua::Binding::native<&reserve>).function("size", &size).function("animationNames", &lua::Binding::native<&animationNames>).function("writeFields", &lua::Binding::native<&writeFields>).function("readFields", &lua::Binding::native<&readFields>).function("update", &lua::Binding::native<&update>).function("step", &lua::Binding::native<&step>).function("finished", &lua::Binding::native<&finished>).function("draw", &lua::Binding::native<&draw>).function("on", &lua::Binding::native<&on>).property("texture", &texture).install();
}

} // namespace haylen::animation2d
