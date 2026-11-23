#include "2d/animation/AnimatedBatchBinding.hpp"

#include <string>
#include <utility>
#include <vector>

#include "2d/graphics/SpriteBatchBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/Animation2DPlugin.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::animation2d {

haylen::animation2d::AnimatedBatch& AnimatedBatchBinding::check(lua_State* L) {
    return lua::Userdata::check<haylen::animation2d::AnimatedBatch>(L, 1);
}

std::size_t AnimatedBatchBinding::checkIndex(lua_State* L, int index, const haylen::animation2d::AnimatedBatch& batch) {
    const lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= 1 && static_cast<std::size_t>(value) <= batch.size(), index, "sprite index out of range");
    return static_cast<std::size_t>(value - 1);
}

int AnimatedBatchBinding::create(lua_State* L) {
    auto batch = std::make_shared<haylen::animation2d::AnimatedBatch>(lua::Stack::read<haylen::graphics::Texture>(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 2) != 0) {
        batch->addAnimation(lua::Stack::read<std::string>(L, -2), lua::Stack::read<haylen::animation2d::Animation>(L, -1));
        lua_pop(L, 1);
    }

    lua::Runtime::getEngine(L).getPlugin<haylen::plugins::Animation2DPlugin>().track(batch);
    lua::Userdata::emplace<haylen::animation2d::AnimatedBatch>(L, std::move(batch));
    return 1;
}

void AnimatedBatchBinding::readAnimationFields(lua_State* L, int table, haylen::animation2d::AnimatedBatch& batch, std::size_t index) {
    if (lua_getfield(L, table, "animation") != LUA_TNIL) {
        batch.play(index, lua::Stack::read<std::string_view>(L, -1));
    }

    lua_pop(L, 1);
    float speed = batch.getSpeed(index);
    lua::Table::readField(L, table, "speed", speed);
    batch.setSpeed(index, speed);
    haylen::math::Vec2 scale = batch.getScale(index);
    lua::Table::readField(L, table, "scaleX", scale.x);
    lua::Table::readField(L, table, "scaleY", scale.y);
    batch.setScale(index, scale);
    if (lua_getfield(L, table, "partColors") != LUA_TNIL) {
        batch.setPartColors(index, lua::Stack::read<haylen::graphics2d::PartColors>(L, -1));
    }

    lua_pop(L, 1);
}

