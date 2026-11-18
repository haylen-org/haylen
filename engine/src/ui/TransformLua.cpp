#include "ui/TransformLua.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::ui {

void TransformLua::push(lua_State* L, const std::shared_ptr<Transform>& transform) {
    lua::Userdata::emplace<Transform>(L, transform);
}

void TransformLua::install(lua_State* L) {
    lua::ClassBuilder<Transform>(L).checkedField<&Transform::offset, &checkOffset>("offset").checkedField<&Transform::scale, &checkScale>("scale").checkedField<&Transform::opacity, &checkOpacity>("opacity").checkedField<&Transform::tint, &checkTint>("tint").install();
}

void TransformLua::checkOffset(const math::Vec2& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("The offset of a UI transform needs finite coordinates.");
    }
}

void TransformLua::checkScale(const math::Vec2& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("The scale of a UI transform needs finite coordinates.");
    }
}

void TransformLua::checkOpacity(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("The opacity of a UI transform is a finite number.");
    }
}

void TransformLua::checkTint(const math::Color& value) {
    if (!std::isfinite(value.r) || !std::isfinite(value.g) || !std::isfinite(value.b) || !std::isfinite(value.a)) {
        throw std::invalid_argument("The tint of a UI transform needs finite channels.");
    }
}

} // namespace haylen::ui
