#include "ui/TransformBinding.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::ui {

void TransformBinding::push(lua_State* L, const std::shared_ptr<haylen::ui::Transform>& transform) {
    lua::Userdata::emplace<haylen::ui::Transform>(L, transform);
}

void TransformBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::ui::Transform>(L).checkedField<&haylen::ui::Transform::offset, &checkOffset>("offset").checkedField<&haylen::ui::Transform::scale, &checkScale>("scale").checkedField<&haylen::ui::Transform::opacity, &checkOpacity>("opacity").checkedField<&haylen::ui::Transform::tint, &checkTint>("tint").install();
}

void TransformBinding::checkOffset(const haylen::math::Vec2& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("The offset of a UI transform needs finite coordinates.");
    }
}

void TransformBinding::checkScale(const haylen::math::Vec2& value) {
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) {
        throw std::invalid_argument("The scale of a UI transform needs finite coordinates.");
    }
}

void TransformBinding::checkOpacity(float value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("The opacity of a UI transform is a finite number.");
    }
}

void TransformBinding::checkTint(const haylen::math::Color& value) {
    if (!std::isfinite(value.r) || !std::isfinite(value.g) || !std::isfinite(value.b) || !std::isfinite(value.a)) {
        throw std::invalid_argument("The tint of a UI transform needs finite channels.");
    }
}

} // namespace haylen::lua::ui
