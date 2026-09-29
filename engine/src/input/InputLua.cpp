#include "input/InputLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/platform/Event.hpp"

namespace haylen::input {

Input& InputLua::getInput(lua_State* L) {
    return lua::Runtime::getEngine(L).getInput();
}

// Lua counts gamepads from one, like every other sequence in the language.
std::size_t InputLua::readGamepadIndex(lua_State* L, int index) {
    const lua_Integer value = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, value >= 1 && value <= static_cast<lua_Integer>(Input::kMaxGamepads), index, "gamepad index out of range");
    return static_cast<std::size_t>(value - 1);
}

MouseButton InputLua::readMouseButton(lua_State* L, int index) {
    return lua_isnoneornil(L, index) ? MouseButton::Left : lua::Stack::read<MouseButton>(L, index);
}

int InputLua::keyDown(lua_State* L) {
    lua::Stack::push(L, getInput(L).isKeyDown(lua::Stack::read<Key>(L, 1)));
    return 1;
}

int InputLua::keyPressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isKeyPressed(lua::Stack::read<Key>(L, 1)));
    return 1;
}

int InputLua::keyReleased(lua_State* L) {
    lua::Stack::push(L, getInput(L).isKeyReleased(lua::Stack::read<Key>(L, 1)));
    return 1;
}

int InputLua::anyKeyPressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isAnyKeyPressed());
    return 1;
}

void InputLua::pushModifiers(lua_State* L, KeyModifiers held) {
    lua_createtable(L, 0, 4);
    lua::Stack::push(L, held.shift);
    lua_setfield(L, -2, "shift");
    lua::Stack::push(L, held.control);
    lua_setfield(L, -2, "control");
    lua::Stack::push(L, held.alt);
    lua_setfield(L, -2, "alt");
    lua::Stack::push(L, held.super);
    lua_setfield(L, -2, "super");
}

int InputLua::modifiers(lua_State* L) {
    pushModifiers(L, getInput(L).getModifiers());
    return 1;
}

int InputLua::text(lua_State* L) {
    std::string typed;
    for (const char32_t codePoint : getInput(L).getText()) {
        core::Utf8::append(typed, codePoint);
    }
    lua::Stack::push(L, typed);
    return 1;
}

int InputLua::mouseDown(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMouseDown(readMouseButton(L, 1)));
    return 1;
}

int InputLua::mousePressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMousePressed(readMouseButton(L, 1)));
    return 1;
}

int InputLua::mouseReleased(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMouseReleased(readMouseButton(L, 1)));
    return 1;
}

int InputLua::pushPair(lua_State* L, math::Vec2 value) {
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int InputLua::mousePosition(lua_State* L) {
    return pushPair(L, getInput(L).getMousePosition());
}

int InputLua::mouseFramebufferPosition(lua_State* L) {
    return pushPair(L, getInput(L).getMouseFramebufferPosition());
}

int InputLua::mouseDelta(lua_State* L) {
    return pushPair(L, getInput(L).getMouseDelta());
}

int InputLua::mouseScroll(lua_State* L) {
    return pushPair(L, getInput(L).getMouseScroll());
}

int InputLua::mouseInside(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMouseInside());
    return 1;
}

int InputLua::pointerCaptured(lua_State* L) {
    lua::Stack::push(L, getInput(L).isPointerCaptured());
    return 1;
}

int InputLua::keyCaptured(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isKeyCaptured(lua::Stack::read<Key>(L, 1)));
    return 1;
}

int InputLua::gamepadCaptured(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isGamepadButtonCaptured(readGamepadIndex(L, 2), lua::Stack::read<GamepadButton>(L, 1)));
    return 1;
}

