#include "2d/physics/VehicleLua.hpp"

#include <lua.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "haylen/2d/physics/TopDownVehicle.hpp"
#include "haylen/2d/physics/Vehicle.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<physics2d::ScriptedOwner<physics2d::Vehicle>> {
    static constexpr const char* name = "haylen.Vehicle";
    using Storage = physics2d::ScriptedOwner<physics2d::Vehicle>;
};

template <> struct Type<physics2d::ScriptedOwner<physics2d::TopDownVehicle>> {
    static constexpr const char* name = "haylen.TopDownVehicle";
    using Storage = physics2d::ScriptedOwner<physics2d::TopDownVehicle>;
};

template <> struct EnumNames<physics2d::Vehicle::Drive> {
    static std::optional<physics2d::Vehicle::Drive> fromName(std::string_view name) {
        return physics2d::Vehicle::driveFromName(name);
    }
    static std::string_view name(physics2d::Vehicle::Drive value) {
        return physics2d::Vehicle::driveName(value);
    }
};

} // namespace haylen::lua

namespace haylen::physics2d {

Vehicle& VehicleLua::checkVehicle(lua_State* L) {
    Vehicle& vehicle = lua::Userdata::check<ScriptedOwner<Vehicle>>(L, 1).object;
    if (!vehicle.isValid()) {
        luaL_error(L, "The vehicle was destroyed.");
    }
    return vehicle;
}

TopDownVehicle& VehicleLua::checkTopDown(lua_State* L) {
    TopDownVehicle& vehicle = lua::Userdata::check<ScriptedOwner<TopDownVehicle>>(L, 1).object;
    if (!vehicle.isValid()) {
        luaL_error(L, "The vehicle was destroyed.");
    }
    return vehicle;
}

// Builds a car with `newVehicle(world, {x, y, chassisWidth, chassisHeight, wheelRadius, rearWheel, frontWheel, acceleration, topSpeed, brakeAcceleration, centerOfMass, airControl, antiRoll, drive, ...})`. Its user value is the world object.
int VehicleLua::newVehicle(lua_State* L) {
    const std::shared_ptr<World>& world = lua::Userdata::checkShared<World>(L, 1);
    Vehicle::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kVehicleFields});
        lua::Table::readField(L, 2, "x", options.position.x);
        lua::Table::readField(L, 2, "y", options.position.y);
        lua::Table::readField(L, 2, "chassisWidth", options.chassisSize.x);
        lua::Table::readField(L, 2, "chassisHeight", options.chassisSize.y);
        lua::Table::readField(L, 2, "wheelRadius", options.wheelRadius);
        lua::Table::readField(L, 2, "rearWheel", options.rearWheel);
        lua::Table::readField(L, 2, "frontWheel", options.frontWheel);
        lua::Table::readField(L, 2, "density", options.density);
        lua::Table::readField(L, 2, "wheelDensity", options.wheelDensity);
        lua::Table::readField(L, 2, "wheelFriction", options.wheelFriction);
        lua::Table::readField(L, 2, "suspensionHertz", options.suspensionHertz);
        lua::Table::readField(L, 2, "suspensionDamping", options.suspensionDamping);
        lua::Table::readField(L, 2, "suspensionTravel", options.suspensionTravel);
        lua::Table::readField(L, 2, "acceleration", options.acceleration);
        lua::Table::readField(L, 2, "topSpeed", options.topSpeed);
        lua::Table::readField(L, 2, "brakeAcceleration", options.brakeAcceleration);
        lua::Table::readField(L, 2, "centerOfMass", options.centerOfMass);
        lua::Table::readField(L, 2, "airControl", options.airControl);
        lua::Table::readField(L, 2, "antiRoll", options.antiRoll);
        lua::Table::readField(L, 2, "drive", options.drive);
        lua::Table::readField(L, 2, "bullet", options.bullet);
        options.filter = ShapeLua::readFilter(L, 2, options.filter);
    }
    lua::Userdata::emplace<ScriptedOwner<Vehicle>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int VehicleLua::vehicleChassis(lua_State* L) {
    const Body body = checkVehicle(L).getChassis();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int VehicleLua::vehicleRearWheel(lua_State* L) {
    const Body body = checkVehicle(L).getRearWheel();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int VehicleLua::vehicleFrontWheel(lua_State* L) {
    const Body body = checkVehicle(L).getFrontWheel();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int VehicleLua::vehicleJoints(lua_State* L) {
    const Vehicle& vehicle = checkVehicle(L);
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::pushList(L, -1, std::vector<Joint>(vehicle.getJoints().begin(), vehicle.getJoints().end()));
    return 1;
}

int VehicleLua::vehicleGetThrottle(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).getThrottle());
    return 1;
}

int VehicleLua::vehicleSetThrottle(lua_State* L) {
    checkVehicle(L).setThrottle(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleLua::vehicleGetBrake(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).getBrake());
    return 1;
}

int VehicleLua::vehicleSetBrake(lua_State* L) {
    checkVehicle(L).setBrake(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleLua::vehicleGrounded(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).isGrounded());
    return 1;
}

int VehicleLua::vehicleSpeed(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).getSpeed());
    return 1;
}

