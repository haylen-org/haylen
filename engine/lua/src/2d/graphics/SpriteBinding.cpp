#include "2d/graphics/SpriteBinding.hpp"

#include <cstdint>

#include "debug/PickRegistry.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

template <typename Handle> int SpriteBinding::pushHandle(lua_State* L, const Handle& handle) {
    if (!handle.isValid()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, handle);
    return 1;
}

template <typename Handle> Handle SpriteBinding::readHandle(lua_State* L, int index) {
    return lua_isnil(L, index) ? Handle{} : lua::Stack::read<Handle>(L, index);
}

int SpriteBinding::getMaterial(lua_State* L) {
    return pushHandle(L, lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1).order.material);
}

int SpriteBinding::setMaterial(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1).order.material = readHandle<haylen::graphics2d::Material>(L, 3);
    return 0;
}

int SpriteBinding::getNormalMap(lua_State* L) {
    return pushHandle(L, lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1).order.normalMap);
}

int SpriteBinding::setNormalMap(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1).order.normalMap = readHandle<haylen::graphics::Texture>(L, 3);
    return 0;
}

int SpriteBinding::getPartMask(lua_State* L) {
    return pushHandle(L, lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1).order.partMask);
}

int SpriteBinding::setPartMask(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1).order.partMask = readHandle<haylen::graphics::Texture>(L, 3);
    return 0;
}

int SpriteBinding::draw(lua_State* L) {
    const haylen::graphics2d::Sprite& sprite = lua::Userdata::check<haylen::graphics2d::Sprite>(L, 1);
    haylen::graphics2d::Renderer& renderer = lua::Runtime::getEngine(L).getRenderer2D();
    renderer.draw(sprite);
    if (renderer.isPicking()) {
        offerPick(L, renderer, sprite);
    }

    return 0;
}

// The sprite covers the rectangle of its size around its pivot, placed by its position, rotation and scale, as the renderer draws it, and its position lives in the coordinates of the canvas.
void SpriteBinding::offerPick(lua_State* L, haylen::graphics2d::Renderer& renderer, const haylen::graphics2d::Sprite& sprite) {
    const std::uint64_t owner = lua::debug::PickRegistry::ownerOf(&sprite);
    if (renderer.isPickSearching()) {
        lua::debug::PickRegistry::remember(L, 1, owner, nullptr);
    }

    const haylen::math::Vec2 source = sprite.source.isEmpty() ? sprite.texture.getSize() : sprite.source.getSize();
    const haylen::math::Vec2 size = sprite.size.isZero() ? source : sprite.size;
    const haylen::math::Rect area{-sprite.pivot.x * size.x, -sprite.pivot.y * size.y, size.x, size.y};
    const haylen::math::Transform2D placed = haylen::math::Transform2D::compose(sprite.position, sprite.rotation, sprite.scale);
    haylen::debug::Picking::Placement placement{.position = sprite.position, .rotation = sprite.rotation, .scale = sprite.scale};
    placement.outline = {placed.apply(area.getMin()), placed.apply({area.getRight(), area.y}), placed.apply(area.getMax()), placed.apply({area.x, area.getBottom()})};
    renderer.offerPick({.owner = owner}, placement, sprite.order.layer);
}

void SpriteBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::Sprite>(L).field<&haylen::graphics2d::Sprite::texture>("texture").nestedField<&haylen::graphics2d::Sprite::position, &haylen::math::Vec2::x>("x").nestedField<&haylen::graphics2d::Sprite::position, &haylen::math::Vec2::y>("y").field<&haylen::graphics2d::Sprite::position>("position").nestedField<&haylen::graphics2d::Sprite::size, &haylen::math::Vec2::x>("width").nestedField<&haylen::graphics2d::Sprite::size, &haylen::math::Vec2::y>("height").nestedField<&haylen::graphics2d::Sprite::scale, &haylen::math::Vec2::x>("scaleX").nestedField<&haylen::graphics2d::Sprite::scale, &haylen::math::Vec2::y>("scaleY").nestedField<&haylen::graphics2d::Sprite::pivot, &haylen::math::Vec2::x>("pivotX").nestedField<&haylen::graphics2d::Sprite::pivot, &haylen::math::Vec2::y>("pivotY").field<&haylen::graphics2d::Sprite::rotation>("rotation").field<&haylen::graphics2d::Sprite::tilt>("tilt").field<&haylen::graphics2d::Sprite::color>("color").field<&haylen::graphics2d::Sprite::flash>("flash").field<&haylen::graphics2d::Sprite::source>("source").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::layer>("layer").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::depth>("depth").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::sortOffset>("sortOffset").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::sortId>("sortId").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::visibility>("visibility").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::blend>("blend").property("material", &getMaterial, &setMaterial).property("partMask", &getPartMask, &setPartMask).field<&haylen::graphics2d::Sprite::partColors>("partColors").field<&haylen::graphics2d::Sprite::effect>("effect").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::distortion>("distortion").property("normalMap", &getNormalMap, &setNormalMap).nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::specular>("specular").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::shininess>("shininess").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::emission>("emission").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::lightMask>("lightMask").nestedField<&haylen::graphics2d::Sprite::order, &haylen::graphics2d::DrawOrder::unshaded>("unshaded").nestedField<&haylen::graphics2d::Sprite::flip, &haylen::graphics2d::SpriteFlip::horizontal>("flipHorizontal").nestedField<&haylen::graphics2d::Sprite::flip, &haylen::graphics2d::SpriteFlip::vertical>("flipVertical").nestedField<&haylen::graphics2d::Sprite::flip, &haylen::graphics2d::SpriteFlip::diagonal>("flipDiagonal").function("draw", &lua::Binding::native<&draw>).install();
}

} // namespace haylen::lua::graphics2d
