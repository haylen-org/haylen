#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/graphics/CameraRig.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<graphics2d::CameraRig> {
    static constexpr const char* name = "haylen.CameraRig";
    using Storage = std::shared_ptr<graphics2d::CameraRig>;
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// Installs the `CameraRig` class of `haylen.graphics2d`.
class CameraRigLua final {
  public:
    static void install(lua_State* L);

    // Creates a rig with `graphics2d.newCameraRig({limits, zoom, blendTime, zones, rail})`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kOptionFields{"limits", "zoom", "blendTime", "zones", "rail"};
    static constexpr std::array<std::string_view, 6> kZoneFields{"name", "area", "limits", "zoom", "offset", "priority"};

    [[nodiscard]] static math::Vec2 readZoom(lua_State* L, int index);
    [[nodiscard]] static CameraRig::Zone readZone(lua_State* L, int index);
    static void readRail(lua_State* L, int index, CameraRig& rig);
    static int addZone(lua_State* L);
    static int removeZone(lua_State* L);
    static int clearZones(lua_State* L);
    static int setRail(lua_State* L);
    static int project(lua_State* L);
    static int update(lua_State* L);
    static int zone(lua_State* L);
    static int zoneCount(lua_State* L);
    static int getBlendTime(lua_State* L);
    static int setBlendTime(lua_State* L);
};

} // namespace haylen::graphics2d
