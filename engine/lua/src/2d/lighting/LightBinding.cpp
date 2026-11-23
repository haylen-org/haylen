#include "2d/lighting/LightBinding.hpp"

#include <vector>

#include "2d/lighting/OccluderBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::lighting2d {

haylen::lighting2d::Light LightBinding::read(lua_State* L, int index) {
    if (const haylen::lighting2d::Light* light = lua::Userdata::test<haylen::lighting2d::Light>(L, index)) {
        return *light;
    }

    haylen::lighting2d::Light light;
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

int LightBinding::newLight(lua_State* L) {
    lua::Userdata::emplace<haylen::lighting2d::Light>(L, read(L, 1));
    return 1;
}

int LightBinding::illuminate(lua_State* L) {
    const haylen::math::Color ambient = lua::Stack::read<haylen::math::Color>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    std::vector<haylen::lighting2d::Light> lights(lua_rawlen(L, 2));
    for (std::size_t index = 0; index < lights.size(); ++index) {
        lua_rawgeti(L, 2, static_cast<lua_Integer>(index + 1));
        lights[index] = read(L, -1);
        lights[index].validate();
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

    lua::Stack::push(L, haylen::lighting2d::Light::illuminate(ambient, lights, {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}, lightMask, layer));
    return 1;
}

int LightBinding::falloff(lua_State* L) {
    lua::Stack::push(L, haylen::lighting2d::Light::falloff(lua::Stack::read<float>(L, 1)));
    return 1;
}

int LightBinding::getTexture(lua_State* L) {
    const haylen::lighting2d::Light& light = lua::Userdata::check<haylen::lighting2d::Light>(L, 1);
    if (!light.texture.isValid()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, light.texture);
    return 1;
}

int LightBinding::setTexture(lua_State* L) {
    lua::Userdata::check<haylen::lighting2d::Light>(L, 1).texture = lua_isnil(L, 3) ? haylen::graphics::Texture{} : lua::Stack::read<haylen::graphics::Texture>(L, 3);
    return 0;
}

int LightBinding::affects(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::lighting2d::Light>(L, 1).affects(lua::Stack::read<std::uint8_t>(L, 2), lua::Stack::read<int>(L, 3)));
    return 1;
}

// The queries check the light first, as drawing it does, so a light that could never draw answers no number.
int LightBinding::strengthAt(lua_State* L) {
    const haylen::lighting2d::Light& light = lua::Userdata::check<haylen::lighting2d::Light>(L, 1);
    light.validate();
    lua::Stack::push(L, light.getStrengthAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}));
    return 1;
}

// Returns the light map value after the light draws over a color with `light:apply(color, x, y)`.
int LightBinding::apply(lua_State* L) {
    const haylen::lighting2d::Light& light = lua::Userdata::check<haylen::lighting2d::Light>(L, 1);
    light.validate();
    lua::Stack::push(L, light.apply(lua::Stack::read<haylen::math::Color>(L, 2), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)}));
    return 1;
}

// Tells whether occluders shadow a point with `light:shadowedAt(x, y, occluders)`, where `occluders` is a list of `Occluder` objects or tables.
int LightBinding::shadowedAt(lua_State* L) {
    const haylen::lighting2d::Light& light = lua::Userdata::check<haylen::lighting2d::Light>(L, 1);
    light.validate();
    luaL_checktype(L, 4, LUA_TTABLE);
    std::vector<haylen::lighting2d::Occluder> occluders(lua_rawlen(L, 4));
    for (std::size_t index = 0; index < occluders.size(); ++index) {
        lua_rawgeti(L, 4, static_cast<lua_Integer>(index + 1));
        OccluderBinding::read(L, -1, occluders[index]);
        lua_pop(L, 1);
    }

    lua::Stack::push(L, light.isShadowedAt({lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)}, occluders));
    return 1;
}

void LightBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::lighting2d::Light>(L).field<&haylen::lighting2d::Light::type>("type").nestedField<&haylen::lighting2d::Light::position, &haylen::math::Vec2::x>("x").nestedField<&haylen::lighting2d::Light::position, &haylen::math::Vec2::y>("y").field<&haylen::lighting2d::Light::position>("position").field<&haylen::lighting2d::Light::radius>("radius").field<&haylen::lighting2d::Light::color>("color").field<&haylen::lighting2d::Light::intensity>("intensity").field<&haylen::lighting2d::Light::rotation>("rotation").nestedField<&haylen::lighting2d::Light::scale, &haylen::math::Vec2::x>("scaleX").nestedField<&haylen::lighting2d::Light::scale, &haylen::math::Vec2::y>("scaleY").property("texture", &getTexture, &setTexture).field<&haylen::lighting2d::Light::innerAngle>("innerAngle").field<&haylen::lighting2d::Light::outerAngle>("outerAngle").field<&haylen::lighting2d::Light::height>("height").field<&haylen::lighting2d::Light::enabled>("enabled").field<&haylen::lighting2d::Light::blend>("blend").field<&haylen::lighting2d::Light::itemMask>("itemMask").field<&haylen::lighting2d::Light::layerMin>("layerMin").field<&haylen::lighting2d::Light::layerMax>("layerMax").field<&haylen::lighting2d::Light::shadows>("shadows").field<&haylen::lighting2d::Light::shadowFilter>("shadowFilter").field<&haylen::lighting2d::Light::shadowColor>("shadowColor").field<&haylen::lighting2d::Light::shadowSmoothness>("shadowSmoothness").field<&haylen::lighting2d::Light::shadowMask>("shadowMask").function("affects", &lua::Binding::native<&affects>).function("strengthAt", &lua::Binding::native<&strengthAt>).function("apply", &lua::Binding::native<&apply>).function("shadowedAt", &lua::Binding::native<&shadowedAt>).install();
}

} // namespace haylen::lua::lighting2d
