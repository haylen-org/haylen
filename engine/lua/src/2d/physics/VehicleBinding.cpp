#include "2d/physics/VehicleBinding.hpp"

#include <lua.hpp>

#include <optional>
#include <string_view>
#include <vector>

#include "2d/physics/Physics2DBinding.hpp"
#include "2d/physics/ScriptedOwner.hpp"
#include "2d/physics/ShapeBinding.hpp"
#include "haylen/2d/physics/TopDownVehicle.hpp"
#include "haylen/2d/physics/Vehicle.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::lua {

template <> struct Type<lua::physics2d::ScriptedOwner<haylen::physics2d::Vehicle>> {
    static constexpr const char* name = "haylen.Vehicle";
    using Storage = lua::physics2d::ScriptedOwner<haylen::physics2d::Vehicle>;
};

template <> struct Type<lua::physics2d::ScriptedOwner<haylen::physics2d::TopDownVehicle>> {
    static constexpr const char* name = "haylen.TopDownVehicle";
    using Storage = lua::physics2d::ScriptedOwner<haylen::physics2d::TopDownVehicle>;
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::physics2d::Vehicle::Drive> {
    static std::optional<haylen::physics2d::Vehicle::Drive> fromName(std::string_view name) {
        return haylen::physics2d::Vehicle::driveFromName(name);
    }

    static std::string_view name(haylen::physics2d::Vehicle::Drive value) {
        return haylen::physics2d::Vehicle::driveName(value);
    }
};

} // namespace haylen::core

namespace haylen::lua::physics2d {

haylen::physics2d::Vehicle& VehicleBinding::checkVehicle(lua_State* L) {
    haylen::physics2d::Vehicle& vehicle = lua::Userdata::check<ScriptedOwner<haylen::physics2d::Vehicle>>(L, 1).object;
    if (!vehicle.isValid()) {
        luaL_error(L, "The vehicle was destroyed.");
    }

    return vehicle;
}

haylen::physics2d::TopDownVehicle& VehicleBinding::checkTopDown(lua_State* L) {
    haylen::physics2d::TopDownVehicle& vehicle = lua::Userdata::check<ScriptedOwner<haylen::physics2d::TopDownVehicle>>(L, 1).object;
    if (!vehicle.isValid()) {
        luaL_error(L, "The vehicle was destroyed.");
    }

    return vehicle;
}

// Builds a car with `newVehicle(world, {x, y, chassisWidth, chassisHeight, wheelRadius, rearWheel, frontWheel, acceleration, topSpeed, brakeAcceleration, centerOfMass, airControl, antiRoll, drive, ...})`. Its user value is the world object.
int VehicleBinding::newVehicle(lua_State* L) {
    const std::shared_ptr<haylen::physics2d::World>& world = lua::Userdata::checkShared<haylen::physics2d::World>(L, 1);
    haylen::physics2d::Vehicle::Options options;
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
        options.filter = ShapeBinding::readFilter(L, 2, options.filter);
    }

    lua::Userdata::emplace<ScriptedOwner<haylen::physics2d::Vehicle>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int VehicleBinding::vehicleChassis(lua_State* L) {
    const haylen::physics2d::Body body = checkVehicle(L).getChassis();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, body);
    return 1;
}

int VehicleBinding::vehicleRearWheel(lua_State* L) {
    const haylen::physics2d::Body body = checkVehicle(L).getRearWheel();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, body);
    return 1;
}

int VehicleBinding::vehicleFrontWheel(lua_State* L) {
    const haylen::physics2d::Body body = checkVehicle(L).getFrontWheel();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, body);
    return 1;
}

int VehicleBinding::vehicleJoints(lua_State* L) {
    const haylen::physics2d::Vehicle& vehicle = checkVehicle(L);
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::pushList(L, -1, std::vector<haylen::physics2d::Joint>(vehicle.getJoints().begin(), vehicle.getJoints().end()));
    return 1;
}

int VehicleBinding::vehicleGetThrottle(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).getThrottle());
    return 1;
}

int VehicleBinding::vehicleSetThrottle(lua_State* L) {
    checkVehicle(L).setThrottle(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleBinding::vehicleGetBrake(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).getBrake());
    return 1;
}

int VehicleBinding::vehicleSetBrake(lua_State* L) {
    checkVehicle(L).setBrake(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleBinding::vehicleGrounded(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).isGrounded());
    return 1;
}

int VehicleBinding::vehicleSpeed(lua_State* L) {
    lua::Stack::push(L, checkVehicle(L).getSpeed());
    return 1;
}

int VehicleBinding::vehicleDrive(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Vehicle>>(L, 1).object.getOptions().drive);
    return 1;
}

int VehicleBinding::vehicleDestroy(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::Vehicle>>(L, 1).object.destroy();
    return 0;
}

int VehicleBinding::vehicleValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::Vehicle>>(L, 1).object.isValid());
    return 1;
}

