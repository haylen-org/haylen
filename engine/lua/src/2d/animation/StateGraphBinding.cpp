#include "2d/animation/StateGraphBinding.hpp"

#include <string>
#include <utility>
#include <vector>

#include "2d/animation/AnimatedBatchBinding.hpp"
#include "2d/animation/SkeletonBinding.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::animation2d {

int StateGraphBinding::create(lua_State* L) {
    const haylen::core::Json description = lua_type(L, 1) == LUA_TSTRING ? lua::Runtime::getEngine(L).getAssets().json(lua::Stack::read<std::string_view>(L, 1)) : lua::JsonConverter::read(L, 1);
    lua::Userdata::emplace<haylen::animation2d::StateGraph>(L, std::make_shared<haylen::animation2d::StateGraph>(haylen::animation2d::StateGraph::parse(description)));
    return 1;
}

int StateGraphBinding::states(lua_State* L) {
    std::vector<std::string> names;
    for (const haylen::animation2d::StateGraph::State& entry : lua::Userdata::check<haylen::animation2d::StateGraph>(L, 1).getStates()) {
        names.push_back(entry.name);
    }

    lua::Stack::push(L, names);
    return 1;
}

int StateGraphBinding::parameters(lua_State* L) {
    lua_newtable(L);
    for (const haylen::animation2d::StateGraph::Parameter& parameter : lua::Userdata::check<haylen::animation2d::StateGraph>(L, 1).getParameters()) {
        lua::Stack::push(L, haylen::animation2d::StateGraph::kindName(parameter.kind));
        lua_setfield(L, -2, parameter.name.c_str());
    }

    return 1;
}

// Plays the graph on an animator, with `{sprite = index}` on a sprite of an animated batch, or with `{track = index}` on a track of a skeleton.
int StateGraphBinding::newPlayer(lua_State* L) {
    const std::shared_ptr<haylen::animation2d::StateGraph>& graph = lua::Userdata::checkShared<haylen::animation2d::StateGraph>(L, 1);
    haylen::animation2d::StateGraphPlayer::Target target;
    if (lua::Userdata::test<haylen::animation2d::Animator>(L, 2) != nullptr) {
        target = lua::Userdata::checkShared<haylen::animation2d::Animator>(L, 2);
    } else if (lua::Userdata::test<haylen::animation2d::Skeleton>(L, 2) != nullptr) {
        lua_Integer track = 1;
        if (!lua_isnoneornil(L, 3)) {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua::Table::checkFields(L, 3, {kPlayerFields});
            lua::Table::readField(L, 3, "track", track);
        }

        luaL_argcheck(L, track >= 1, 3, "tracks are numbered from one");
        target = haylen::animation2d::StateGraphPlayer::SkeletonTrack{.skeleton = lua::Userdata::checkShared<haylen::animation2d::Skeleton>(L, 2), .track = static_cast<std::size_t>(track - 1)};
    } else {
        const std::shared_ptr<haylen::animation2d::AnimatedBatch>& batch = lua::Userdata::checkShared<haylen::animation2d::AnimatedBatch>(L, 2);
        lua_Integer sprite = 0;
        if (!lua_isnoneornil(L, 3)) {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua::Table::checkFields(L, 3, {kPlayerFields});
            lua::Table::readField(L, 3, "sprite", sprite);
        }

        if (sprite < 1 || static_cast<std::size_t>(sprite) > batch->size()) {
            return luaL_error(L, "A state graph player of an animated batch needs the number of one of its sprites in \"sprite\".");
        }

        target = haylen::animation2d::StateGraphPlayer::BatchSprite{.batch = batch, .index = static_cast<std::size_t>(sprite - 1)};
    }

    lua::Userdata::emplace<haylen::animation2d::StateGraphPlayer>(L, std::make_shared<haylen::animation2d::StateGraphPlayer>(graph, std::move(target)));
    return 1;
}

// Sets a number, a boolean or a vector by the kind of the parameter, and `true` sets a trigger.
int StateGraphBinding::set(lua_State* L) {
    haylen::animation2d::StateGraphPlayer& player = lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const haylen::animation2d::StateGraph::Parameter* parameter = player.getGraph().findParameter(name);
    if (parameter == nullptr) {
        return luaL_error(L, "The state graph has no parameter \"%s\".", std::string(name).c_str());
    }

    switch (parameter->kind) {
    case haylen::animation2d::StateGraph::Kind::Number:
        player.setNumber(name, lua::Stack::read<float>(L, 3));
        break;
    case haylen::animation2d::StateGraph::Kind::Boolean:
        player.setBoolean(name, lua_toboolean(L, 3) != 0);
        break;
    case haylen::animation2d::StateGraph::Kind::Trigger:
        if (lua_toboolean(L, 3) != 0) {
            player.trigger(name);
        }

        break;
    case haylen::animation2d::StateGraph::Kind::Vector:
        player.setVector(name, lua::Stack::read<haylen::math::Vec2>(L, 3));
        break;
    }

    return 0;
}

