#include "2d/animation/StateGraphLua.hpp"

#include <string>
#include <utility>
#include <vector>

#include "2d/animation/AnimatedBatchLua.hpp"
#include "2d/animation/SkeletonLua.hpp"
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
#include "lua/Owners.hpp"

namespace haylen::animation2d {

int StateGraphLua::create(lua_State* L) {
    const core::Json description = lua_type(L, 1) == LUA_TSTRING ? lua::Runtime::getEngine(L).getAssets().json(lua::Stack::read<std::string_view>(L, 1)) : lua::JsonConverter::read(L, 1);
    lua::Userdata::emplace<StateGraph>(L, std::make_shared<StateGraph>(StateGraph::parse(description)));
    return 1;
}

int StateGraphLua::states(lua_State* L) {
    std::vector<std::string> names;
    for (const StateGraph::State& entry : lua::Userdata::check<StateGraph>(L, 1).getStates()) {
        names.push_back(entry.name);
    }
    lua::Stack::push(L, names);
    return 1;
}

int StateGraphLua::parameters(lua_State* L) {
    lua_newtable(L);
    for (const StateGraph::Parameter& parameter : lua::Userdata::check<StateGraph>(L, 1).getParameters()) {
        lua::Stack::push(L, StateGraph::kindName(parameter.kind));
        lua_setfield(L, -2, parameter.name.c_str());
    }
    return 1;
}

// Plays the graph on an animator, with `{sprite = index}` on a sprite of an animated batch, or with `{track = index}` on a track of a skeleton.
int StateGraphLua::newPlayer(lua_State* L) {
    const std::shared_ptr<StateGraph>& graph = lua::Userdata::checkShared<StateGraph>(L, 1);
    StateGraphPlayer::Target target;
    if (lua::Userdata::test<Animator>(L, 2) != nullptr) {
        target = lua::Userdata::checkShared<Animator>(L, 2);
    } else if (lua::Userdata::test<Skeleton>(L, 2) != nullptr) {
        lua_Integer track = 1;
        if (!lua_isnoneornil(L, 3)) {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua::Table::checkFields(L, 3, {kPlayerFields});
            lua::Table::readField(L, 3, "track", track);
        }
        luaL_argcheck(L, track >= 1, 3, "tracks are numbered from one");
        target = StateGraphPlayer::SkeletonTrack{.skeleton = lua::Userdata::checkShared<Skeleton>(L, 2), .track = static_cast<std::size_t>(track - 1)};
    } else {
        const std::shared_ptr<AnimatedBatch>& batch = lua::Userdata::checkShared<AnimatedBatch>(L, 2);
        lua_Integer sprite = 0;
        if (!lua_isnoneornil(L, 3)) {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua::Table::checkFields(L, 3, {kPlayerFields});
            lua::Table::readField(L, 3, "sprite", sprite);
        }
        if (sprite < 1 || static_cast<std::size_t>(sprite) > batch->size()) {
            return luaL_error(L, "A state graph player of an animated batch needs the number of one of its sprites in \"sprite\".");
        }
        target = StateGraphPlayer::BatchSprite{.batch = batch, .index = static_cast<std::size_t>(sprite - 1)};
    }
    lua::Userdata::emplace<StateGraphPlayer>(L, std::make_shared<StateGraphPlayer>(graph, std::move(target)));
    return 1;
}

// Sets a number, a boolean or a vector by the kind of the parameter, and `true` sets a trigger.
int StateGraphLua::set(lua_State* L) {
    StateGraphPlayer& player = lua::Userdata::check<StateGraphPlayer>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const StateGraph::Parameter* parameter = player.getGraph().findParameter(name);
    if (parameter == nullptr) {
        return luaL_error(L, "The state graph has no parameter \"%s\".", std::string(name).c_str());
    }
    switch (parameter->kind) {
    case StateGraph::Kind::Number:
        player.setNumber(name, lua::Stack::read<float>(L, 3));
        break;
    case StateGraph::Kind::Boolean:
        player.setBoolean(name, lua_toboolean(L, 3) != 0);
        break;
    case StateGraph::Kind::Trigger:
        if (lua_toboolean(L, 3) != 0) {
            player.trigger(name);
        }
        break;
    case StateGraph::Kind::Vector:
        player.setVector(name, lua::Stack::read<math::Vec2>(L, 3));
        break;
    }
    return 0;
}

int StateGraphLua::get(lua_State* L) {
    const StateGraphPlayer& player = lua::Userdata::check<StateGraphPlayer>(L, 1);
    const std::string_view name = lua::Stack::read<std::string_view>(L, 2);
    const StateGraph::Parameter* parameter = player.getGraph().findParameter(name);
    if (parameter == nullptr) {
        return luaL_error(L, "The state graph has no parameter \"%s\".", std::string(name).c_str());
    }
    switch (parameter->kind) {
    case StateGraph::Kind::Number:
        lua::Stack::push(L, player.getNumber(name));
        break;
    case StateGraph::Kind::Vector:
        lua::Stack::push(L, player.getVector(name));
        break;
    case StateGraph::Kind::Boolean:
    case StateGraph::Kind::Trigger:
        lua::Stack::push(L, parameter->kind == StateGraph::Kind::Boolean ? player.getBoolean(name) : false);
        break;
    }
    return 1;
}

int StateGraphLua::trigger(lua_State* L) {
    lua::Userdata::check<StateGraphPlayer>(L, 1).trigger(lua::Stack::read<std::string_view>(L, 2));
    return 0;
}

// The state listeners of the player run in the state of the call that moves it.
template <typename Move> void StateGraphLua::move(lua_State* L, Move&& body) {
    StateGraphPlayer& player = lua::Userdata::check<StateGraphPlayer>(L, 1);
    struct Restore {
        lua_State* previous;
        ~Restore() {
            moving = previous;
        }
    } restore{moving};
    moving = L;
    body(player);
}

int StateGraphLua::update(lua_State* L) {
    const float delta = lua::Stack::read<float>(L, 2);
    move(L, [delta](StateGraphPlayer& player) { player.update(delta); });
    return 0;
}

int StateGraphLua::enter(lua_State* L) {
    const std::string name = lua::Stack::read<std::string>(L, 2);
    move(L, [&name](StateGraphPlayer& player) { player.enter(name); });
    return 0;
}

int StateGraphLua::state(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<StateGraphPlayer>(L, 1).getState());
    return 1;
}

