#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::graphics2d {
struct Glass;
}

namespace haylen::lua::graphics2d {
// The function of `haylen.graphics2d` that draws glass over what lies behind it.
class GlassBinding final {
  public:
    // Draws glass with `drawGlass(rect, {blur, saturation, brightness, tint, rim, rimWidth, refraction, kind, radius, rotation, ..., layer, material, ...})`, whose options join those of the glass, those of the shape of `drawShape` without a texture and the draw order.
    static int drawGlass(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 16> kGlassFields{"blur", "saturation", "brightness", "tint", "rim", "rimWidth", "refraction", "kind", "radius", "rotation", "startAngle", "sweep", "color", "borderWidth", "borderColor", "softness"};

    static void readGlassField(lua_State* L, std::string_view key, haylen::graphics2d::Glass& glass);
};

} // namespace haylen::lua::graphics2d
