#include "input/GamepadMotionBinding.hpp"

#include <lua.hpp>

#include <memory>

#include "haylen/core/Engine.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "runtime/Owners.hpp"

namespace haylen::lua::input {

void GamepadMotionBinding::Ownership::disconnect() {
    if (!connected) {
        return;
    }

    connected = false;
    motion.stop(id);
}

haylen::input::Input& GamepadMotionBinding::getInput(lua_State* L) {
    return lua::Runtime::getEngine(L).getInput();
}

std::size_t GamepadMotionBinding::readGamepadIndex(lua_State* L, int index) {
    const lua_Integer value = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, value >= 1 && value <= static_cast<lua_Integer>(haylen::input::Input::kMaxGamepads), index, "gamepad index out of range");
    return static_cast<std::size_t>(value - 1);
}

void GamepadMotionBinding::readSensors(lua_State* L, int index, haylen::input::GamepadMotion::Sensors& sensors) {
    if (lua_getfield(L, index, "sensors") == LUA_TNIL) {
        lua_pop(L, 1);
        return;
    }

    if (!lua_istable(L, -1)) {
        luaL_error(L, "The option \"sensors\" of the motion of gamepads must be a list of sensor names, such as {'accelerometer', 'gyroscope'}.");
    }

    sensors.reset();
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, -1));
    for (lua_Integer entry = 1; entry <= count; ++entry) {
        lua_rawgeti(L, -1, entry);
        sensors.set(static_cast<std::size_t>(lua::Stack::read<haylen::input::MotionSensor>(L, -1)));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
}

void GamepadMotionBinding::pushSensors(lua_State* L, const haylen::input::GamepadMotion::Sensors& sensors) {
    lua_createtable(L, static_cast<int>(sensors.count()), 0);
    lua_Integer slot = 1;
    for (std::size_t index = 0; index < sensors.size(); ++index) {
        if (sensors.test(index)) {
            lua::Stack::push(L, static_cast<haylen::input::MotionSensor>(index));
            lua_rawseti(L, -2, slot++);
        }
    }
}

int GamepadMotionBinding::pushVector(lua_State* L, const std::array<float, 3>& value) {
    for (const float component : value) {
        lua::Stack::push(L, component);
    }

    return 3;
}

// Starts the sensors with `startGamepadMotion({sensors, owner})` and returns the request, which runs at once.
int GamepadMotionBinding::startGamepadMotion(lua_State* L) {
    haylen::input::GamepadMotion::Sensors sensors;
    sensors.set(static_cast<std::size_t>(haylen::input::MotionSensor::Accelerometer));
    int owner = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kStartOptions});
        readSensors(L, 1, sensors);
        if (lua_getfield(L, 1, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        } else {
            lua_pop(L, 1);
        }
    }

    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }

    haylen::input::GamepadMotion& motion = lua::Runtime::getEngine(L).getGamepadMotion();
    const std::uint64_t id = motion.start(sensors);
    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<Ownership>(motion, id));
    }

    lua::Userdata::emplace<Request>(L, Request{.id = id, .sensors = sensors});
    return 1;
}

int GamepadMotionBinding::gamepadSensors(lua_State* L) {
    pushSensors(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motionSensors);
    return 1;
}

int GamepadMotionBinding::gamepadAcceleration(lua_State* L) {
    return pushVector(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motion.acceleration);
}

int GamepadMotionBinding::gamepadRotationRate(lua_State* L) {
    return pushVector(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motion.rotationRate);
}

int GamepadMotionBinding::gamepadAttitude(lua_State* L) {
    for (const float component : getInput(L).getGamepad(readGamepadIndex(L, 1)).motion.attitude) {
        lua::Stack::push(L, component);
    }

    return 4;
}

int GamepadMotionBinding::gamepadTilt(lua_State* L) {
    const haylen::math::Vec2 value = getInput(L).getGamepadTilt(readGamepadIndex(L, 1));
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

// Returns the fingers down on the touchpads of a gamepad as `{finger, touchpad, x, y, pressure}` tables, counting fingers and touchpads from 1.
int GamepadMotionBinding::gamepadTouches(lua_State* L) {
    const haylen::input::GamepadState& gamepad = getInput(L).getGamepad(readGamepadIndex(L, 1));
    lua_createtable(L, 0, 0);
    lua_Integer slot = 1;
    for (std::size_t index = 0; index < gamepad.touchpad.size(); ++index) {
        const haylen::input::TouchpadFinger& finger = gamepad.touchpad[index];
        if (!finger.down) {
            continue;
        }

        lua_createtable(L, 0, 5);
        lua_pushinteger(L, static_cast<lua_Integer>(index + 1));
        lua_setfield(L, -2, "finger");
        lua_pushinteger(L, static_cast<lua_Integer>(finger.touchpad) + 1);
        lua_setfield(L, -2, "touchpad");
        lua::Stack::push(L, finger.position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, finger.position.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, finger.pressure);
        lua_setfield(L, -2, "pressure");
        lua_rawseti(L, -2, slot++);
    }

    return 1;
}

int GamepadMotionBinding::stop(lua_State* L) {
    lua::Runtime::getEngine(L).getGamepadMotion().stop(lua::Userdata::check<Request>(L, 1).id);
    return 0;
}

int GamepadMotionBinding::running(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGamepadMotion().isRunning(lua::Userdata::check<Request>(L, 1).id));
    return 1;
}

int GamepadMotionBinding::sensors(lua_State* L) {
    pushSensors(L, lua::Userdata::check<Request>(L, 1).sensors);
    return 1;
}

void GamepadMotionBinding::install(lua_State* L) {
    lua::ClassBuilder<Request>(L).function("stop", &stop).property("running", &running).property("sensors", &sensors).install();
}

void GamepadMotionBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"startGamepadMotion", &lua::Binding::native<&startGamepadMotion>}, {"gamepadSensors", &gamepadSensors}, {"gamepadAcceleration", &gamepadAcceleration}, {"gamepadRotationRate", &gamepadRotationRate}, {"gamepadAttitude", &gamepadAttitude}, {"gamepadTilt", &gamepadTilt}, {"gamepadTouches", &gamepadTouches}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::input