int StateGraphLua::stateTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<StateGraphPlayer>(L, 1).getStateTime());
    return 1;
}

int StateGraphLua::animation(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<StateGraphPlayer>(L, 1).getAnimation());
    return 1;
}

// Listens with `on('stateChanged', function, {owner = owner})`, called with the state left and the state entered. A listener ends with its owner, and without one it takes the default owner.
int StateGraphLua::on(lua_State* L) {
    StateGraphPlayer& player = lua::Userdata::check<StateGraphPlayer>(L, 1);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "stateChanged") {
        return luaL_error(L, "Unknown state graph player event \"%s\". Players report \"stateChanged\".", std::string(event).c_str());
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
    core::Connection connection = player.stateChanged.connect([function, main](std::string_view from, std::string_view to) {
        if (moving != nullptr) {
            if (function->push(moving)) {
                lua::Stack::push(moving, from);
                lua::Stack::push(moving, to);
                lua_call(moving, 2, 0);
            }
            return;
        }
        lua::Runtime::runReporting(main, [&] {
            if (function->push(main)) {
                lua::Stack::push(main, from);
                lua::Stack::push(main, to);
                lua::Runtime::protectedCall(main, 2, 0);
            }
        });
    }, {.owner = lifetime});
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    return 1;
}

void StateGraphLua::install(lua_State* L) {
    lua::ClassBuilder<StateGraph>(L).property("states", &lua::Binding::native<&states>).property("parameters", &lua::Binding::native<&parameters>).function("newPlayer", &lua::Binding::native<&newPlayer>).install();
    lua::ClassBuilder<StateGraphPlayer>(L).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("trigger", &lua::Binding::native<&trigger>).function("update", &lua::Binding::native<&update>).function("enter", &lua::Binding::native<&enter>).function("on", &lua::Binding::native<&on>).property("state", &state).property("stateTime", &stateTime).property("animation", &animation).install();
}

} // namespace haylen::animation2d
