#include "2d/graphics/CameraLua.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

math::Rect CameraLua::getScreen(lua_State* L) {
    return lua::Runtime::getEngine(L).getViewport().getVisibleRect();
}

math::Vec2 CameraLua::readPoint(lua_State* L, int index) {
    return {lua::Stack::read<float>(L, index), lua::Stack::read<float>(L, index + 1)};
}

int CameraLua::pushPoint(lua_State* L, math::Vec2 point) {
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

float CameraLua::readSeconds(lua_State* L, int index) {
    const auto seconds = lua::Stack::read<float>(L, index);
    if (!(seconds >= 0.0F && std::isfinite(seconds))) {
        throw std::invalid_argument("A camera moves forward by a number of seconds that is finite and not negative.");
    }
    return seconds;
}

int CameraLua::follow(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).follow(readPoint(L, 2), readSeconds(L, 4), getScreen(L));
    return 0;
}

int CameraLua::update(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).update(readSeconds(L, 2));
    return 0;
}

// Frames a list of points with `frame({{x, y}, ...}, padding, dt)`.
int CameraLua::frame(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).frame(lua::Stack::read<std::vector<math::Vec2>>(L, 2), lua::Stack::read<float>(L, 3), readSeconds(L, 4), getScreen(L));
    return 0;
}

int CameraLua::clampToLimits(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).clampToLimits(getScreen(L));
    return 0;
}

int CameraLua::resetSmoothing(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).resetSmoothing(getScreen(L));
    return 0;
}

int CameraLua::align(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).align(getScreen(L));
    return 0;
}

int CameraLua::snapTo(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).snapTo(readPoint(L, 2), getScreen(L));
    return 0;
}

int CameraLua::zoomAt(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).zoomAt(lua::Stack::read<float>(L, 2), readPoint(L, 3), getScreen(L));
    return 0;
}

int CameraLua::flash(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).flash(lua::Stack::read<math::Color>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

// Shakes along a direction with `shake(amount, dx, dy)`.
int CameraLua::shake(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).shake(lua::Stack::read<float>(L, 2), readPoint(L, 3));
    return 0;
}

int CameraLua::visibleBounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Camera>(L, 1).visibleBounds(getScreen(L)));
    return 1;
}

int CameraLua::viewTransform(lua_State* L) {
    const Camera& camera = lua::Userdata::check<Camera>(L, 1);
    lua::Stack::push(L, camera.viewTransform(camera.getViewRect(getScreen(L)).getSize()));
    return 1;
}

int CameraLua::viewSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Camera>(L, 1).getViewRect(getScreen(L)).getSize());
    return 1;
}

int CameraLua::worldToScreen(lua_State* L) {
    return pushPoint(L, lua::Userdata::check<Camera>(L, 1).worldToScreen(readPoint(L, 2), getScreen(L)));
}

int CameraLua::screenToWorld(lua_State* L) {
    return pushPoint(L, lua::Userdata::check<Camera>(L, 1).screenToWorld(readPoint(L, 2), getScreen(L)));
}

int CameraLua::drawDebug(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).drawDebug(lua::Runtime::getEngine(L).getRenderer2D(), getScreen(L), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int CameraLua::getX(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Camera>(L, 1).position.x);
    return 1;
}

int CameraLua::setX(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).position.x = lua::Stack::read<float>(L, 3);
    return 0;
}

int CameraLua::getY(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Camera>(L, 1).position.y);
    return 1;
}

int CameraLua::setY(lua_State* L) {
    lua::Userdata::check<Camera>(L, 1).position.y = lua::Stack::read<float>(L, 3);
    return 0;
}

int CameraLua::getDragMargins(lua_State* L) {
    const math::Insets& margins = lua::Userdata::check<Camera>(L, 1).dragMargins;
    lua_createtable(L, 0, 4);
    for (const auto& [name, value] : {std::pair{"left", margins.left}, std::pair{"top", margins.top}, std::pair{"right", margins.right}, std::pair{"bottom", margins.bottom}}) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    }
    return 1;
}

