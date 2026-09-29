#pragma once

#include <lua.hpp>

#include <array>
#include <optional>
#include <string_view>

#include "haylen/2d/lighting/Light.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Type.hpp"

namespace haylen::lua {

template <> struct Type<lighting2d::Light> {
    static constexpr const char* name = "haylen.Light";
    using Storage = lighting2d::Light;
};

template <> struct EnumNames<lighting2d::Light::Type> {
    static std::optional<lighting2d::Light::Type> fromName(std::string_view name) {
        return lighting2d::Light::typeFromName(name);
    }
    static std::string_view name(lighting2d::Light::Type value) {
        return lighting2d::Light::typeName(value);
    }
};

template <> struct EnumNames<lighting2d::Light::Blend> {
    static std::optional<lighting2d::Light::Blend> fromName(std::string_view name) {
        return lighting2d::Light::blendFromName(name);
    }
    static std::string_view name(lighting2d::Light::Blend value) {
        return lighting2d::Light::blendName(value);
    }
};

template <> struct EnumNames<lighting2d::Light::ShadowFilter> {
    static std::optional<lighting2d::Light::ShadowFilter> fromName(std::string_view name) {
        return lighting2d::Light::shadowFilterFromName(name);
    }
    static std::string_view name(lighting2d::Light::ShadowFilter value) {
        return lighting2d::Light::shadowFilterName(value);
    }
};

} // namespace haylen::lua

namespace haylen::lighting2d {

// Installs the Light class of haylen.lighting2d, whose properties write straight into the light that graphics2d.drawLight draws.
class LightLua final {
  public:
    static constexpr std::array<std::string_view, 24> kFields{"type", "x", "y", "radius", "color", "intensity", "rotation", "scaleX", "scaleY", "texture", "innerAngle", "outerAngle", "height", "enabled", "blend", "itemMask", "layerMin", "layerMax", "shadows", "shadowFilter", "shadowColor", "shadowSmoothness", "shadowMask", "position"};

    static void install(lua_State* L);

    // Reads a Light, or a table with its properties over the defaults, at index.
    [[nodiscard]] static Light read(lua_State* L, int index);

    // Creates a light with newLight({type = 'spot', x = 10, y = 20, radius = 200, ...}).
    static int newLight(lua_State* L);

    // Returns the light map value at x and y with illuminate(ambient, lights, x, y, {lightMask, layer}).
    static int illuminate(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kIlluminateFields{"lightMask", "layer"};

    static int getTexture(lua_State* L);
    static int setTexture(lua_State* L);
    static int affects(lua_State* L);
    static int strengthAt(lua_State* L);
    static int apply(lua_State* L);
    static int shadowedAt(lua_State* L);
};

} // namespace haylen::lighting2d