int VehicleLua::vehicleDrive(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Vehicle>>(L, 1).object.getOptions().drive);
    return 1;
}

int VehicleLua::vehicleDestroy(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<Vehicle>>(L, 1).object.destroy();
    return 0;
}

int VehicleLua::vehicleValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<Vehicle>>(L, 1).object.isValid());
    return 1;
}

// Builds a car seen from above with `newTopDownVehicle(world, {x, y, rotation, length, width, acceleration, topSpeed, grip, steeringLock, drive, ...})`. Its user value is the world object.
int VehicleLua::newTopDownVehicle(lua_State* L) {
    const std::shared_ptr<World>& world = lua::Userdata::checkShared<World>(L, 1);
    TopDownVehicle::Options options;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kTopDownFields});
        lua::Table::readField(L, 2, "x", options.position.x);
        lua::Table::readField(L, 2, "y", options.position.y);
        lua::Table::readField(L, 2, "rotation", options.rotation);
        lua::Table::readField(L, 2, "length", options.size.x);
        lua::Table::readField(L, 2, "width", options.size.y);
        lua::Table::readField(L, 2, "density", options.density);
        lua::Table::readField(L, 2, "friction", options.friction);
        lua::Table::readField(L, 2, "restitution", options.restitution);
        lua::Table::readField(L, 2, "frontAxle", options.frontAxle);
        lua::Table::readField(L, 2, "rearAxle", options.rearAxle);
        lua::Table::readField(L, 2, "acceleration", options.acceleration);
        lua::Table::readField(L, 2, "reverseAcceleration", options.reverseAcceleration);
        lua::Table::readField(L, 2, "topSpeed", options.topSpeed);
        lua::Table::readField(L, 2, "reverseSpeed", options.reverseSpeed);
        lua::Table::readField(L, 2, "brakeAcceleration", options.brakeAcceleration);
        lua::Table::readField(L, 2, "grip", options.grip);
        lua::Table::readField(L, 2, "handbrakeGrip", options.handbrakeGrip);
        lua::Table::readField(L, 2, "steeringLock", options.steeringLock);
        lua::Table::readField(L, 2, "steeringSpeed", options.steeringSpeed);
        lua::Table::readField(L, 2, "highSpeedLock", options.highSpeedLock);
        lua::Table::readField(L, 2, "rollingDrag", options.rollingDrag);
        lua::Table::readField(L, 2, "angularDamping", options.angularDamping);
        lua::Table::readField(L, 2, "drive", options.drive);
        lua::Table::readField(L, 2, "bullet", options.bullet);
        options.filter = ShapeLua::readFilter(L, 2, options.filter);
    }
    lua::Userdata::emplace<ScriptedOwner<TopDownVehicle>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int VehicleLua::topDownBody(lua_State* L) {
    const Body body = checkTopDown(L).getBody();
    lua_getiuservalue(L, 1, 1);
    Physics2DLua::push(L, -1, body);
    return 1;
}

