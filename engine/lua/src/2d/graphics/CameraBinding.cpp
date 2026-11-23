#include "2d/graphics/CameraBinding.hpp"

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

namespace haylen::lua::graphics2d {

haylen::math::Rect CameraBinding::getScreen(lua_State* L) {
    return lua::Runtime::getEngine(L).getViewport().getVisibleRect();
}

haylen::math::Vec2 CameraBinding::readPoint(lua_State* L, int index) {
    return {lua::Stack::read<float>(L, index), lua::Stack::read<float>(L, index + 1)};
}

int CameraBinding::pushPoint(lua_State* L, haylen::math::Vec2 point) {
    lua::Stack::push(L, point.x);
    lua::Stack::push(L, point.y);
    return 2;
}

float CameraBinding::readSeconds(lua_State* L, int index) {
    const auto seconds = lua::Stack::read<float>(L, index);
    if (!(seconds >= 0.0F && std::isfinite(seconds))) {
        throw std::invalid_argument("A camera moves forward by a number of seconds that is finite and not negative.");
    }

    return seconds;
}

int CameraBinding::follow(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).follow(readPoint(L, 2), readSeconds(L, 4), getScreen(L));
    return 0;
}

int CameraBinding::update(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).update(readSeconds(L, 2));
    return 0;
}

// Frames a list of points with `frame({{x, y}, ...}, padding, dt)`.
int CameraBinding::frame(lua_State* L) {
    lua::Stack::readInto(L, 2, framedPoints);
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).frame(framedPoints, lua::Stack::read<float>(L, 3), readSeconds(L, 4), getScreen(L));
    return 0;
}

int CameraBinding::clampToLimits(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).clampToLimits(getScreen(L));
    return 0;
}

int CameraBinding::resetSmoothing(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).resetSmoothing(getScreen(L));
    return 0;
}

int CameraBinding::align(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).align(getScreen(L));
    return 0;
}

int CameraBinding::snapTo(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).snapTo(readPoint(L, 2), getScreen(L));
    return 0;
}

int CameraBinding::zoomAt(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).zoomAt(lua::Stack::read<float>(L, 2), readPoint(L, 3), getScreen(L));
    return 0;
}

int CameraBinding::flash(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).flash(lua::Stack::read<haylen::math::Color>(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

// Shakes along a direction with `shake(amount, dx, dy)`.
int CameraBinding::shake(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).shake(lua::Stack::read<float>(L, 2), readPoint(L, 3));
    return 0;
}

int CameraBinding::visibleBounds(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).visibleBounds(getScreen(L)));
    return 1;
}

int CameraBinding::viewTransform(lua_State* L) {
    const haylen::graphics2d::Camera& camera = lua::Userdata::check<haylen::graphics2d::Camera>(L, 1);
    lua::Stack::push(L, camera.viewTransform(camera.getViewRect(getScreen(L)).getSize()));
    return 1;
}

int CameraBinding::viewSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).getViewRect(getScreen(L)).getSize());
    return 1;
}

int CameraBinding::worldToScreen(lua_State* L) {
    return pushPoint(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).worldToScreen(readPoint(L, 2), getScreen(L)));
}

int CameraBinding::screenToWorld(lua_State* L) {
    return pushPoint(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).screenToWorld(readPoint(L, 2), getScreen(L)));
}

int CameraBinding::drawDebug(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).drawDebug(lua::Runtime::getEngine(L).getRenderer2D(), getScreen(L), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int CameraBinding::getX(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).position.x);
    return 1;
}

int CameraBinding::setX(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).position.x = lua::Stack::read<float>(L, 3);
    return 0;
}

int CameraBinding::getY(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).position.y);
    return 1;
}

int CameraBinding::setY(lua_State* L) {
    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).position.y = lua::Stack::read<float>(L, 3);
    return 0;
}

int CameraBinding::getDragMargins(lua_State* L) {
    const haylen::math::Insets& margins = lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).dragMargins;
    lua_createtable(L, 0, 4);
    for (const auto& [name, value] : {std::pair{"left", margins.left}, std::pair{"top", margins.top}, std::pair{"right", margins.right}, std::pair{"bottom", margins.bottom}}) {
        lua::Stack::push(L, value);
        lua_setfield(L, -2, name);
    }

    return 1;
}

int CameraBinding::getDeadZone(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).deadZone);
    return 1;
}

