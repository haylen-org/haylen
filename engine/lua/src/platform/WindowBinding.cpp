#include "platform/WindowBinding.hpp"

#include <lua.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/Displays.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/FoldSimulation.hpp"
#include "haylen/platform/FullscreenTarget.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/WindowPlacement.hpp"
#include "platform/SystemAnswerBinding.hpp"

namespace haylen::lua::platform {

// A region is a rectangle when it is a `Rect`, has an `x` field or holds numbers, and a polygon when it holds points.
bool WindowBinding::isRect(lua_State* L, int index) {
    if (lua::Userdata::test<haylen::math::Rect>(L, index) != nullptr) {
        return true;
    }

    lua_getfield(L, index, "x");
    lua_rawgeti(L, index, 1);
    const bool rect = !lua_isnil(L, -2) || lua_type(L, -1) == LUA_TNUMBER;
    lua_pop(L, 2);
    return rect;
}

haylen::math::Polygon::Outline WindowBinding::readRegion(lua_State* L, int index) {
    if (!lua::Userdata::test<haylen::math::Rect>(L, index) && !lua_istable(L, index)) {
        luaL_error(L, "A mouse passthrough region is a rectangle or a polygon, not a %s.", luaL_typename(L, index));
    }

    if (isRect(L, index)) {
        const haylen::math::Rect rect = lua::Stack::read<haylen::math::Rect>(L, index);
        return {rect.getMin(), {rect.getRight(), rect.y}, rect.getMax(), {rect.x, rect.getBottom()}};
    }

    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, index));
    if (count < 3) {
        luaL_error(L, "A mouse passthrough polygon needs at least three points.");
    }

    haylen::math::Polygon::Outline outline;
    outline.reserve(static_cast<std::size_t>(count));
    for (lua_Integer point = 1; point <= count; ++point) {
        lua_rawgeti(L, index, point);
        outline.push_back(lua::Stack::read<haylen::math::Vec2>(L, -1));
        lua_pop(L, 1);
    }

    return outline;
}

haylen::math::Rect WindowBinding::toDesign(const haylen::graphics::Viewport& viewport, const haylen::math::Rect& pixels) {
    return haylen::math::Rect::fromMinMax(viewport.toDesign(pixels.getMin()), viewport.toDesign(pixels.getMax()));
}

int WindowBinding::framebufferSize(lua_State* L) {
    const haylen::math::Vec2 value = lua::Runtime::getEngine(L).getWindow().getFramebufferSize();
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int WindowBinding::dpiScale(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getDpiScale());
    return 1;
}

int WindowBinding::fullscreen(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isFullscreen());
    return 1;
}

// A mode is a table with its size in pixels and an optional refresh rate, such as one of the modes of `display.list()`, whose scale it ignores.
haylen::platform::Display::Mode WindowBinding::readMode(lua_State* L, int index) {
    luaL_argexpected(L, lua_istable(L, index), 2, "table with the width and the height of a mode");
    lua::Table::checkFields(L, index, {kModeFields});
    haylen::platform::Display::Mode mode{};
    lua::Table::readField(L, index, "width", mode.width);
    lua::Table::readField(L, index, "height", mode.height);
    lua::Table::readField(L, index, "refreshRate", mode.refreshRate);
    if (mode.width <= 0 || mode.height <= 0) {
        luaL_error(L, "A fullscreen mode needs a positive \"width\" and \"height\" in pixels.");
    }

    return mode;
}

// Takes `setFullscreen(enabled, {display, mode})`, where the options choose the display that the window covers and a mode of it for exclusive fullscreen, and returns a promise of the answer.
int WindowBinding::setFullscreen(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const bool enabled = lua::Stack::read<bool>(L, 1);
    if (lua_isnoneornil(L, 2)) {
        engine.getWindow().setFullscreen(enabled, awaitAnswer(L));
        return 1;
    }

    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kFullscreenOptions});
    luaL_argcheck(L, enabled, 2, "options only go with true");
    haylen::platform::Window::Fullscreen target;
    lua::Table::readField(L, 2, "display", target.display);
    lua_getfield(L, 2, "mode");
    if (!lua_isnil(L, -1)) {
        target.mode = readMode(L, lua_gettop(L));
    }

    lua_pop(L, 1);
    // A display that is not connected or a mode that it does not list raises before the call waits for an answer.
    static_cast<void>(haylen::platform::FullscreenTarget::resolve(engine.getDisplays().getAll(), engine.getWindow().getDisplayId(), target));
    engine.getWindow().enterFullscreen(target, awaitAnswer(L));
    return 1;
}