int StateGraphBinding::get(lua_State* L) {
    const haylen::animation2d::StateGraphPlayer& player = lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const haylen::animation2d::StateGraph::Parameter* parameter = player.getGraph().findParameter(name);
    if (parameter == nullptr) {
        return luaL_error(L, "The state graph has no parameter \"%s\".", std::string(name).c_str());
    }

    switch (parameter->kind) {
    case haylen::animation2d::StateGraph::Kind::Number:
        lua::Stack::push(L, player.getNumber(name));
        break;
    case haylen::animation2d::StateGraph::Kind::Vector:
        lua::Stack::push(L, player.getVector(name));
        break;
    case haylen::animation2d::StateGraph::Kind::Boolean:
    case haylen::animation2d::StateGraph::Kind::Trigger:
        lua::Stack::push(L, parameter->kind == haylen::animation2d::StateGraph::Kind::Boolean ? player.getBoolean(name) : false);
        break;
    }

    return 1;
}

int StateGraphBinding::trigger(lua_State* L) {
    lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1).trigger(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

// The state listeners of the player run in the state of the call that moves it.
template <typename Move> void StateGraphBinding::move(lua_State* L, Move&& body) {
    haylen::animation2d::StateGraphPlayer& player = lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1);
    struct Restore {
        lua_State* previous;
        ~Restore() {
            moving = previous;
        }
    } restore{moving};
    moving = L;
    body(player);
}

int StateGraphBinding::update(lua_State* L) {
    const float delta = lua::Stack::read<float>(L, 2);
    move(L, [delta](haylen::animation2d::StateGraphPlayer& player) { player.update(delta); });
    return 0;
}

int StateGraphBinding::enter(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 2);
    move(L, [&name](haylen::animation2d::StateGraphPlayer& player) { player.enter(name); });
    return 0;
}

int StateGraphBinding::state(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1).getState());
    return 1;
}

int StateGraphBinding::stateTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1).getStateTime());
    return 1;
}

int StateGraphBinding::animation(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1).getAnimation());
    return 1;
}

// A player that changes state while Lua moves it calls the listener on that call, so an error reaches its caller, and otherwise the main thread reports the error.
void StateGraphBinding::notifyStateChanged(const lua::Owners::Function& function, lua_State* main, std::string_view from, std::string_view to) {
    if (moving != nullptr) {
        callStateChanged(function, moving, from, to, false);
        return;
    }

    lua::Runtime::runReporting(main, [&] { callStateChanged(function, main, from, to, true); });
}

void StateGraphBinding::callStateChanged(const lua::Owners::Function& function, lua_State* state, std::string_view from, std::string_view to, bool reported) {
    if (!function.push(state)) {
        return;
    }

    lua::Stack::push(state, from);
    lua::Stack::push(state, to);

    if (reported) {
        lua::Runtime::protectedCall(state, 2, 0);
        return;
    }

    lua_call(state, 2, 0);
}

// Listens with `on('stateChanged', function, {owner = owner})`, called with the state left and the state entered. A listener ends with its owner, and without one it takes the default owner.
int StateGraphBinding::on(lua_State* L) {
    haylen::animation2d::StateGraphPlayer& player = lua::Userdata::check<haylen::animation2d::StateGraphPlayer>(L, 1);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "stateChanged") {
        return luaL_error(L, "Unknown state graph player event \"%s\". Players report \"stateChanged\".", std::string(event).c_str());
    }

    const lua::Owners::Listener listener = lua::Owners::readListener(L, 3, 4);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    haylen::core::Connection connection = player.stateChanged.connect([function = listener.function, main](std::string_view from, std::string_view to) {
        notifyStateChanged(*function, main, from, to);
    }, {.owner = listener.lifetime});
    // clang-format on
    lua::Owners::pushConnection(L, listener, std::move(connection));
    return 1;
}

void StateGraphBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::animation2d::StateGraph>(L).property("states", &lua::Binding::native<&states>).property("parameters", &lua::Binding::native<&parameters>).function("newPlayer", &lua::Binding::native<&newPlayer>).install();
    lua::ClassBuilder<haylen::animation2d::StateGraphPlayer>(L).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("trigger", &lua::Binding::native<&trigger>).function("update", &lua::Binding::native<&update>).function("enter", &lua::Binding::native<&enter>).function("on", &lua::Binding::native<&on>).property("state", &state).property("stateTime", &stateTime).property("animation", &animation).install();
}

} // namespace haylen::lua::animation2d