int CameraLua::getDeadZone(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Camera>(L, 1).deadZone);
    return 1;
}

// The slack of the view comes from the dead zone and the drag margins, which take no negative values, so the view always has a range to rest in.
int CameraLua::setDeadZone(lua_State* L) {
    const auto value = lua::Stack::read<math::Vec2>(L, 3);
    if (!(value.x >= 0.0F && value.y >= 0.0F)) {
        throw std::invalid_argument("The dead zone of a camera cannot be negative.");
    }
    lua::Userdata::check<Camera>(L, 1).deadZone = value;
    return 0;
}

int CameraLua::setDragMargins(lua_State* L) {
    const auto value = lua::Stack::read<math::Insets>(L, 3);
    if (!(value.left >= 0.0F && value.top >= 0.0F && value.right >= 0.0F && value.bottom >= 0.0F)) {
        throw std::invalid_argument("The drag margins of a camera cannot be negative.");
    }
    lua::Userdata::check<Camera>(L, 1).dragMargins = value;
    return 0;
}

void CameraLua::install(lua_State* L) {
    lua::ClassBuilder<Camera>(L).field<&Camera::position>("position").property("x", &getX, &setX).property("y", &getY, &setY).field<&Camera::offset>("offset").field<&Camera::anchor>("anchor").accessor<&Camera::getZoom, &Camera::setZoom>("zoom").accessor<&Camera::getMinZoom, &Camera::setMinZoom>("minZoom").accessor<&Camera::getMaxZoom, &Camera::setMaxZoom>("maxZoom").field<&Camera::rotation>("rotation").field<&Camera::ignoreRotation>("ignoreRotation").field<&Camera::viewport>("viewport").field<&Camera::limits>("limits").field<&Camera::limitSmoothing>("limitSmoothing").field<&Camera::positionSmoothing>("positionSmoothing").field<&Camera::positionSmoothingSpeed>("positionSmoothingSpeed").field<&Camera::rotationSmoothing>("rotationSmoothing").field<&Camera::rotationSmoothingSpeed>("rotationSmoothingSpeed").property("deadZone", &getDeadZone, &lua::Binding::native<&setDeadZone>).field<&Camera::dragHorizontal>("dragHorizontal").field<&Camera::dragVertical>("dragVertical").property("dragMargins", &getDragMargins, &lua::Binding::native<&setDragMargins>).field<&Camera::dragOffset>("dragOffset").field<&Camera::lookAheadTime>("lookAheadTime").field<&Camera::maxLookAhead>("maxLookAhead").field<&Camera::lookAheadSmoothingSpeed>("lookAheadSmoothingSpeed").field<&Camera::maxShakeOffset>("maxShakeOffset").field<&Camera::maxShakeAngle>("maxShakeAngle").field<&Camera::shakeFrequency>("shakeFrequency").field<&Camera::traumaDecay>("traumaDecay").accessor<&Camera::getTrauma, &Camera::setTrauma>("trauma").field<&Camera::pixelSnap>("pixelSnap").property("viewSize", &viewSize).function("follow", &lua::Binding::native<&follow>).function("frame", &lua::Binding::native<&frame>).function("update", &lua::Binding::native<&update>).function("clampToLimits", &clampToLimits).function("resetSmoothing", &resetSmoothing).function("align", &align).function("snapTo", &snapTo).function("zoomAt", &lua::Binding::native<&zoomAt>).method<&Camera::addTrauma>("addTrauma").function("shake", &shake).function("flash", &lua::Binding::native<&flash>).method<&Camera::getFlash>("flashColor").method<&Camera::getShakeOffset>("shakeOffset").method<&Camera::getRenderPosition>("renderPosition").method<&Camera::getRenderRotation>("renderRotation").function("viewTransform", &viewTransform).function("visibleBounds", &visibleBounds).function("worldToScreen", &worldToScreen).function("screenToWorld", &screenToWorld).function("drawDebug", &lua::Binding::native<&drawDebug>).install();
}

} // namespace haylen::graphics2d