// The slack of the view comes from the dead zone and the drag margins, which take no negative values, so the view always has a range to rest in.
int CameraBinding::setDeadZone(lua_State* L) {
    const auto value = lua::Stack::read<haylen::math::Vec2>(L, 3);
    if (!(value.x >= 0.0F && value.y >= 0.0F)) {
        throw std::invalid_argument("The dead zone of a camera cannot be negative.");
    }

    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).deadZone = value;
    return 0;
}

int CameraBinding::setDragMargins(lua_State* L) {
    const auto value = lua::Stack::read<haylen::math::Insets>(L, 3);
    if (!(value.left >= 0.0F && value.top >= 0.0F && value.right >= 0.0F && value.bottom >= 0.0F)) {
        throw std::invalid_argument("The drag margins of a camera cannot be negative.");
    }

    lua::Userdata::check<haylen::graphics2d::Camera>(L, 1).dragMargins = value;
    return 0;
}

void CameraBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::Camera>(L).field<&haylen::graphics2d::Camera::position>("position").property("x", &getX, &setX).property("y", &getY, &setY).field<&haylen::graphics2d::Camera::offset>("offset").field<&haylen::graphics2d::Camera::anchor>("anchor").accessor<&haylen::graphics2d::Camera::getZoom, &haylen::graphics2d::Camera::setZoom>("zoom").accessor<&haylen::graphics2d::Camera::getMinZoom, &haylen::graphics2d::Camera::setMinZoom>("minZoom").accessor<&haylen::graphics2d::Camera::getMaxZoom, &haylen::graphics2d::Camera::setMaxZoom>("maxZoom").field<&haylen::graphics2d::Camera::rotation>("rotation").field<&haylen::graphics2d::Camera::ignoreRotation>("ignoreRotation").field<&haylen::graphics2d::Camera::viewport>("viewport").field<&haylen::graphics2d::Camera::limits>("limits").field<&haylen::graphics2d::Camera::limitSmoothing>("limitSmoothing").field<&haylen::graphics2d::Camera::positionSmoothing>("positionSmoothing").field<&haylen::graphics2d::Camera::positionSmoothingSpeed>("positionSmoothingSpeed").field<&haylen::graphics2d::Camera::rotationSmoothing>("rotationSmoothing").field<&haylen::graphics2d::Camera::rotationSmoothingSpeed>("rotationSmoothingSpeed").property("deadZone", &getDeadZone, &lua::Binding::native<&setDeadZone>).field<&haylen::graphics2d::Camera::dragHorizontal>("dragHorizontal").field<&haylen::graphics2d::Camera::dragVertical>("dragVertical").property("dragMargins", &getDragMargins, &lua::Binding::native<&setDragMargins>).field<&haylen::graphics2d::Camera::dragOffset>("dragOffset").field<&haylen::graphics2d::Camera::lookAheadTime>("lookAheadTime").field<&haylen::graphics2d::Camera::maxLookAhead>("maxLookAhead").field<&haylen::graphics2d::Camera::lookAheadSmoothingSpeed>("lookAheadSmoothingSpeed").field<&haylen::graphics2d::Camera::maxShakeOffset>("maxShakeOffset").field<&haylen::graphics2d::Camera::maxShakeAngle>("maxShakeAngle").field<&haylen::graphics2d::Camera::shakeFrequency>("shakeFrequency").field<&haylen::graphics2d::Camera::traumaDecay>("traumaDecay").accessor<&haylen::graphics2d::Camera::getTrauma, &haylen::graphics2d::Camera::setTrauma>("trauma").field<&haylen::graphics2d::Camera::pixelSnap>("pixelSnap").property("viewSize", &viewSize).function("follow", &lua::Binding::native<&follow>).function("frame", &lua::Binding::native<&frame>).function("update", &lua::Binding::native<&update>).function("clampToLimits", &clampToLimits).function("resetSmoothing", &resetSmoothing).function("align", &align).function("snapTo", &snapTo).function("zoomAt", &lua::Binding::native<&zoomAt>).method<&haylen::graphics2d::Camera::addTrauma>("addTrauma").function("shake", &shake).function("flash", &lua::Binding::native<&flash>).method<&haylen::graphics2d::Camera::getFlash>("flashColor").method<&haylen::graphics2d::Camera::getShakeOffset>("shakeOffset").method<&haylen::graphics2d::Camera::getRenderPosition>("renderPosition").method<&haylen::graphics2d::Camera::getRenderRotation>("renderRotation").function("viewTransform", &viewTransform).function("visibleBounds", &visibleBounds).function("worldToScreen", &worldToScreen).function("screenToWorld", &screenToWorld).function("drawDebug", &lua::Binding::native<&drawDebug>).install();
}

} // namespace haylen::lua::graphics2d