// Builds a car seen from above with `newTopDownVehicle(world, {x, y, rotation, length, width, acceleration, topSpeed, grip, steeringLock, drive, ...})`. Its user value is the world object.
int VehicleBinding::newTopDownVehicle(lua_State* L) {
    const std::shared_ptr<haylen::physics2d::World>& world = lua::Userdata::checkShared<haylen::physics2d::World>(L, 1);
    haylen::physics2d::TopDownVehicle::Options options;
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
        options.filter = ShapeBinding::readFilter(L, 2, options.filter);
    }

    lua::Userdata::emplace<ScriptedOwner<haylen::physics2d::TopDownVehicle>>(L, world, options);
    lua_pushvalue(L, 1);
    lua_setiuservalue(L, -2, 1);
    return 1;
}

int VehicleBinding::topDownBody(lua_State* L) {
    const haylen::physics2d::Body body = checkTopDown(L).getBody();
    lua_getiuservalue(L, 1, 1);
    Physics2DBinding::push(L, -1, body);
    return 1;
}

int VehicleBinding::topDownGetThrottle(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getThrottle());
    return 1;
}

int VehicleBinding::topDownSetThrottle(lua_State* L) {
    checkTopDown(L).setThrottle(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleBinding::topDownGetSteering(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSteering());
    return 1;
}

int VehicleBinding::topDownSetSteering(lua_State* L) {
    checkTopDown(L).setSteering(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleBinding::topDownGetBrake(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getBrake());
    return 1;
}

int VehicleBinding::topDownSetBrake(lua_State* L) {
    checkTopDown(L).setBrake(lua::Stack::read<float>(L, 3));
    return 0;
}

int VehicleBinding::topDownGetHandbrake(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).isHandbrake());
    return 1;
}

int VehicleBinding::topDownSetHandbrake(lua_State* L) {
    checkTopDown(L).setHandbrake(lua::Stack::read<bool>(L, 3));
    return 0;
}

int VehicleBinding::topDownSteeringAngle(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSteeringAngle());
    return 1;
}

int VehicleBinding::topDownSpeed(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSpeed());
    return 1;
}

int VehicleBinding::topDownSlip(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).getSlip());
    return 1;
}

int VehicleBinding::topDownDrifting(lua_State* L) {
    lua::Stack::push(L, checkTopDown(L).isDrifting());
    return 1;
}

int VehicleBinding::topDownDestroy(lua_State* L) {
    lua::Userdata::check<ScriptedOwner<haylen::physics2d::TopDownVehicle>>(L, 1).object.destroy();
    return 0;
}

int VehicleBinding::topDownValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ScriptedOwner<haylen::physics2d::TopDownVehicle>>(L, 1).object.isValid());
    return 1;
}

void VehicleBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedOwner<haylen::physics2d::Vehicle>>(L).function("joints", &lua::Binding::native<&vehicleJoints>).function("destroy", &lua::Binding::native<&vehicleDestroy>).property("chassis", &lua::Binding::native<&vehicleChassis>).property("rearWheel", &lua::Binding::native<&vehicleRearWheel>).property("frontWheel", &lua::Binding::native<&vehicleFrontWheel>).property("throttle", &lua::Binding::native<&vehicleGetThrottle>, &lua::Binding::native<&vehicleSetThrottle>).property("brake", &lua::Binding::native<&vehicleGetBrake>, &lua::Binding::native<&vehicleSetBrake>).property("grounded", &lua::Binding::native<&vehicleGrounded>).property("speed", &lua::Binding::native<&vehicleSpeed>).property("drive", &lua::Binding::native<&vehicleDrive>).property("valid", &lua::Binding::native<&vehicleValid>).install();
    lua::ClassBuilder<ScriptedOwner<haylen::physics2d::TopDownVehicle>>(L).function("destroy", &lua::Binding::native<&topDownDestroy>).property("body", &lua::Binding::native<&topDownBody>).property("throttle", &lua::Binding::native<&topDownGetThrottle>, &lua::Binding::native<&topDownSetThrottle>).property("steering", &lua::Binding::native<&topDownGetSteering>, &lua::Binding::native<&topDownSetSteering>).property("brake", &lua::Binding::native<&topDownGetBrake>, &lua::Binding::native<&topDownSetBrake>).property("handbrake", &lua::Binding::native<&topDownGetHandbrake>, &lua::Binding::native<&topDownSetHandbrake>).property("steeringAngle", &lua::Binding::native<&topDownSteeringAngle>).property("speed", &lua::Binding::native<&topDownSpeed>).property("slip", &lua::Binding::native<&topDownSlip>).property("drifting", &lua::Binding::native<&topDownDrifting>).property("valid", &lua::Binding::native<&topDownValid>).install();
}

void VehicleBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newVehicle", &lua::Binding::native<&newVehicle>},
        {"newTopDownVehicle", &lua::Binding::native<&newTopDownVehicle>},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::physics2d
