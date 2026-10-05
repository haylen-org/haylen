#pragma once

#include <array>
#include <string_view>

#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Shape.hpp"

struct lua_State;

namespace haylen::physics2d {

// Installs the `Shape` class of `haylen.physics2d` and reads the shape options that bodies and queries take.
class ShapeLua final {
  public:
    static void install(lua_State* L);

    // Reads the `category`, `mask` and `group` fields of the table at `index` over the given filter.
    [[nodiscard]] static CollisionFilter readFilter(lua_State* L, int index, CollisionFilter filter);

    // Reads `{density, friction, restitution, rollingResistance, category, mask, group, sensor, offsetX, offsetY, rotation, tangentSpeed, oneWay, contactEvents, hitEvents, sensorEvents}`, where `nil` gives the defaults.
    [[nodiscard]] static Shape::Options readOptions(lua_State* L, int index);

    // Pushes an outline as `{points = {Vec2, ...}, closed = boolean}`.
    static void pushOutline(lua_State* L, const Shape::Outline& outline);

  private:
    static constexpr std::array<std::string_view, 16> kOptionFields{"density", "friction", "restitution", "rollingResistance", "category", "mask", "group", "sensor", "offsetX", "offsetY", "rotation", "tangentSpeed", "oneWay", "contactEvents", "hitEvents", "sensorEvents"};

    [[nodiscard]] static Shape& check(lua_State* L);

    static int isValid(lua_State* L);
    static int getBody(lua_State* L);
    static int isSensor(lua_State* L);
    static int getKind(lua_State* L);
    static int getPoints(lua_State* L);
    static int getWorldPoints(lua_State* L);
    static int getRadius(lua_State* L);
    static int outline(lua_State* L);
    static int getBounds(lua_State* L);
    static int getCategory(lua_State* L);
    static int setCategory(lua_State* L);
    static int getMask(lua_State* L);
    static int setMask(lua_State* L);
    static int getGroup(lua_State* L);
    static int setGroup(lua_State* L);
    static int getTangentSpeed(lua_State* L);
    static int setTangentSpeed(lua_State* L);
    static int getOneWay(lua_State* L);
    static int setOneWay(lua_State* L);
    static int getFriction(lua_State* L);
    static int setFriction(lua_State* L);
    static int getRestitution(lua_State* L);
    static int setRestitution(lua_State* L);
    static int getDensity(lua_State* L);
    static int setDensity(lua_State* L);
    static int getRollingResistance(lua_State* L);
    static int setRollingResistance(lua_State* L);
    static int hasContactEvents(lua_State* L);
    static int setContactEvents(lua_State* L);
    static int hasHitEvents(lua_State* L);
    static int setHitEvents(lua_State* L);
    static int hasSensorEvents(lua_State* L);
    static int setSensorEvents(lua_State* L);
    static int overlaps(lua_State* L);
    static int destroy(lua_State* L);
    static int equal(lua_State* L);
};

} // namespace haylen::physics2d