int VehicleLua::topDownGetThrottle(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getThrottle());
    return 1;
}

int VehicleLua::topDownSetThrottle(lua_State* L) {
    checkTopDown(L).setThrottle(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleLua::topDownGetSteering(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSteering());
    return 1;
}

int VehicleLua::topDownSetSteering(lua_State* L) {
    checkTopDown(L).setSteering(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleLua::topDownGetBrake(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getBrake());
    return 1;
}

int VehicleLua::topDownSetBrake(lua_State* L) {
    checkTopDown(L).setBrake(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleLua::topDownGetHandbrake(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).isHandbrake());
    return 1;
}

int VehicleLua::topDownSetHandbrake(lua_State* L) {
    checkTopDown(L).setHandbrake(lua::Stack::read<bool>(L, 3));
    return 0;
}

int VehicleLua::topDownSteeringAngle(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSteeringAngle());
    return 1;
}

int VehicleLua::topDownSpeed(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSpeed());
    return 1;
}

int VehicleLua::topDownSlip(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSlip());
    return 1;
}

int VehicleLua::topDownDrifting(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).isDrifting());
    return 1;
}

int VehicleLua::topDownDestroy(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<TopDownVehicle>>(L, 1).object.destroy();
    return 0;
}

int VehicleLua::topDownValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<TopDownVehicle>>(L, 1).object.isValid());
    return 1;
}

void VehicleLua::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<Vehicle>>(L).function("joints", &lua::Binding::native<&vehicleJoints>).function("destroy", &lua::Binding::native<&vehicleDestroy>).property("chassis", &lua::Binding::native<&vehicleChassis>).property("rearWheel", &lua::Binding::native<&vehicleRearWheel>).property("frontWheel", &lua::Binding::native<&vehicleFrontWheel>).property("throttle", &lua::Binding::native<&vehicleGetThrottle>, &lua::Binding::native<&vehicleSetThrottle>).property("brake", &lua::Binding::native<&vehicleGetBrake>, &lua::Binding::native<&vehicleSetBrake>).property("grounded", &lua::Binding::native<&vehicleGrounded>).property("speed", &lua::Binding::native<&vehicleSpeed>).property("drive", &lua::Binding::native<&vehicleDrive>).property("valid", &lua::Binding::native<&vehicleValid>).install();
    lua::ClassBuilder<ScriptedOwner<TopDownVehicle>>(L).function("destroy", &lua::Binding::native<&topDownDestroy>).property("body", &lua::Binding::native<&topDownBody>).property("throttle", &lua::Binding::native<&topDownGetThrottle>, &lua::Binding::native<&topDownSetThrottle>).property("steering", &lua::Binding::native<&topDownGetSteering>, &lua::Binding::native<&topDownSetSteering>).property("brake", &lua::Binding::native<&topDownGetBrake>, &lua::Binding::native<&topDownSetBrake>).property("handbrake", &lua::Binding::native<&topDownGetHandbrake>, &lua::Binding::native<&topDownSetHandbrake>).property("steeringAngle", &lua::Binding::native<&topDownSteeringAngle>).property("speed", &lua::Binding::native<&topDownSpeed>).property("slip", &lua::Binding::native<&topDownSlip>).property("drifting", &lua::Binding::native<&topDownDrifting>).property("valid", &lua::Binding::native<&topDownValid>).install();
}

void VehicleLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newVehicle", &lua::Binding::native<&newVehicle>},
        {"newTopDownVehicle", &lua::Binding::native<&newTopDownVehicle>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::physics2d