// A sprite starts with the animation its table names, or with the first animation of the batch by name.
int AnimatedBatchBinding::add(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    const haylen::graphics2d::SpriteInstance sprite = lua::TypeConverter::readSpriteInstance(L, 2, batch.getTexture(), {}, {kAnimationFields});
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

int AnimatedBatchBinding::set(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    const std::size_t index = checkIndex(L, 2, batch);
    batch.set(index, lua::TypeConverter::readSpriteInstance(L, 3, batch.getTexture(), batch.get(index), {kAnimationFields}));
    readAnimationFields(L, 3, batch, index);
    return 0;
}

int AnimatedBatchBinding::get(lua_State* L) {
    const haylen::animation2d::AnimatedBatch& batch = check(L);
    const std::size_t index = checkIndex(L, 2, batch);
    lua::graphics2d::SpriteBatchBinding::pushSpriteInstance(L, batch.get(index), batch.getPartColors(index), batch.getData(index));
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

int AnimatedBatchBinding::play(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    batch.play(checkIndex(L, 2, batch), lua::Stack::read<std::string_view>(L, 3), lua_toboolean(L, 4) != 0);
    return 0;
}

// Moves a sprite to a time of its animation with `batch:seek(index, time)` and shows its frame there.
int AnimatedBatchBinding::seek(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    batch.seek(checkIndex(L, 2, batch), lua::Stack::read<float>(L, 3));
    return 0;
}

int AnimatedBatchBinding::animationOf(lua_State* L) {
    const haylen::animation2d::AnimatedBatch& batch = check(L);
    lua::Stack::push(L, batch.getAnimation(checkIndex(L, 2, batch)));
    return 1;
}

int AnimatedBatchBinding::frameOf(lua_State* L) {
    const haylen::animation2d::AnimatedBatch& batch = check(L);
    lua::Stack::push(L, batch.getFrame(checkIndex(L, 2, batch)) + 1);
    return 1;
}

int AnimatedBatchBinding::remove(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    batch.remove(checkIndex(L, 2, batch));
    return 0;
}

int AnimatedBatchBinding::clear(lua_State* L) {
    check(L).clear();
    return 0;
}

int AnimatedBatchBinding::reserve(lua_State* L) {
    check(L).reserve(lua::Stack::read<std::size_t>(L, 2));
    return 0;
}

int AnimatedBatchBinding::size(lua_State* L) {
    lua::Stack::push(L, check(L).size());
    return 1;
}

int AnimatedBatchBinding::animationNames(lua_State* L) {
    lua::Stack::push(L, check(L).getAnimationNames());
    return 1;
}

int AnimatedBatchBinding::writeFields(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    const haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, 2);
    const lua_Integer first = luaL_optinteger(L, 4, 1);
    luaL_argcheck(L, first >= 1, 4, "sprites are numbered from one");
    batch.writeFields(buffer.getValues(), lua::graphics2d::SpriteBatchBinding::readLayout(L, 3), static_cast<std::size_t>(first - 1));
    return 0;
}

int AnimatedBatchBinding::readFields(lua_State* L) {
    const haylen::animation2d::AnimatedBatch& batch = check(L);
    haylen::core::FloatBuffer& buffer = lua::Userdata::check<haylen::core::FloatBuffer>(L, 2);
    const lua_Integer first = luaL_optinteger(L, 4, 1);
    luaL_argcheck(L, first >= 1, 4, "sprites are numbered from one");
    batch.readFields(buffer.getValues(), lua::graphics2d::SpriteBatchBinding::readLayout(L, 3), static_cast<std::size_t>(first - 1));
    return 0;
}

// The complete listeners of the batch run in the state of the call that advances it.
template <typename Advance> void AnimatedBatchBinding::advance(lua_State* L, Advance&& body) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
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

int AnimatedBatchBinding::update(lua_State* L) {
    const float delta = lua::Stack::read<float>(L, 2);
    advance(L, [delta](haylen::animation2d::AnimatedBatch& batch, haylen::core::JobSystem& jobs) { batch.update(delta, &jobs); });
    return 0;
}

int AnimatedBatchBinding::step(lua_State* L) {
    const lua_Integer ticks = lua_isnoneornil(L, 2) ? 1 : luaL_checkinteger(L, 2);
    luaL_argcheck(L, ticks >= 0, 2, "ticks must not be negative");
    advance(L, [ticks](haylen::animation2d::AnimatedBatch& batch, haylen::core::JobSystem& jobs) { batch.step(static_cast<std::uint32_t>(ticks), &jobs); });
    return 0;
}

int AnimatedBatchBinding::finished(lua_State* L) {
    const std::span<const std::size_t> ended = check(L).getFinished();
    lua_createtable(L, static_cast<int>(ended.size()), 0);
    for (std::size_t place = 0; place < ended.size(); ++place) {
        lua_pushinteger(L, static_cast<lua_Integer>(ended[place] + 1));
        lua_rawseti(L, -2, static_cast<lua_Integer>(place + 1));
    }

    return 1;
}

int AnimatedBatchBinding::draw(lua_State* L) {
    check(L).draw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int AnimatedBatchBinding::texture(lua_State* L) {
    lua::Stack::push(L, check(L).getTexture());
    return 1;
}

// A batch that ends sprites while Lua advances it calls the listener on that call, so an error reaches its caller, and otherwise the main thread reports the error.
void AnimatedBatchBinding::notifyEnded(const lua::Owners::Function& function, lua_State* main, std::span<const std::size_t> ended) {
    if (advancing != nullptr) {
        callEnded(function, advancing, ended, false);
        return;
    }

    lua::Runtime::runReporting(main, [&] { callEnded(function, main, ended, true); });
}

void AnimatedBatchBinding::callEnded(const lua::Owners::Function& function, lua_State* state, std::span<const std::size_t> ended, bool reported) {
    if (!function.push(state)) {
        return;
    }

    lua_createtable(state, static_cast<int>(ended.size()), 0);
    for (std::size_t place = 0; place < ended.size(); ++place) {
        lua_pushinteger(state, static_cast<lua_Integer>(ended[place] + 1));
        lua_rawseti(state, -2, static_cast<lua_Integer>(place + 1));
    }

    if (reported) {
        lua::Runtime::protectedCall(state, 1, 0);
        return;
    }

    lua_call(state, 1, 0);
}

// Listens with `on('complete', function, {owner = owner})`, called with the list of the sprites whose one-shot animation ended in an update. A listener ends with its owner, and without one it takes the default owner.
int AnimatedBatchBinding::on(lua_State* L) {
    haylen::animation2d::AnimatedBatch& batch = check(L);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "complete") {
        return luaL_error(L, "Unknown animated batch event \"%s\". Animated batches report \"complete\".", std::string(event).c_str());
    }

    const lua::Owners::Listener listener = lua::Owners::readListener(L, 3, 4);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    haylen::core::Connection connection = batch.completed.connect([function = listener.function, main](std::span<const std::size_t> ended) {
        notifyEnded(*function, main, ended);
    }, {.owner = listener.lifetime});
    // clang-format on
    lua::Owners::pushConnection(L, listener, std::move(connection));
    return 1;
}

void AnimatedBatchBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::animation2d::AnimatedBatch>(L).function("add", &lua::Binding::native<&add>).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("play", &lua::Binding::native<&play>).function("seek", &lua::Binding::native<&seek>).function("animation", &lua::Binding::native<&animationOf>).function("frame", &lua::Binding::native<&frameOf>).function("remove", &lua::Binding::native<&remove>).function("clear", &clear).function("reserve", &lua::Binding::native<&reserve>).function("size", &size).function("animationNames", &lua::Binding::native<&animationNames>).function("writeFields", &lua::Binding::native<&writeFields>).function("readFields", &lua::Binding::native<&readFields>).function("update", &lua::Binding::native<&update>).function("step", &lua::Binding::native<&step>).function("finished", &lua::Binding::native<&finished>).function("draw", &lua::Binding::native<&draw>).function("on", &lua::Binding::native<&on>).property("texture", &texture).install();
}

} // namespace haylen::lua::animation2d
