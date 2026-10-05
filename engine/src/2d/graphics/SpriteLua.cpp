#include "2d/graphics/SpriteLua.hpp"

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

int SpriteLua::getMaterial(lua_State* L) {
    const Material& material = lua::Userdata::check<Sprite>(L, 1).order.material;
    if (!material.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, material);
    return 1;
}

int SpriteLua::setMaterial(lua_State* L) {
    lua::Userdata::check<Sprite>(L, 1).order.material = lua_isnil(L, 3) ? Material{} : lua::Stack::read<Material>(L, 3);
    return 0;
}

int SpriteLua::getNormalMap(lua_State* L) {
    const graphics::Texture& normalMap = lua::Userdata::check<Sprite>(L, 1).order.normalMap;
    if (!normalMap.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, normalMap);
    return 1;
}

int SpriteLua::setNormalMap(lua_State* L) {
    lua::Userdata::check<Sprite>(L, 1).order.normalMap = lua_isnil(L, 3) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 3);
    return 0;
}

int SpriteLua::getPartMask(lua_State* L) {
    const graphics::Texture& partMask = lua::Userdata::check<Sprite>(L, 1).order.partMask;
    if (!partMask.isValid()) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, partMask);
    return 1;
}

int SpriteLua::setPartMask(lua_State* L) {
    lua::Userdata::check<Sprite>(L, 1).order.partMask = lua_isnil(L, 3) ? graphics::Texture{} : lua::Stack::read<graphics::Texture>(L, 3);
    return 0;
}

int SpriteLua::draw(lua_State* L) {
    lua::Runtime::getEngine(L).getRenderer2D().draw(lua::Userdata::check<Sprite>(L, 1));
    return 0;
}

void SpriteLua::install(lua_State* L) {
    lua::ClassBuilder<Sprite>(L).field<&Sprite::texture>("texture").nestedField<&Sprite::position, &math::Vec2::x>("x").nestedField<&Sprite::position, &math::Vec2::y>("y").field<&Sprite::position>("position").nestedField<&Sprite::size, &math::Vec2::x>("width").nestedField<&Sprite::size, &math::Vec2::y>("height").nestedField<&Sprite::scale, &math::Vec2::x>("scaleX").nestedField<&Sprite::scale, &math::Vec2::y>("scaleY").nestedField<&Sprite::pivot, &math::Vec2::x>("pivotX").nestedField<&Sprite::pivot, &math::Vec2::y>("pivotY").field<&Sprite::rotation>("rotation").field<&Sprite::color>("color").field<&Sprite::flash>("flash").field<&Sprite::source>("source").nestedField<&Sprite::order, &DrawOrder::layer>("layer").nestedField<&Sprite::order, &DrawOrder::depth>("depth").nestedField<&Sprite::order, &DrawOrder::sortOffset>("sortOffset").nestedField<&Sprite::order, &DrawOrder::visibility>("visibility").nestedField<&Sprite::order, &DrawOrder::blend>("blend").property("material", &getMaterial, &setMaterial).property("partMask", &getPartMask, &setPartMask).field<&Sprite::partColors>("partColors").property("normalMap", &getNormalMap, &setNormalMap).nestedField<&Sprite::order, &DrawOrder::specular>("specular").nestedField<&Sprite::order, &DrawOrder::shininess>("shininess").nestedField<&Sprite::order, &DrawOrder::emission>("emission").nestedField<&Sprite::order, &DrawOrder::lightMask>("lightMask").nestedField<&Sprite::order, &DrawOrder::unshaded>("unshaded").nestedField<&Sprite::flip, &SpriteFlip::horizontal>("flipHorizontal").nestedField<&Sprite::flip, &SpriteFlip::vertical>("flipVertical").nestedField<&Sprite::flip, &SpriteFlip::diagonal>("flipDiagonal").function("draw", &lua::Binding::native<&draw>).install();
}

} // namespace haylen::graphics2d