void InputLua::pushTouch(lua_State* L, const Touch& finger) {
    lua_createtable(L, 0, 8);
    lua::Stack::push(L, finger.id);
    lua_setfield(L, -2, "id");
    lua::Stack::push(L, finger.position.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, finger.position.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, finger.startPosition.x);
    lua_setfield(L, -2, "startX");
    lua::Stack::push(L, finger.startPosition.y);
    lua_setfield(L, -2, "startY");
    lua::Stack::push(L, finger.position.x - finger.previousPosition.x);
    lua_setfield(L, -2, "dx");
    lua::Stack::push(L, finger.position.y - finger.previousPosition.y);
    lua_setfield(L, -2, "dy");
    lua::Stack::push(L, finger.phase);
    lua_setfield(L, -2, "phase");
    lua::Stack::push(L, finger.duration);
    lua_setfield(L, -2, "duration");
}

int InputLua::touches(lua_State* L) {
    const std::span<const Touch> active = getInput(L).getTouches();
    lua_createtable(L, static_cast<int>(active.size()), 0);
    for (std::size_t index = 0; index < active.size(); ++index) {
        pushTouch(L, active[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int InputLua::touch(lua_State* L) {
    const Touch* found = getInput(L).findTouch(lua::Stack::read<std::uint64_t>(L, 1));
    if (found == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    pushTouch(L, *found);
    return 1;
}

// Returns the gestures of this frame as {type, x, y, dx, dy, scale} tables, in design coordinates.
int InputLua::gestures(lua_State* L) {
    const std::span<const Gesture> recognized = lua::Runtime::getEngine(L).getGestures().getGestures();
    lua_createtable(L, static_cast<int>(recognized.size()), 0);
    for (std::size_t index = 0; index < recognized.size(); ++index) {
        const Gesture& gesture = recognized[index];
        lua_createtable(L, 0, 6);
        lua::Stack::push(L, Gesture::typeName(gesture.type));
        lua_setfield(L, -2, "type");
        lua::Stack::push(L, gesture.position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, gesture.position.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, gesture.delta.x);
        lua_setfield(L, -2, "dx");
        lua::Stack::push(L, gesture.delta.y);
        lua_setfield(L, -2, "dy");
        lua::Stack::push(L, gesture.scale);
        lua_setfield(L, -2, "scale");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Tunes recognition with setGestureSettings({tapMaxDuration, tapMaxMovement, doubleTapInterval, doubleTapDistance, longPressDuration, swipeMinDistance, swipeMaxDuration, mouse}).
int InputLua::setGestureSettings(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kGestureFields});
    GestureRecognizer& recognizer = lua::Runtime::getEngine(L).getGestures();
    GestureRecognizer::Settings settings = recognizer.getSettings();
    lua::Table::readField(L, 1, "tapMaxDuration", settings.tapMaxDuration);
    lua::Table::readField(L, 1, "tapMaxMovement", settings.tapMaxMovement);
    lua::Table::readField(L, 1, "doubleTapInterval", settings.doubleTapInterval);
    lua::Table::readField(L, 1, "doubleTapDistance", settings.doubleTapDistance);
    lua::Table::readField(L, 1, "longPressDuration", settings.longPressDuration);
    lua::Table::readField(L, 1, "swipeMinDistance", settings.swipeMinDistance);
    lua::Table::readField(L, 1, "swipeMaxDuration", settings.swipeMaxDuration);
    lua::Table::readField(L, 1, "mouse", settings.mouse);
    recognizer.setSettings(settings);
    return 0;
}

// Returns the thresholds of gesture recognition in the table format setGestureSettings accepts.
int InputLua::gestureSettings(lua_State* L) {
    const GestureRecognizer::Settings& settings = lua::Runtime::getEngine(L).getGestures().getSettings();
    lua_createtable(L, 0, static_cast<int>(kGestureFields.size()));
    lua::Stack::push(L, settings.tapMaxDuration);
    lua_setfield(L, -2, "tapMaxDuration");
    lua::Stack::push(L, settings.tapMaxMovement);
    lua_setfield(L, -2, "tapMaxMovement");
    lua::Stack::push(L, settings.doubleTapInterval);
    lua_setfield(L, -2, "doubleTapInterval");
    lua::Stack::push(L, settings.doubleTapDistance);
    lua_setfield(L, -2, "doubleTapDistance");
    lua::Stack::push(L, settings.longPressDuration);
    lua_setfield(L, -2, "longPressDuration");
    lua::Stack::push(L, settings.swipeMinDistance);
    lua_setfield(L, -2, "swipeMinDistance");
    lua::Stack::push(L, settings.swipeMaxDuration);
    lua_setfield(L, -2, "swipeMaxDuration");
    lua::Stack::push(L, settings.mouse);
    lua_setfield(L, -2, "mouse");
    return 1;
}

int InputLua::gamepadConnected(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).connected);
    return 1;
}

int InputLua::gamepadName(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).name);
    return 1;
}

int InputLua::gamepadDown(lua_State* L) {
    lua::Stack::push(L, getInput(L).isGamepadDown(readGamepadIndex(L, 2), lua::Stack::read<GamepadButton>(L, 1)));
    return 1;
}

int InputLua::gamepadPressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isGamepadPressed(readGamepadIndex(L, 2), lua::Stack::read<GamepadButton>(L, 1)));
    return 1;
}

