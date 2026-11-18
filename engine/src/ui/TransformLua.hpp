#pragma once

#include <memory>

#include "haylen/lua/Type.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Transform.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<ui::Transform> {
    static constexpr const char* name = "haylen.UiTransform";
    using Storage = std::weak_ptr<ui::Transform>;
};

} // namespace haylen::lua

namespace haylen::ui {

// Installs the `UiTransform` class, the handle to the transform of a GUI node. Its offset, scale, opacity and tint are native properties, so tweens animate them without running Lua, and the handle reads as released once its node is gone.
class TransformLua final {
  public:
    static void install(lua_State* L);
    static void push(lua_State* L, const std::shared_ptr<Transform>& transform);

  private:
    static void checkOffset(const math::Vec2& value);
    static void checkScale(const math::Vec2& value);
    static void checkOpacity(float value);
    static void checkTint(const math::Color& value);
};

} // namespace haylen::ui
