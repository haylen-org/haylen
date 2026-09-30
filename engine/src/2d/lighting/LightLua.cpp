#include "2d/lighting/LightLua.hpp"

#include <vector>

#include "2d/lighting/OccluderLua.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lighting2d {

Light LightLua::read(lua_State* L, int index) {
    if (const Light* light = lua::Userdata::test<Light>(L, index)) {
        return *light;
    }
    Light light;
    if (lua_isnoneornil(L, index)) {
        return light;
    }

    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    lua::Table::checkFields(L, table, {kFields});
    lua::Table::readField(L, table, "type", light.type);
    lua::Table::readField(L, table, "position", light.position);
    lua::Table::readField(L, table, "x", light.position.x);
    lua::Table::readField(L, table, "y", light.position.y);
    lua::Table::readField(L, table, "radius", light.radius);
    lua::Table::readField(L, table, "color", light.color);
    lua::Table::readField(L, table, "intensity", light.intensity);
    lua::Table::readField(L, table, "rotation", light.rotation);
    lua::Table::readField(L, table, "scaleX", light.scale.x);
    lua::Table::readField(L, table, "scaleY", light.scale.y);
    lua::Table::readField(L, table, "texture", light.texture);
    lua::Table::readField(L, table, "innerAngle", light.innerAngle);
    lua::Table::readField(L, table, "outerAngle", light.outerAngle);
    lua::Table::readField(L, table, "height", light.height);
    lua::Table::readField(L, table, "enabled", light.enabled);
    lua::Table::readField(L, table, "blend", light.blend);
    lua::Table::readField(L, table, "itemMask", light.itemMask);
    lua::Table::readField(L, table, "layerMin", light.layerMin);
    lua::Table::readField(L, table, "layerMax", light.layerMax);
    lua::Table::readField(L, table, "shadows", light.shadows);
    lua::Table::readField(L, table, "shadowFilter", light.shadowFilter);
    lua::Table::readField(L, table, "shadowColor", light.shadowColor);
    lua::Table::readField(L, table, "shadowSmoothness", light.shadowSmoothness);
    lua::Table::readField(L, table, "shadowMask", light.shadowMask);
    return light;
}

int LightLua::newLight(lua_State* L) {
    lua::Userdata::emplace<Light>(L, read(L, 1));
    return 1;
}

int LightLua::illuminate(lua_State* L) {
    const math::Color ambient = lua::Stack::read<math::Color>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<Light> lights(static_cast<std::size_t>(luaL_len(L, 2)));
    for (std::size_t index = 0; index < lights.size(); ++index) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(index + 1));
        lights[index] = read(L, -1);
        lua_pop(L, 1);
    }

    std::uint8_t lightMask = 1;
    int layer = 0;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kIlluminateFields});
        lua::Table::readField(L, 5, "lightMask", lightMask);
        lua::Table::readField(L, 5, "layer", layer);
    }
    lua::Stack::push(L, Light::illuminate(ambient, lights, {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, lightMask, layer));
    return 1;
}

int LightLua::falloff(lua_State* L) {
    lua::Stack::push(L, Light::falloff(lua::Stack::read<float>(L, 1)));
    return 1;
}

int LightLua::getTexture(lua_State* L) {
    const Light& light = lua::Userdata::check<Light>(L, 1);
    if (!light.texture.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, light.texture);
    return 1;
}

int LightLua::setTexture(lua_State* L) {
    lua::Userdata::check<Light>(L, 1).texture = lua_isnil(L, 3) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 3);
    return 0;
}

int LightLua::affects(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Light>(L, 1).affects(lua::Stack::read<std::uint8_t>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

int LightLua::strengthAt(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Light>(L, 1).getStrengthAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

// Returns the light map value after the light draws over a color with `light:apply(color, x, y)`.
int LightLua::apply(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Light>(L, 1).apply(lua::Stack::read<math::Color>(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}));
    return 1;
}

// Tells whether occluders shadow a point with `light:shadowedAt(x, y, occluders)`, where `occluders` is a list of `Occluder` objects or tables.
int LightLua::shadowedAt(lua_State* L) {
    const Light& light = lua::Userdata::check<Light>(L, 1);
    luaL_checktype(L, 4, LUA_TTABLE);
    std::vector<Occluder> occluders(static_cast<std::size_t>(luaL_len(L, 4)));
    for (std::size_t index = 0; index < occluders.size(); ++index) {
        lua_rawgeti(L, 4, static_cast<lua_Integer>(index + 1));
        occluders[index] = OccluderLua::read(L, -1);
        lua_pop(L, 1);
    }
    lua::Stack::push(L, light.isShadowedAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, occluders));
    return 1;
}

void LightLua::install(lua_State* L) {
    lua::ClassBuilder<Light>(L).field<&Light::type>("type").nestedField<&Light::position, &math::Vec2::x>("x").nestedField<&Light::position, &math::Vec2::y>("y").field<&Light::position>("position").field<&Light::radius>("radius").field<&Light::color>("color").field<&Light::intensity>("intensity").field<&Light::rotation>("rotation").nestedField<&Light::scale, &math::Vec2::x>("scaleX").nestedField<&Light::scale, &math::Vec2::y>("scaleY").property("texture", &getTexture, &setTexture).field<&Light::innerAngle>("innerAngle").field<&Light::outerAngle>("outerAngle").field<&Light::height>("height").field<&Light::enabled>("enabled").field<&Light::blend>("blend").field<&Light::itemMask>("itemMask").field<&Light::layerMin>("layerMin").field<&Light::layerMax>("layerMax").field<&Light::shadows>("shadows").field<&Light::shadowFilter>("shadowFilter").field<&Light::shadowColor>("shadowColor").field<&Light::shadowSmoothness>("shadowSmoothness").field<&Light::shadowMask>("shadowMask").function("affects", &lua::Binding::native<&affects>).function("strengthAt", &lua::Binding::native<&strengthAt>).function("apply", &lua::Binding::native<&apply>).function("shadowedAt", &lua::Binding::native<&shadowedAt>).install();
}

} // namespace haylen::lighting2d