int InputLua::gamepadReleased(lua_State* L) {
    lua::Stack::push(L, getInput(L).isGamepadReleased(readGamepadIndex(L, 2), lua::Stack::read<GamepadButton>(L, 1)));
    return 1;
}

int InputLua::gamepadAxis(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepadAxis(readGamepadIndex(L, 2), lua::Stack::read<GamepadAxis>(L, 1)));
    return 1;
}

int InputLua::gamepadStick(lua_State* L) {
    const std::string_view side = lua::Stack::read<std::string_view>(L, 1);
    luaL_argcheck(L, side == "left" || side == "right", 1, "expected left or right");
    return pushPair(L, getInput(L).getGamepadStick(readGamepadIndex(L, 2), side == "right"));
}

int InputLua::setDeadzone(lua_State* L) {
    getInput(L).setGamepadDeadzone(lua::Stack::read<float>(L, 1));
    return 0;
}

int InputLua::gamepadDeadzone(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepadDeadzone());
    return 1;
}

int InputLua::lastDevice(lua_State* L) {
    lua::Stack::push(L, getInput(L).getLastDevice());
    return 1;
}

int InputLua::loadActions(lua_State* L) {
    core::Engine& owner = lua::Runtime::getEngine(L);
    const core::Json document = lua_type(L, 1) == LUA_TSTRING ? owner.getAssets().json(lua::Stack::read<std::string_view>(L, 1)) : lua::JsonConverter::read(L, 1);
    owner.getActions().load(document);
    return 0;
}

int InputLua::saveActions(lua_State* L) {
    lua::JsonConverter::push(L, lua::Runtime::getEngine(L).getActions().save());
    return 1;
}

int InputLua::actionNames(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().getNames());
    return 1;
}

// Adds an action given as a table in the action map document format, or replaces the action with the same name in its place.
int InputLua::defineAction(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Runtime::getEngine(L).getActions().define(ActionMap::Action::fromJson(lua::JsonConverter::read(L, 1)));
    return 0;
}

int InputLua::removeAction(lua_State* L) {
    lua::Runtime::getEngine(L).getActions().remove(lua::Stack::read<std::string_view>(L, 1));
    return 0;
}

int InputLua::clearActions(lua_State* L) {
    lua::Runtime::getEngine(L).getActions().clear();
    return 0;
}

int InputLua::actionDefinition(lua_State* L) {
    const ActionMap::Action* definition = lua::Runtime::getEngine(L).getActions().findAction(lua::Stack::read<std::string_view>(L, 1));
    if (definition == nullptr) {
        lua_pushnil(L);
        return 1;
    }
    lua::JsonConverter::push(L, definition->toJson());
    return 1;
}

