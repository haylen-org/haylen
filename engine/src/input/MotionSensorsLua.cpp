#include "input/MotionSensorsLua.hpp"

#include <lua.hpp>

#include <memory>
#include <optional>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "lua/Owners.hpp"
#include "platform/PlatformLua.hpp"
#include "varn/async/Promise.h"

namespace haylen::input {

void MotionSensorsLua::Ownership::disconnect() {
    if (!connected) {
        return;
    }
    connected = false;
    sensors.stop(id);
}

Input& MotionSensorsLua::getInput(lua_State* L) {
    return lua::Runtime::getEngine(L).getInput();
}

std::size_t MotionSensorsLua::readGamepadIndex(lua_State* L, int index) {
    const lua_Integer value = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, value >= 1 && value <= static_cast<lua_Integer>(Input::kMaxGamepads), index, "gamepad index out of range");
    return static_cast<std::size_t>(value - 1);
}

void MotionSensorsLua::readSensors(lua_State* L, int index, MotionSensors::Sensors& sensors) {
    if (lua_getfield(L, index, "sensors") == LUA_TNIL) {
        lua_pop(L, 1);
        return;
    }
    if (!lua_istable(L, -1)) {
        luaL_error(L, "The option \"sensors\" of the motion sensors must be a list of sensor names, such as {'accelerometer', 'gyroscope'}.");
    }
    sensors.reset();
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, -1));
    for (lua_Integer entry = 1; entry <= count; ++entry) {
        lua_rawgeti(L, -1, entry);
        sensors.set(static_cast<std::size_t>(lua::Stack::read<MotionSensor>(L, -1)));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void MotionSensorsLua::pushSensors(lua_State* L, const MotionSensors::Sensors& sensors) {
    lua_createtable(L, static_cast<int>(sensors.count()), 0);
    lua_Integer slot = 1;
    for (std::size_t index = 0; index < sensors.size(); ++index) {
        if (sensors.test(index)) {
            lua::Stack::push(L, static_cast<MotionSensor>(index));
            lua_rawseti(L, -2, slot++);
        }
    }
}

int MotionSensorsLua::pushVector(lua_State* L, const std::array<float, 3>& value) {
    for (const float component : value) {
        lua::Stack::push(L, component);
    }
    return 3;
}

int MotionSensorsLua::pushQuaternion(lua_State* L, const std::array<float, 4>& value) {
    for (const float component : value) {
        lua::Stack::push(L, component);
    }
    return 4;
}

// Giving up a start that still waits for its answer stops the request, which fails the call with `cancelled`, while a request that runs stops through its own `stop`.
bool MotionSensorsLua::cancelStart(lua_State* L, std::uint64_t id) {
    MotionSensors& sensors = lua::Runtime::getEngine(L).getMotionSensors();
    if (!sensors.isWaiting(id)) {
        return false;
    }
    sensors.stop(id);
    return true;
}

// Starts the sensors with `startMotion({sensors, rate, device, gamepads, owner})` and returns a call that resolves with the request once they run.
int MotionSensorsLua::startMotion(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    MotionSensors::Options options;
    options.sensors.set(static_cast<std::size_t>(MotionSensor::Accelerometer));
    int owner = 0;
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {kStartOptions});
        readSensors(L, 1, options.sensors);
        lua::Table::readField(L, 1, "rate", options.rate);
        lua::Table::readField(L, 1, "device", options.device);
        lua::Table::readField(L, 1, "gamepads", options.gamepads);
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

    auto pending = std::make_shared<platform::PlatformLua::Call>();
    pending->promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    pending->cancel = &cancelStart;

    // clang-format off
    MotionSensors& sensors = engine.getMotionSensors();
    pending->id = sensors.start(options, [pending, options](std::optional<MotionSensors::Failure> failure) {
        if (failure) {
            platform::Bridge::Error error{.message = std::move(failure->message), .code = std::move(failure->code)};
            if (!failure->missing.empty()) {
                error.data = {{"missing", platform::Permission::Answer{.missing = std::move(failure->missing)}.missingToJson()}};
            }
            pending->fail(std::move(error));
            return;
        }
        pending->promise->resolveCustom([request = Request{.id = pending->id, .options = options}](lua_State* state) { lua::Userdata::emplace<Request>(state, request); });
    });
    // clang-format on

    if (owner != 0) {
        lua::Owners::add(L, owner, std::make_shared<Ownership>(sensors, pending->id));
    }
    lua::Userdata::emplace<platform::PlatformLua::Call>(L, std::move(pending));
    return 1;
}

