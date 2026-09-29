#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "haylen/input/KeyModifiers.hpp"
#include "haylen/input/MouseButton.hpp"
#include "haylen/input/Touch.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::platform {
struct Event;
}

namespace haylen::input {

class Input;

// Installs haylen.input, which reads the keyboard, the mouse, touches, gamepads, gestures, virtual controls and the action map.
class InputLua final {
  public:
    static void install(lua_State* L);

    // Pushes an event as a table with positions converted to design coordinates.
    static void pushEvent(lua_State* L, const platform::Event& event);

  private:
    static constexpr std::array<std::string_view, 8> kGestureFields{"tapMaxDuration", "tapMaxMovement", "doubleTapInterval", "doubleTapDistance", "longPressDuration", "swipeMinDistance", "swipeMaxDuration", "mouse"};

    [[nodiscard]] static Input& getInput(lua_State* L);
    [[nodiscard]] static std::size_t readGamepadIndex(lua_State* L, int index);
    [[nodiscard]] static MouseButton readMouseButton(lua_State* L, int index);
    static void pushModifiers(lua_State* L, KeyModifiers held);
    static int pushPair(lua_State* L, math::Vec2 value);
    static void pushTouch(lua_State* L, const Touch& finger);

    static int keyDown(lua_State* L);
    static int keyPressed(lua_State* L);
    static int keyReleased(lua_State* L);
    static int anyKeyPressed(lua_State* L);
    static int modifiers(lua_State* L);
    static int text(lua_State* L);
    static int mouseDown(lua_State* L);
    static int mousePressed(lua_State* L);
    static int mouseReleased(lua_State* L);
    static int mousePosition(lua_State* L);
    static int mouseFramebufferPosition(lua_State* L);
    static int mouseDelta(lua_State* L);
    static int mouseScroll(lua_State* L);
    static int mouseInside(lua_State* L);
    static int pointerCaptured(lua_State* L);
    static int touches(lua_State* L);
    static int touch(lua_State* L);
    static int gestures(lua_State* L);
    static int setGestureSettings(lua_State* L);
    static int gestureSettings(lua_State* L);
    static int gamepadConnected(lua_State* L);
    static int gamepadName(lua_State* L);
    static int gamepadDown(lua_State* L);
    static int gamepadPressed(lua_State* L);
    static int gamepadReleased(lua_State* L);
    static int gamepadAxis(lua_State* L);
    static int gamepadStick(lua_State* L);
    static int setDeadzone(lua_State* L);
    static int gamepadDeadzone(lua_State* L);
    static int lastDevice(lua_State* L);
    static int loadActions(lua_State* L);
    static int saveActions(lua_State* L);
    static int actionNames(lua_State* L);
    static int defineAction(lua_State* L);
    static int removeAction(lua_State* L);
    static int clearActions(lua_State* L);
    static int actionDefinition(lua_State* L);
    static int setPressThreshold(lua_State* L);
    static int actionDown(lua_State* L);
    static int actionPressed(lua_State* L);
    static int actionReleased(lua_State* L);
    static int actionValue(lua_State* L);
    static int actionVector(lua_State* L);
    static int setGamepadIndex(lua_State* L);
    static int setVirtualButton(lua_State* L);
    static int setVirtualStick(lua_State* L);
    static int clearVirtual(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::input
