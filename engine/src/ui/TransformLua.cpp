#include "ui/TransformLua.hpp"

#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::ui {

void TransformLua::push(lua_State* L, const std::shared_ptr<Transform>& transform) {
    lua::Userdata::emplace<Transform>(L, transform);
}

void TransformLua::install(lua_State* L) {
    lua::ClassBuilder<Transform>(L).field<&Transform::offset>("offset").field<&Transform::scale>("scale").field<&Transform::opacity>("opacity").field<&Transform::tint>("tint").install();
}

} // namespace haylen::ui
