#include "3d/animation/ClipPlayer3DBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "3d/animation/Clip3DBinding.hpp"
#include "3d/animation/Track3DBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::Clip3D;
using haylen::animation3d::ClipPlayer3D;

ClipPlayer3D& ClipPlayer3DBinding::check(lua_State* L) {
    return lua::Userdata::check<ClipPlayer3D>(L, 1);
}

int ClipPlayer3DBinding::create(lua_State* L) {
    std::shared_ptr<const Clip3D> clip = lua::Userdata::checkShared<Clip3D>(L, 1);
    ClipPlayer3D::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kFields});
        lua::Table::readField(L, 2, "loop", options.loop);
        lua::Table::readField(L, 2, "speed", options.speed);
        lua::Table::readField(L, 2, "paused", options.paused);
        lua_getfield(L, 2, "time");
        if (!lua_isnil(L, -1)) {
            float time = 0.0F;
            lua::Table::readValue(L, "time", time);
            options.time = time;
        }

        lua_pop(L, 1);
    }

    lua::Userdata::emplace<ClipPlayer3D>(L, std::make_shared<ClipPlayer3D>(std::move(clip), options));
    return 1;
}

int ClipPlayer3DBinding::getClip(lua_State* L) {
    Clip3DBinding::push(L, check(L).getClipPointer());
    return 1;
}

int ClipPlayer3DBinding::setClip(lua_State* L) {
    check(L).setClip(lua::Userdata::checkShared<Clip3D>(L, 3));
    return 0;
}

int ClipPlayer3DBinding::getTime(lua_State* L) {
    lua::Stack::push(L, check(L).getTime());
    return 1;
}

int ClipPlayer3DBinding::setTime(lua_State* L) {
    check(L).setTime(lua::Stack::read<float>(L, 3));
    return 0;
}

int ClipPlayer3DBinding::getSpeed(lua_State* L) {
    lua::Stack::push(L, check(L).getSpeed());
    return 1;
}

int ClipPlayer3DBinding::setSpeed(lua_State* L) {
    check(L).setSpeed(lua::Stack::read<float>(L, 3));
    return 0;
}

int ClipPlayer3DBinding::getPaused(lua_State* L) {
    lua::Stack::push(L, check(L).isPaused());
    return 1;
}

int ClipPlayer3DBinding::setPaused(lua_State* L) {
    check(L).setPaused(lua::Stack::read<bool>(L, 3));
    return 0;
}

int ClipPlayer3DBinding::getLoop(lua_State* L) {
    lua::Stack::push(L, check(L).getLoop());
    return 1;
}

int ClipPlayer3DBinding::setLoop(lua_State* L) {
    check(L).setLoop(lua::Stack::read<ClipPlayer3D::Loop>(L, 3));
    return 0;
}

int ClipPlayer3DBinding::finished(lua_State* L) {
    lua::Stack::push(L, check(L).isFinished());
    return 1;
}

int ClipPlayer3DBinding::update(lua_State* L) {
    ClipPlayer3D& player = check(L);
    const float delta = lua::Stack::read<float>(L, 2);
    struct Restore {
        lua_State* previous;
        ~Restore() {
            updating = previous;
        }
    } restore{updating};
    updating = L;
    player.update(delta);
    return 0;
}

// Writes the values of every track with `readValues(buffer[, first])`, laid out as `clip:sample` writes them.
int ClipPlayer3DBinding::readValues(lua_State* L) {
    const std::span<const float> values = check(L).getValues();
    const std::span<float> target = lua::core::FloatBufferBinding::readRange(L, 2);
    if (target.size() < values.size()) {
        throw std::invalid_argument("The buffer holds " + std::to_string(target.size()) + " values from its first one, while the player writes " + std::to_string(values.size()) + ".");
    }

    std::ranges::copy(values, target.begin());
    return 0;
}

// Returns the value of the track of a property of a target with `value(target, property)`, or nil for a track the clip lacks.
int ClipPlayer3DBinding::value(lua_State* L) {
    const ClipPlayer3D& player = check(L);
    const Clip3D& clip = player.getClip();
    const std::optional<std::size_t> found = clip.findTrack(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<std::string_view>(L, 3));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    const haylen::animation3d::Track3D& track = clip.getTracks()[*found];
    Track3DBinding::pushValue(L, track, player.getValues().subspan(clip.getValueOffset(*found), track.getComponentCount()));
    return 1;
}

void ClipPlayer3DBinding::notify(const lua::Owners::Function& function, lua_State* main, const Clip3D::Event* event) {
    if (updating != nullptr) {
        call(function, updating, event, false);
        return;
    }

    lua::Runtime::runReporting(main, [&] { call(function, main, event, true); });
}

void ClipPlayer3DBinding::call(const lua::Owners::Function& function, lua_State* state, const Clip3D::Event* event, bool reported) {
    if (!function.push(state)) {
        return;
    }

    int arguments = 0;
    if (event != nullptr) {
        lua_createtable(state, 0, 3);
        lua::Stack::push(state, event->name);
        lua_setfield(state, -2, "name");
        lua::Stack::push(state, event->time);
        lua_setfield(state, -2, "time");
        if (!event->data.is_null()) {
            lua::JsonConverter::push(state, event->data);
            lua_setfield(state, -2, "data");
        }

        arguments = 1;
    }

    if (reported) {
        lua::Runtime::protectedCall(state, arguments, 0);
        return;
    }

    lua_call(state, arguments, 0);
}

// Listens with `on('event', function[, {owner}])`, called with `{name, time, data}` of every event the player passes, or `on('complete', function[, {owner}])`, called once a clip that plays once reaches its end. A listener ends with its owner, and without one it takes the default owner.
int ClipPlayer3DBinding::on(lua_State* L) {
    ClipPlayer3D& player = check(L);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "event" && event != "complete") {
        return luaL_error(L, "Unknown clip player event \"%s\". Clip players report \"event\" and \"complete\".", std::string(event).c_str());
    }

    const lua::Owners::Listener listener = lua::Owners::readListener(L, 3, 4);
    lua_State* main = lua::Runtime::getMainThread(L);
    haylen::core::Connection connection;
    if (event == "event") {
        // clang-format off
        connection = player.eventFired.connect([function = listener.function, main](const Clip3D::Event& fired) {
            notify(*function, main, &fired);
        }, {.owner = listener.lifetime});
        // clang-format on
    } else {
        // clang-format off
        connection = player.completed.connect([function = listener.function, main] {
            notify(*function, main, nullptr);
        }, {.owner = listener.lifetime});
        // clang-format on
    }

    lua::Owners::pushConnection(L, listener, std::move(connection));
    return 1;
}

void ClipPlayer3DBinding::install(lua_State* L) {
    lua::ClassBuilder<ClipPlayer3D>(L).property("clip", &lua::Binding::native<&getClip>, &lua::Binding::native<&setClip>).property("time", &lua::Binding::native<&getTime>, &lua::Binding::native<&setTime>).property("speed", &lua::Binding::native<&getSpeed>, &lua::Binding::native<&setSpeed>).property("paused", &lua::Binding::native<&getPaused>, &lua::Binding::native<&setPaused>).property("loop", &lua::Binding::native<&getLoop>, &lua::Binding::native<&setLoop>).property("finished", &lua::Binding::native<&finished>).function("update", &lua::Binding::native<&update>).function("readValues", &lua::Binding::native<&readValues>).function("value", &lua::Binding::native<&value>).function("on", &lua::Binding::native<&on>).install();
}

} // namespace haylen::lua::animation3d