int InputLua::setPressThreshold(lua_State* L) {
    lua::Runtime::getEngine(L).getActions().setPressThreshold(lua::Stack::read<float>(L, 1));
    return 0;
}

int InputLua::actionDown(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isDown(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int InputLua::actionPressed(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isPressed(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int InputLua::actionReleased(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isReleased(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int InputLua::actionValue(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().getValue(lua::Stack::read<std::string_view>(L, 1)));
    return 1;
}

int InputLua::actionVector(lua_State* L) {
    return pushPair(L, lua::Runtime::getEngine(L).getActions().getVector(lua::Stack::read<std::string_view>(L, 1)));
}

int InputLua::setGamepadIndex(lua_State* L) {
    lua::Runtime::getEngine(L).getActions().setGamepadIndex(lua_isnoneornil(L, 1) ? std::nullopt : std::optional<std::size_t>(readGamepadIndex(L, 1)));
    return 0;
}

int InputLua::setVirtualButton(lua_State* L) {
    lua::Runtime::getEngine(L).getVirtualInput().setButton(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<bool>(L, 2));
    return 0;
}

int InputLua::setVirtualStick(lua_State* L) {
    lua::Runtime::getEngine(L).getVirtualInput().setStick(lua::Stack::read<std::string_view>(L, 1), {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int InputLua::clearVirtual(lua_State* L) {
    lua::Runtime::getEngine(L).getVirtualInput().clear();
    return 0;
}

int InputLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"keyDown", &keyDown}, {"keyPressed", &keyPressed}, {"keyReleased", &keyReleased}, {"anyKeyPressed", &anyKeyPressed}, {"modifiers", &modifiers}, {"text", &text}, {"mouseDown", &mouseDown}, {"mousePressed", &mousePressed}, {"mouseReleased", &mouseReleased}, {"mousePosition", &mousePosition}, {"mouseFramebufferPosition", &mouseFramebufferPosition}, {"mouseDelta", &mouseDelta}, {"mouseScroll", &mouseScroll}, {"mouseInside", &mouseInside}, {"pointerCaptured", &pointerCaptured}, {"keyCaptured", &keyCaptured}, {"gamepadCaptured", &gamepadCaptured}, {"touches", &touches}, {"touch", &touch}, {"gestures", &lua::Binding::native<&gestures>}, {"setGestureSettings", &lua::Binding::native<&setGestureSettings>}, {"gestureSettings", &gestureSettings}, {"gamepadConnected", &gamepadConnected}, {"gamepadName", &gamepadName}, {"gamepadDown", &gamepadDown}, {"gamepadPressed", &gamepadPressed}, {"gamepadReleased", &gamepadReleased}, {"gamepadAxis", &gamepadAxis}, {"gamepadStick", &gamepadStick}, {"setDeadzone", &lua::Binding::native<&setDeadzone>}, {"gamepadDeadzone", &gamepadDeadzone}, {"lastDevice", &lastDevice}, {"loadActions", &lua::Binding::native<&loadActions>}, {"saveActions", &saveActions}, {"actionNames", &actionNames}, {"defineAction", &lua::Binding::native<&defineAction>}, {"removeAction", &removeAction}, {"clearActions", &clearActions}, {"actionDefinition", &actionDefinition}, {"setPressThreshold", &lua::Binding::native<&setPressThreshold>}, {"down", &actionDown}, {"pressed", &actionPressed}, {"released", &actionReleased}, {"value", &actionValue}, {"vector", &actionVector}, {"setGamepadIndex", &setGamepadIndex}, {"setVirtualButton", &setVirtualButton}, {"setVirtualStick", &setVirtualStick}, {"clearVirtual", &clearVirtual}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void InputLua::install(lua_State* L) {
    lua::Binding::preload(L, "haylen.input", &open);
}

void InputLua::pushEvent(lua_State* L, const platform::Event& event) {
    const graphics::Viewport& viewport = lua::Runtime::getEngine(L).getViewport();
    lua_createtable(L, 0, 8);
    lua::Stack::push(L, event.type);
    lua_setfield(L, -2, "type");

    switch (event.type) {
    case platform::Event::Type::KeyDown:
    case platform::Event::Type::KeyUp:
        lua::Stack::push(L, event.key);
        lua_setfield(L, -2, "key");
        lua::Stack::push(L, event.repeat);
        lua_setfield(L, -2, "repeat");
        break;
    case platform::Event::Type::Character: {
        std::string character;
        core::Utf8::append(character, event.character);
        lua::Stack::push(L, character);
        lua_setfield(L, -2, "character");
        break;
    }
    case platform::Event::Type::MouseDown:
    case platform::Event::Type::MouseUp: {
        const math::Vec2 position = viewport.toDesign(event.position);
        lua::Stack::push(L, event.mouseButton);
        lua_setfield(L, -2, "button");
        lua::Stack::push(L, position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, position.y);
        lua_setfield(L, -2, "y");
        break;
    }
    case platform::Event::Type::MouseMove: {
        const math::Vec2 position = viewport.toDesign(event.position);
        const math::Vec2 delta = event.delta / viewport.getPixelsPerUnit();
        lua::Stack::push(L, position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, position.y);
        lua_setfield(L, -2, "y");
        lua::Stack::push(L, delta.x);
        lua_setfield(L, -2, "dx");
        lua::Stack::push(L, delta.y);
        lua_setfield(L, -2, "dy");
        break;
    }
    case platform::Event::Type::MouseScroll:
        lua::Stack::push(L, event.scroll.x);
        lua_setfield(L, -2, "scrollX");
        lua::Stack::push(L, event.scroll.y);
        lua_setfield(L, -2, "scrollY");
        break;
    case platform::Event::Type::TouchBegan:
    case platform::Event::Type::TouchMoved:
    case platform::Event::Type::TouchEnded:
    case platform::Event::Type::TouchCancelled:
        lua_createtable(L, static_cast<int>(event.touchCount), 0);
        for (std::size_t index = 0; index < event.touchCount; ++index) {
            const platform::TouchPoint& point = event.touches[index];
            const math::Vec2 position = viewport.toDesign(point.position);
            lua_createtable(L, 0, 4);
            lua::Stack::push(L, point.id);
            lua_setfield(L, -2, "id");
            lua::Stack::push(L, position.x);
            lua_setfield(L, -2, "x");
            lua::Stack::push(L, position.y);
            lua_setfield(L, -2, "y");
            lua::Stack::push(L, point.changed);
            lua_setfield(L, -2, "changed");
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }
        lua_setfield(L, -2, "touches");
        break;
    case platform::Event::Type::TextEdited:
        lua::Stack::push(L, event.textEdit.field);
        lua_setfield(L, -2, "field");
        lua::Stack::push(L, event.textEdit.text);
        lua_setfield(L, -2, "text");
        return;
    case platform::Event::Type::TextAction:
        lua::Stack::push(L, event.textEdit.field);
        lua_setfield(L, -2, "field");
        lua::Stack::push(L, event.textAction);
        lua_setfield(L, -2, "action");
        return;
    case platform::Event::Type::KeyboardChanged: {
        const math::Rect area = math::Rect::fromMinMax(viewport.toDesign(event.keyboardFrame.getMin()), viewport.toDesign(event.keyboardFrame.getMax()));
        lua::Stack::push(L, area);
        lua_setfield(L, -2, "frame");
        return;
    }
    case platform::Event::Type::NetworkChanged:
        lua::Stack::push(L, event.online);
        lua_setfield(L, -2, "online");
        return;
    default:
        return;
    }

    // Every input event carries the modifier keys held when it happened.
    pushModifiers(L, event.modifiers);
    lua_setfield(L, -2, "modifiers");
}

} // namespace haylen::input
