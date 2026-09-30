#include "platform/WindowLua.hpp"

#include <lua.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/WindowPlacement.hpp"

namespace haylen::platform {

// A region is a rectangle when it is a `Rect`, has an `x` field or holds numbers, and a polygon when it holds points.
bool WindowLua::isRect(lua_State* L, int index) {
    if (lua::Userdata::test<math::Rect>(L, index) != nullptr) {
        return true;
    }
    lua_getfield(L, index, "x");
    lua_rawgeti(L, index, 1);
    const bool rect = !lua_isnil(L, -2) || lua_type(L, -1) == LUA_TNUMBER;
    lua_pop(L, 2);
    return rect;
}

math::Polygon::Outline WindowLua::readRegion(lua_State* L, int index) {
    if (!lua::Userdata::test<math::Rect>(L, index) && !lua_istable(L, index)) {
        luaL_error(L, "A mouse passthrough region is a rectangle or a polygon, not a %s.", luaL_typename(L, index));
    }
    if (isRect(L, index)) {
        const math::Rect rect = lua::Stack::read<math::Rect>(L, index);
        return {rect.getMin(), {rect.getRight(), rect.y}, rect.getMax(), {rect.x, rect.getBottom()}};
    }

    const lua_Integer count = luaL_len(L, index);
    if (count < 3) {
        luaL_error(L, "A mouse passthrough polygon needs at least three points.");
    }
    math::Polygon::Outline outline;
    outline.reserve(static_cast<std::size_t>(count));
    for (lua_Integer point = 1; point <= count; ++point) {
        lua_rawgeti(L, index, point);
        outline.push_back(lua::Stack::read<math::Vec2>(L, -1));
        lua_pop(L, 1);
    }
    return outline;
}

void WindowLua::pushMonitor(lua_State* L, const Monitor& monitor) {
    lua_createtable(L, 0, 5);
    lua::Stack::push(L, monitor.name);
    lua_setfield(L, -2, "name");
    lua::Stack::push(L, monitor.bounds);
    lua_setfield(L, -2, "bounds");
    lua::Stack::push(L, monitor.workArea);
    lua_setfield(L, -2, "workArea");
    lua::Stack::push(L, monitor.scale);
    lua_setfield(L, -2, "scale");
    lua::Stack::push(L, monitor.primary);
    lua_setfield(L, -2, "primary");
}

int WindowLua::framebufferSize(lua_State* L) {
    const math::Vec2 value = lua::Runtime::getEngine(L).getWindow().getFramebufferSize();
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int WindowLua::dpiScale(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getDpiScale());
    return 1;
}

int WindowLua::fullscreen(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isFullscreen());
    return 1;
}