// Pushes a promise that the answer of a call of the window settles at the next frame, and returns what the window calls with it.
std::function<void(haylen::platform::SystemAnswer)> WindowBinding::awaitAnswer(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    promise.push(L);
    return engine.getSystem().awaitAnswer([&engine, promise](const haylen::platform::SystemAnswer& answer) { SystemAnswerBinding::settle(engine, promise, answer); });
}

int WindowBinding::resizable(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isResizable());
    return 1;
}

int WindowBinding::setResizable(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setResizable(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::setTitle(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setTitle(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int WindowBinding::setCursor(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setCursor(lua::Stack::read<haylen::platform::Window::Cursor>(L, 1));
    return 0;
}

int WindowBinding::setCursorVisible(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setCursorVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::setMouseLocked(lua_State* L) {
    const bool locked = lua::Stack::read<bool>(L, 1);
    lua::Runtime::getEngine(L).getWindow().setMouseLocked(locked, awaitAnswer(L));
    return 1;
}

int WindowBinding::setKeyboardVisible(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setKeyboardVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::orientation(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getOrientation());
    return 1;
}

int WindowBinding::lockOrientation(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().lockOrientation(lua::Stack::read<haylen::platform::Orientation>(L, 1));
    return 0;
}

// Returns the fold across the window as a table with its `bounds` in design units, its `axis`, its `state` and whether it is `separating` and `occluding`, or `nil` without one.
int WindowBinding::fold(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const std::optional<haylen::platform::Fold> value = engine.getFold();
    if (!value) {
        lua_pushnil(L);
        return 1;
    }

    lua_createtable(L, 0, 5);
    lua::Stack::push(L, toDesign(engine.getViewport(), value->bounds));
    lua_setfield(L, -2, "bounds");
    lua::Stack::push(L, haylen::platform::Fold::axisName(value->axis));
    lua_setfield(L, -2, "axis");
    lua::Stack::push(L, haylen::platform::Fold::stateName(value->state));
    lua_setfield(L, -2, "state");
    lua::Stack::push(L, value->separating);
    lua_setfield(L, -2, "separating");
    lua::Stack::push(L, value->occluding);
    lua_setfield(L, -2, "occluding");
    return 1;
}

int WindowBinding::posture(lua_State* L) {
    const std::optional<haylen::platform::Fold> value = lua::Runtime::getEngine(L).getFold();
    lua::Stack::push(L, haylen::platform::Fold::postureName(value ? value->getPosture() : haylen::platform::Fold::Posture::Flat));
    return 1;
}

// Returns the areas of the window on each side of a separating fold as `Rect` values in design units, the left or top one first, or the whole window as the only one.
int WindowBinding::segments(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const haylen::math::Vec2 size = engine.getWindow().getFramebufferSize();
    const std::optional<haylen::platform::Fold> value = engine.getFold();
    const std::vector<haylen::math::Rect> areas = value ? value->getSegments(size) : std::vector<haylen::math::Rect>{{0.0F, 0.0F, size.x, size.y}};
    lua_createtable(L, static_cast<int>(areas.size()), 0);
    for (std::size_t index = 0; index < areas.size(); ++index) {
        lua::Stack::push(L, toDesign(engine.getViewport(), areas[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Returns the simulated fold as a preset name or a table of its axis, state and hinge, or `nil` while the device reports its own.
int WindowBinding::foldSimulation(lua_State* L) {
    const std::optional<haylen::platform::FoldSimulation>& simulation = lua::Runtime::getEngine(L).getFoldSimulation();
    if (!simulation) {
        lua_pushnil(L);
        return 1;
    }

    lua::JsonConverter::push(L, simulation->toJson());
    return 1;
}

// Simulates a fold by preset name or by a table of its axis, state and hinge with `setFoldSimulation(value)`, and `nil` goes back to the fold of the device.
int WindowBinding::setFoldSimulation(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    if (lua_isnoneornil(L, 1)) {
        engine.setFoldSimulation(std::nullopt);
        return 0;
    }

    engine.setFoldSimulation(haylen::platform::FoldSimulation::fromJson(lua::JsonConverter::read(L, 1)));
    return 0;
}

int WindowBinding::hasPointerDevice(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().hasPointerDevice());
    return 1;
}

int WindowBinding::statusBar(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getStatusBar());
    return 1;
}

int WindowBinding::setStatusBar(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setStatusBar(lua::Stack::read<haylen::platform::Window::StatusBar>(L, 1));
    return 0;
}

int WindowBinding::statusBarColor(lua_State* L) {
    const std::optional<haylen::math::Color> color = lua::Runtime::getEngine(L).getStatusBarColor();
    if (color) {
        lua::Stack::push(L, *color);
    } else {
        lua_pushnil(L);
    }

    return 1;
}

int WindowBinding::setStatusBarColor(lua_State* L) {
    lua::Runtime::getEngine(L).setStatusBarColor(lua_isnoneornil(L, 1) ? std::nullopt : std::optional(lua::Stack::read<haylen::math::Color>(L, 1)));
    return 0;
}

int WindowBinding::systemGestures(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getSystemGestures());
    return 1;
}

int WindowBinding::setSystemGestures(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setSystemGestures(lua::Stack::read<haylen::platform::Window::SystemGestures>(L, 1));
    return 0;
}

int WindowBinding::redraw(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getRedraw());
    return 1;
}

int WindowBinding::setRedraw(lua_State* L) {
    lua::Runtime::getEngine(L).setRedraw(lua::Stack::read<haylen::platform::Window::Redraw>(L, 1));
    return 0;
}

int WindowBinding::requestRedraw(lua_State* L) {
    lua::Runtime::getEngine(L).requestRedraw();
    return 0;
}

int WindowBinding::backLeavesApp(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).canBackLeaveApp());
    return 1;
}

int WindowBinding::setBackLeavesApp(lua_State* L) {
    lua::Runtime::getEngine(L).setBackLeavesApp(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::keepScreenOn(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isScreenKeptOn());
    return 1;
}

int WindowBinding::setKeepScreenOn(lua_State* L) {
    const bool kept = lua::Stack::read<bool>(L, 1);
    lua::Runtime::getEngine(L).getWindow().setKeepScreenOn(kept, awaitAnswer(L));
    return 1;
}

int WindowBinding::frameRate(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getFrameRate());
    return 1;
}

int WindowBinding::setFrameRate(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setFrameRate(static_cast<int>(luaL_checkinteger(L, 1)));
    return 0;
}

int WindowBinding::maxPixelDensity(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getMaxPixelDensity());
    return 1;
}

int WindowBinding::setMaxPixelDensity(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setMaxPixelDensity(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

int WindowBinding::canBeTransparent(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().canBeTransparent());
    return 1;
}

int WindowBinding::transparent(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isTransparent());
    return 1;
}

int WindowBinding::setTransparent(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setTransparent(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::decorated(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isDecorated());
    return 1;
}

int WindowBinding::setDecorated(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setDecorated(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::alwaysOnTop(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isAlwaysOnTop());
    return 1;
}

int WindowBinding::setAlwaysOnTop(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setAlwaysOnTop(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::showInTaskbar(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isShownInTaskbar());
    return 1;
}

int WindowBinding::setShowInTaskbar(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setShowInTaskbar(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::focusable(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isFocusable());
    return 1;
}

int WindowBinding::setFocusable(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setFocusable(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowBinding::frame(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getFrame());
    return 1;
}

int WindowBinding::setFrame(lua_State* L) {
    const haylen::math::Rect value{lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    luaL_argcheck(L, value.width > 0.0F, 3, "positive width expected");
    luaL_argcheck(L, value.height > 0.0F, 4, "positive height expected");
    lua::Runtime::getEngine(L).getWindow().setFrame(value);
    return 0;
}

// Moves the window with `place(position)`, where the position reads like the one of `haylen.json` and keeps the size of the window unless it fills the area.
int WindowBinding::place(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const haylen::platform::WindowPlacement placement = haylen::platform::WindowPlacement::fromJson(lua::JsonConverter::read(L, 1));
    haylen::platform::Window& window = engine.getWindow();
    window.setFrame(placement.resolve(engine.getDisplays().getAll(), window.getFrame().getSize()));
    return 0;
}

int WindowBinding::mousePassthrough(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getMousePassthrough());
    return 1;
}

// Takes `false`, `true` for the whole window, or a list of regions that keep the mouse, in design units that follow the viewport of the moment or in framebuffer pixels when the second argument is `pixels`.
int WindowBinding::setMousePassthrough(lua_State* L) {
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    if (lua_isboolean(L, 1)) {
        engine.getWindow().setMousePassthrough(lua_toboolean(L, 1) != 0 ? haylen::platform::Window::Passthrough::Whole : haylen::platform::Window::Passthrough::Off, {});
        return 0;
    }

    luaL_argexpected(L, lua_istable(L, 1), 1, "boolean or table of regions");
    const std::string_view units = lua_isnoneornil(L, 2) ? "design" : lua::Stack::read<std::string_view>(L, 2);
    luaL_argcheck(L, units == "design" || units == "pixels", 2, "'design' or 'pixels' expected");

    std::vector<haylen::math::Polygon::Outline> regions;
    const lua_Integer count = static_cast<lua_Integer>(lua_rawlen(L, 1));
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, 1, index);
        regions.push_back(readRegion(L, lua_gettop(L)));
        lua_pop(L, 1);
    }

    if (units == "design") {
        const haylen::graphics::Viewport& viewport = engine.getViewport();
        for (haylen::math::Polygon::Outline& region : regions) {
            for (haylen::math::Vec2& point : region) {
                point = viewport.toFramebuffer(point);
            }
        }
    }

    engine.getWindow().setMousePassthrough(haylen::platform::Window::Passthrough::Regions, regions);
    return 0;
}

int WindowBinding::startDrag(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().startDrag();
    return 0;
}

int WindowBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"framebufferSize", &framebufferSize}, {"dpiScale", &dpiScale}, {"fullscreen", &fullscreen}, {"setFullscreen", &lua::Binding::native<&setFullscreen>}, {"resizable", &resizable}, {"setResizable", &lua::Binding::native<&setResizable>}, {"setTitle", &lua::Binding::native<&setTitle>}, {"setCursor", &lua::Binding::native<&setCursor>}, {"setCursorVisible", &lua::Binding::native<&setCursorVisible>}, {"setMouseLocked", &lua::Binding::native<&setMouseLocked>}, {"setKeyboardVisible", &lua::Binding::native<&setKeyboardVisible>}, {"orientation", &orientation}, {"lockOrientation", &lua::Binding::native<&lockOrientation>}, {"statusBar", &statusBar}, {"setStatusBar", &lua::Binding::native<&setStatusBar>}, {"statusBarColor", &lua::Binding::native<&statusBarColor>}, {"setStatusBarColor", &lua::Binding::native<&setStatusBarColor>}, {"systemGestures", &systemGestures}, {"setSystemGestures", &lua::Binding::native<&setSystemGestures>}, {"redraw", &redraw}, {"setRedraw", &lua::Binding::native<&setRedraw>}, {"requestRedraw", &lua::Binding::native<&requestRedraw>}, {"fold", &lua::Binding::native<&fold>}, {"posture", &lua::Binding::native<&posture>}, {"segments", &lua::Binding::native<&segments>}, {"foldSimulation", &lua::Binding::native<&foldSimulation>}, {"setFoldSimulation", &lua::Binding::native<&setFoldSimulation>}, {"hasPointerDevice", &hasPointerDevice}, {"backLeavesApp", &backLeavesApp}, {"setBackLeavesApp", &lua::Binding::native<&setBackLeavesApp>}, {"keepScreenOn", &keepScreenOn}, {"setKeepScreenOn", &lua::Binding::native<&setKeepScreenOn>}, {"frameRate", &frameRate}, {"setFrameRate", &lua::Binding::native<&setFrameRate>}, {"maxPixelDensity", &maxPixelDensity}, {"setMaxPixelDensity", &lua::Binding::native<&setMaxPixelDensity>}, {"canBeTransparent", &canBeTransparent}, {"transparent", &transparent}, {"setTransparent", &lua::Binding::native<&setTransparent>}, {"decorated", &decorated}, {"setDecorated", &lua::Binding::native<&setDecorated>}, {"alwaysOnTop", &alwaysOnTop}, {"setAlwaysOnTop", &lua::Binding::native<&setAlwaysOnTop>}, {"showInTaskbar", &showInTaskbar}, {"setShowInTaskbar", &lua::Binding::native<&setShowInTaskbar>}, {"focusable", &focusable}, {"setFocusable", &lua::Binding::native<&setFocusable>}, {"frame", &lua::Binding::native<&frame>}, {"setFrame", &lua::Binding::native<&setFrame>}, {"place", &lua::Binding::native<&place>}, {"mousePassthrough", &mousePassthrough}, {"setMousePassthrough", &lua::Binding::native<&setMousePassthrough>}, {"startDrag", &lua::Binding::native<&startDrag>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void WindowBinding::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.window", &open);
}

} // namespace haylen::lua::platform