int MotionSensorsLua::motionSensors(lua_State* L) {
    pushSensors(L, getInput(L).getMotion().sensors);
    return 1;
}

int MotionSensorsLua::acceleration(lua_State* L) {
    return pushVector(L, getInput(L).getMotion().acceleration);
}

int MotionSensorsLua::rotationRate(lua_State* L) {
    return pushVector(L, getInput(L).getMotion().rotationRate);
}

int MotionSensorsLua::attitude(lua_State* L) {
    return pushQuaternion(L, getInput(L).getMotion().attitude);
}

int MotionSensorsLua::tilt(lua_State* L) {
    const math::Vec2 value = getInput(L).getTilt();
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int MotionSensorsLua::calibrateTilt(lua_State* L) {
    lua::Stack::push(L, getInput(L).calibrateTilt());
    return 1;
}

int MotionSensorsLua::resetTilt(lua_State* L) {
    getInput(L).resetTilt();
    return 0;
}

int MotionSensorsLua::gamepadSensors(lua_State* L) {
    pushSensors(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motionSensors);
    return 1;
}

int MotionSensorsLua::gamepadAcceleration(lua_State* L) {
    return pushVector(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motion.acceleration);
}

int MotionSensorsLua::gamepadRotationRate(lua_State* L) {
    return pushVector(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motion.rotationRate);
}

int MotionSensorsLua::gamepadAttitude(lua_State* L) {
    return pushQuaternion(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).motion.attitude);
}

int MotionSensorsLua::gamepadTilt(lua_State* L) {
    const math::Vec2 value = getInput(L).getGamepadTilt(readGamepadIndex(L, 1));
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

// Returns the fingers down on the touchpads of a gamepad as `{finger, touchpad, x, y, pressure}` tables, counting fingers and touchpads from 1.
int MotionSensorsLua::gamepadTouches(lua_State* L) {
    const GamepadState& gamepad = getInput(L).getGamepad(readGamepadIndex(L, 1));
    lua_createtable(L, 0, 0);
    lua_Integer slot = 1;
    for (std::size_t index = 0; index < gamepad.touchpad.size(); ++index) {
        const TouchpadFinger& finger = gamepad.touchpad[index];
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

int MotionSensorsLua::stop(lua_State* L) {
    lua::Runtime::getEngine(L).getMotionSensors().stop(lua::Userdata::check<Request>(L, 1).id);
    return 0;
}

int MotionSensorsLua::running(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getMotionSensors().isRunning(lua::Userdata::check<Request>(L, 1).id));
    return 1;
}

int MotionSensorsLua::sensors(lua_State* L) {
    pushSensors(L, lua::Userdata::check<Request>(L, 1).options.sensors);
    return 1;
}

int MotionSensorsLua::rate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Request>(L, 1).options.rate);
    return 1;
}

int MotionSensorsLua::device(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Request>(L, 1).options.device);
    return 1;
}

int MotionSensorsLua::gamepads(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Request>(L, 1).options.gamepads);
    return 1;
}

void MotionSensorsLua::install(lua_State* L) {
    lua::ClassBuilder<Request>(L).function("stop", &stop).property("running", &running).property("sensors", &sensors).property("rate", &rate).property("device", &device).property("gamepads", &gamepads).install();
}

void MotionSensorsLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"startMotion", &lua::Binding::native<&startMotion>}, {"motionSensors", &motionSensors}, {"acceleration", &acceleration}, {"rotationRate", &rotationRate}, {"attitude", &attitude}, {"tilt", &tilt}, {"calibrateTilt", &calibrateTilt}, {"resetTilt", &resetTilt}, {"gamepadSensors", &gamepadSensors}, {"gamepadAcceleration", &gamepadAcceleration}, {"gamepadRotationRate", &gamepadRotationRate}, {"gamepadAttitude", &gamepadAttitude}, {"gamepadTilt", &gamepadTilt}, {"gamepadTouches", &gamepadTouches}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::input