int WindowLua::setFullscreen(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setFullscreen(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::resizable(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isResizable());
    return 1;
}

int WindowLua::setResizable(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setResizable(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::setTitle(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setTitle(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int WindowLua::setCursor(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setCursor(lua::Stack::read<Window::Cursor>(L, 1));
    return 0;
}

int WindowLua::setCursorVisible(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setCursorVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::setMouseLocked(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setMouseLocked(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::setKeyboardVisible(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setKeyboardVisible(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::orientation(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getOrientation());
    return 1;
}

int WindowLua::lockOrientation(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().lockOrientation(lua::Stack::read<Orientation>(L, 1));
    return 0;
}

int WindowLua::clipboard(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getClipboard());
    return 1;
}

int WindowLua::setClipboard(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setClipboard(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int WindowLua::hasPointerDevice(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().hasPointerDevice());
    return 1;
}

int WindowLua::backLeavesApp(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).canBackLeaveApp());
    return 1;
}

int WindowLua::setBackLeavesApp(lua_State* L) {
    lua::Runtime::getEngine(L).setBackLeavesApp(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::canBeTransparent(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().canBeTransparent());
    return 1;
}

int WindowLua::transparent(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isTransparent());
    return 1;
}

int WindowLua::setTransparent(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setTransparent(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::decorated(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isDecorated());
    return 1;
}

int WindowLua::setDecorated(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setDecorated(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::alwaysOnTop(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isAlwaysOnTop());
    return 1;
}

int WindowLua::setAlwaysOnTop(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setAlwaysOnTop(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::showInTaskbar(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isShownInTaskbar());
    return 1;
}

int WindowLua::setShowInTaskbar(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setShowInTaskbar(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::focusable(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().isFocusable());
    return 1;
}

int WindowLua::setFocusable(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().setFocusable(lua::Stack::read<bool>(L, 1));
    return 0;
}

int WindowLua::frame(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getFrame());
    return 1;
}

int WindowLua::setFrame(lua_State* L) {
    const math::Rect value{lua::Stack::read<float>(L, 1), lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)};
    luaL_argcheck(L, value.width > 0.0F, 3, "positive width expected");
    luaL_argcheck(L, value.height > 0.0F, 4, "positive height expected");
    lua::Runtime::getEngine(L).getWindow().setFrame(value);
    return 0;
}

// Moves the window with `place(position)`, where the position reads like the one of `app.json` and keeps the size of the window unless it fills the area.
int WindowLua::place(lua_State* L) {
    Window& window = lua::Runtime::getEngine(L).getWindow();
    const WindowPlacement placement = WindowPlacement::fromJson(lua::JsonConverter::read(L, 1));
    const std::vector<Monitor> all = window.getMonitors();
    window.setFrame(placement.resolve(all, window.getFrame().getSize()));
    return 0;
}

int WindowLua::mousePassthrough(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getWindow().getMousePassthrough());
    return 1;
}

// Takes `false`, `true` for the whole window, or a list of regions that keep the mouse, in design units that follow the viewport of the moment or in framebuffer pixels when the second argument is `pixels`.
int WindowLua::setMousePassthrough(lua_State* L) {
    core::Engine& engine = lua::Runtime::getEngine(L);
    if (lua_isboolean(L, 1)) {
        engine.getWindow().setMousePassthrough(lua_toboolean(L, 1) != 0 ? Window::Passthrough::Whole : Window::Passthrough::Off, {});
        return 0;
    }
    luaL_argexpected(L, lua_istable(L, 1), 1, "boolean or table of regions");
    const std::string_view units = lua_isnoneornil(L, 2) ? "design" : lua::Stack::read<std::string_view>(L, 2);
    luaL_argcheck(L, units == "design" || units == "pixels", 2, "'design' or 'pixels' expected");

    std::vector<math::Polygon::Outline> regions;
    const lua_Integer count = luaL_len(L, 1);
    for (lua_Integer index = 1; index <= count; ++index) {
        lua_rawgeti(L, 1, index);
        regions.push_back(readRegion(L, lua_gettop(L)));
        lua_pop(L, 1);
    }
    if (units == "design") {
        const graphics::Viewport& viewport = engine.getViewport();
        for (math::Polygon::Outline& region : regions) {
            for (math::Vec2& point : region) {
                point = viewport.toFramebuffer(point);
            }
        }
    }
    engine.getWindow().setMousePassthrough(Window::Passthrough::Regions, regions);
    return 0;
}

int WindowLua::startDrag(lua_State* L) {
    lua::Runtime::getEngine(L).getWindow().startDrag();
    return 0;
}

int WindowLua::monitors(lua_State* L) {
    const std::vector<Monitor> all = lua::Runtime::getEngine(L).getWindow().getMonitors();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        pushMonitor(L, all[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int WindowLua::currentMonitor(lua_State* L) {
    pushMonitor(L, lua::Runtime::getEngine(L).getWindow().getCurrentMonitor());
    return 1;
}

int WindowLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"framebufferSize", &framebufferSize}, {"dpiScale", &dpiScale}, {"fullscreen", &fullscreen}, {"setFullscreen", &lua::Binding::native<&setFullscreen>}, {"resizable", &resizable}, {"setResizable", &lua::Binding::native<&setResizable>}, {"setTitle", &lua::Binding::native<&setTitle>}, {"setCursor", &lua::Binding::native<&setCursor>}, {"setCursorVisible", &lua::Binding::native<&setCursorVisible>}, {"setMouseLocked", &lua::Binding::native<&setMouseLocked>}, {"setKeyboardVisible", &lua::Binding::native<&setKeyboardVisible>}, {"orientation", &orientation}, {"lockOrientation", &lua::Binding::native<&lockOrientation>}, {"clipboard", &lua::Binding::native<&clipboard>}, {"setClipboard", &lua::Binding::native<&setClipboard>}, {"hasPointerDevice", &hasPointerDevice}, {"backLeavesApp", &backLeavesApp}, {"setBackLeavesApp", &lua::Binding::native<&setBackLeavesApp>}, {"canBeTransparent", &canBeTransparent}, {"transparent", &transparent}, {"setTransparent", &lua::Binding::native<&setTransparent>}, {"decorated", &decorated}, {"setDecorated", &lua::Binding::native<&setDecorated>}, {"alwaysOnTop", &alwaysOnTop}, {"setAlwaysOnTop", &lua::Binding::native<&setAlwaysOnTop>}, {"showInTaskbar", &showInTaskbar}, {"setShowInTaskbar", &lua::Binding::native<&setShowInTaskbar>}, {"focusable", &focusable}, {"setFocusable", &lua::Binding::native<&setFocusable>}, {"frame", &lua::Binding::native<&frame>}, {"setFrame", &lua::Binding::native<&setFrame>}, {"place", &lua::Binding::native<&place>}, {"mousePassthrough", &mousePassthrough}, {"setMousePassthrough", &lua::Binding::native<&setMousePassthrough>}, {"startDrag", &lua::Binding::native<&startDrag>}, {"monitors", &lua::Binding::native<&monitors>}, {"currentMonitor", &lua::Binding::native<&currentMonitor>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void WindowLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.window", &open);
}

} // namespace haylen::platform
