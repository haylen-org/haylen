#include "input/InputBinding.hpp"

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
#include "haylen/input/AppTextField.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/GestureRecognizer.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/input/PointerEmulation.hpp"
#include "haylen/input/VirtualInput.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/System.hpp"
#include "haylen/platform/TextInput.hpp"
#include "input/ActionMapBinding.hpp"
#include "input/CommandBufferBinding.hpp"
#include "input/GamepadMotionBinding.hpp"
#include "input/InputRecordingBinding.hpp"

namespace haylen::lua::input {

haylen::input::Input& InputBinding::getInput(lua_State* L) {
    return lua::Runtime::getEngine(L).getInput();
}

// Lua counts gamepads from one, like every other sequence in the language.
std::size_t InputBinding::readGamepadIndex(lua_State* L, int index) {
    const lua_Integer value = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, value >= 1 && value <= static_cast<lua_Integer>(haylen::input::Input::kMaxGamepads), index, "gamepad index out of range");
    return static_cast<std::size_t>(value - 1);
}

haylen::input::MouseButton InputBinding::readMouseButton(lua_State* L, int index) {
    return lua_isnoneornil(L, index) ? haylen::input::MouseButton::Left : lua::Stack::read<haylen::input::MouseButton>(L, index);
}

int InputBinding::keyDown(lua_State* L) {
    lua::Stack::push(L, getInput(L).isKeyDown(lua::Stack::read<haylen::input::Key>(L, 1)));
    return 1;
}

int InputBinding::keyPressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isKeyPressed(lua::Stack::read<haylen::input::Key>(L, 1)));
    return 1;
}

int InputBinding::keyReleased(lua_State* L) {
    lua::Stack::push(L, getInput(L).isKeyReleased(lua::Stack::read<haylen::input::Key>(L, 1)));
    return 1;
}

int InputBinding::anyKeyPressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isAnyKeyPressed());
    return 1;
}

void InputBinding::pushModifiers(lua_State* L, haylen::input::KeyModifiers held) {
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

int InputBinding::modifiers(lua_State* L) {
    pushModifiers(L, getInput(L).getModifiers());
    return 1;
}

int InputBinding::text(lua_State* L) {
    std::string typed;
    for (const char32_t codePoint : getInput(L).getText()) {
        haylen::core::Utf8::append(typed, codePoint);
    }

    lua::Stack::push(L, typed);
    return 1;
}

int InputBinding::mouseDown(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMouseDown(readMouseButton(L, 1)));
    return 1;
}

int InputBinding::mousePressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMousePressed(readMouseButton(L, 1)));
    return 1;
}

int InputBinding::mouseReleased(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMouseReleased(readMouseButton(L, 1)));
    return 1;
}

int InputBinding::pushPair(lua_State* L, haylen::math::Vec2 value) {
    lua::Stack::push(L, value.x);
    lua::Stack::push(L, value.y);
    return 2;
}

int InputBinding::mousePosition(lua_State* L) {
    return pushPair(L, getInput(L).getMousePosition());
}

int InputBinding::mouseFramebufferPosition(lua_State* L) {
    return pushPair(L, getInput(L).getMouseFramebufferPosition());
}

int InputBinding::mouseDelta(lua_State* L) {
    return pushPair(L, getInput(L).getMouseDelta());
}

int InputBinding::mouseScroll(lua_State* L) {
    return pushPair(L, getInput(L).getMouseScroll());
}

int InputBinding::mouseInside(lua_State* L) {
    lua::Stack::push(L, getInput(L).isMouseInside());
    return 1;
}

int InputBinding::pointerCaptured(lua_State* L) {
    lua::Stack::push(L, getInput(L).isPointerCaptured());
    return 1;
}

int InputBinding::keyCaptured(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isKeyCaptured(lua::Stack::read<haylen::input::Key>(L, 1)));
    return 1;
}

int InputBinding::gamepadButtonCaptured(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isGamepadButtonCaptured(readGamepadIndex(L, 2), lua::Stack::read<haylen::input::GamepadButton>(L, 1)));
    return 1;
}

int InputBinding::gamepadAxisCaptured(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getActions().isGamepadAxisCaptured(readGamepadIndex(L, 2), lua::Stack::read<haylen::input::GamepadAxis>(L, 1)));
    return 1;
}

void InputBinding::pushTouch(lua_State* L, const haylen::input::Touch& finger) {
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

int InputBinding::touches(lua_State* L) {
    const std::span<const haylen::input::Touch> active = getInput(L).getTouches();
    lua_createtable(L, static_cast<int>(active.size()), 0);
    for (std::size_t index = 0; index < active.size(); ++index) {
        pushTouch(L, active[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int InputBinding::findTouch(lua_State* L) {
    const haylen::input::Touch* found = getInput(L).findTouch(lua::Stack::read<std::uint64_t>(L, 1));
    if (found == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    pushTouch(L, *found);
    return 1;
}

// Returns the gestures of this frame as `{type, x, y, dx, dy, scale, rotation}` tables, in design coordinates.
int InputBinding::gestures(lua_State* L) {
    const std::span<const haylen::input::Gesture> recognized = lua::Runtime::getEngine(L).getGestures().getGestures();
    lua_createtable(L, static_cast<int>(recognized.size()), 0);
    for (std::size_t index = 0; index < recognized.size(); ++index) {
        const haylen::input::Gesture& gesture = recognized[index];
        lua_createtable(L, 0, 7);
        lua::Stack::push(L, haylen::input::Gesture::typeName(gesture.type));
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
        lua::Stack::push(L, gesture.rotation);
        lua_setfield(L, -2, "rotation");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Tunes recognition with `setGestureSettings({tapMaxDuration, tapMaxMovement, doubleTapInterval, doubleTapDistance, longPressDuration, swipeMinDistance, swipeMaxDuration, mouse})`.
int InputBinding::setGestureSettings(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kGestureFields});
    haylen::input::GestureRecognizer& recognizer = lua::Runtime::getEngine(L).getGestures();
    haylen::input::GestureRecognizer::Settings settings = recognizer.getSettings();
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

// Returns the thresholds of gesture recognition in the table format `setGestureSettings` accepts.
int InputBinding::gestureSettings(lua_State* L) {
    const haylen::input::GestureRecognizer::Settings& settings = lua::Runtime::getEngine(L).getGestures().getSettings();
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

int InputBinding::gamepadConnected(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).connected);
    return 1;
}

int InputBinding::gamepadName(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).name);
    return 1;
}

int InputBinding::gamepadLabels(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepad(readGamepadIndex(L, 1)).labels);
    return 1;
}

// Runs the motors of a gamepad with `rumble(index, {low, high, seconds})`, where both motors at 0 stop it.
int InputBinding::rumble(lua_State* L) {
    const std::size_t index = readGamepadIndex(L, 1);
    float low = 0.0F;
    float high = 0.0F;
    float seconds = kRumbleSeconds;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kRumbleOptions});
        lua::Table::readField(L, 2, "low", low);
        lua::Table::readField(L, 2, "high", high);
        lua::Table::readField(L, 2, "seconds", seconds);
    }

    lua::Runtime::getEngine(L).getSystem().rumbleGamepad(index, low, high, seconds);
    return 0;
}

int InputBinding::gamepadDown(lua_State* L) {
    lua::Stack::push(L, getInput(L).isGamepadDown(readGamepadIndex(L, 2), lua::Stack::read<haylen::input::GamepadButton>(L, 1)));
    return 1;
}

int InputBinding::gamepadPressed(lua_State* L) {
    lua::Stack::push(L, getInput(L).isGamepadPressed(readGamepadIndex(L, 2), lua::Stack::read<haylen::input::GamepadButton>(L, 1)));
    return 1;
}

int InputBinding::gamepadReleased(lua_State* L) {
    lua::Stack::push(L, getInput(L).isGamepadReleased(readGamepadIndex(L, 2), lua::Stack::read<haylen::input::GamepadButton>(L, 1)));
    return 1;
}

int InputBinding::gamepadAxis(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepadAxis(readGamepadIndex(L, 2), lua::Stack::read<haylen::input::GamepadAxis>(L, 1)));
    return 1;
}

int InputBinding::gamepadStick(lua_State* L) {
    const std::string_view side = lua::Stack::read<std::string_view>(L, 1);
    luaL_argcheck(L, side == "left" || side == "right", 1, "expected left or right");
    return pushPair(L, getInput(L).getGamepadStick(readGamepadIndex(L, 2), side == "right"));
}

// Returns the lowest index of a gamepad that pressed the button in this frame, or any button when none is named, or nil, which a screen where players join with their own gamepad reads.
int InputBinding::findGamepadPress(lua_State* L) {
    const haylen::input::Input& devices = getInput(L);
    const std::optional<haylen::input::GamepadButton> wanted = lua_isnoneornil(L, 1) ? std::nullopt : std::optional(lua::Stack::read<haylen::input::GamepadButton>(L, 1));
    for (std::size_t index = 0; index < haylen::input::Input::kMaxGamepads; ++index) {
        for (std::size_t code = 0; code < haylen::input::Controls::kGamepadButtonCount; ++code) {
            const auto button = static_cast<haylen::input::GamepadButton>(code);
            if ((!wanted || *wanted == button) && devices.isGamepadPressed(index, button)) {
                lua_pushinteger(L, static_cast<lua_Integer>(index + 1));
                return 1;
            }
        }
    }

    lua_pushnil(L);
    return 1;
}

int InputBinding::setGamepadDeadzone(lua_State* L) {
    getInput(L).setGamepadDeadzone(lua::Stack::read<float>(L, 1));
    return 0;
}

int InputBinding::gamepadDeadzone(lua_State* L) {
    lua::Stack::push(L, getInput(L).getGamepadDeadzone());
    return 1;
}

int InputBinding::setScrollSpeed(lua_State* L) {
    getInput(L).setScrollSpeed(lua::Stack::read<float>(L, 1));
    return 0;
}

int InputBinding::scrollSpeed(lua_State* L) {
    lua::Stack::push(L, getInput(L).getScrollSpeed());
    return 1;
}

int InputBinding::lastDevice(lua_State* L) {
    lua::Stack::push(L, getInput(L).getLastDevice());
    return 1;
}

template <typename Enum, std::size_t Size> void InputBinding::readChoice(lua_State* L, const char* field, const std::array<std::pair<std::string_view, Enum>, Size>& names, Enum& target) {
    if (lua_getfield(L, 1, field) == LUA_TNIL) {
        lua_pop(L, 1);
        return;
    }

    const std::string_view name = lua::Stack::read<std::string_view>(L, -1);
    const auto found = std::ranges::find(names, name, &std::pair<std::string_view, Enum>::first);
    if (found == names.end()) {
        std::string expected;
        for (std::size_t index = 0; index < Size; ++index) {
            expected += std::string(index == 0 ? "" : (index + 1 == Size ? " or " : ", ")) + "\"" + std::string(names[index].first) + "\"";
        }

        luaL_error(L, "The option \"%s\" of a text field must be %s.", field, expected.c_str());
    }

    target = found->second;
    lua_pop(L, 1);
}

// Starts editing the text field the app draws itself with `editText({text, selectionStart, selectionEnd, x, y, width, height, keyboard, returnKey, autocapitalize, autocorrect, maxLength})`, where the place is in design coordinates, and follows it on every later call.
int InputBinding::editText(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kTextFieldOptions});
    haylen::input::AppTextField::State state;

    if (lua_getfield(L, 1, "text") != LUA_TSTRING) {
        return luaL_error(L, "A text field needs its \"text\" as a string.");
    }

    state.text = lua::Stack::read<std::string>(L, -1);
    lua_pop(L, 1);
    // A selection with one end given is a caret there, and one with no end given is a caret at the end of the text.
    const auto length = static_cast<int>(haylen::core::Utf8::countCodePoints(state.text));
    std::optional<int> start;
    std::optional<int> end;
    lua::Table::readField(L, 1, "selectionStart", start);
    lua::Table::readField(L, 1, "selectionEnd", end);
    state.selectionStart = start.value_or(end.value_or(length));
    state.selectionEnd = end.value_or(state.selectionStart);
    if (state.selectionStart < 0 || state.selectionEnd < state.selectionStart || state.selectionEnd > length) {
        return luaL_error(L, "The selection of a text field must run from 0 to the length of its text, with \"selectionStart\" not past \"selectionEnd\".");
    }

    haylen::math::Rect place;
    lua::Table::readField(L, 1, "x", place.x);
    lua::Table::readField(L, 1, "y", place.y);
    lua::Table::readField(L, 1, "width", place.width);
    lua::Table::readField(L, 1, "height", place.height);
    const haylen::graphics::Viewport& viewport = lua::Runtime::getEngine(L).getViewport();
    state.bounds = haylen::math::Rect::fromMinMax(viewport.toFramebuffer(place.getMin()), viewport.toFramebuffer(place.getMax()));

    readChoice(L, "keyboard", haylen::platform::TextInput::kKeyboards, state.options.keyboard);
    readChoice(L, "returnKey", haylen::platform::TextInput::kReturnKeys, state.options.returnKey);
    readChoice(L, "autocapitalize", haylen::platform::TextInput::kCapitalizations, state.options.capitalization);
    lua::Table::readField(L, 1, "autocorrect", state.options.autocorrect);
    lua::Table::readField(L, 1, "maxLength", state.options.maxLength);
    lua::Runtime::getEngine(L).getAppTextField().edit(std::move(state));
    return 0;
}

int InputBinding::finishText(lua_State* L) {
    lua::Runtime::getEngine(L).getAppTextField().finish();
    return 0;
}

int InputBinding::editingText(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getAppTextField().isEditing());
    return 1;
}

int InputBinding::setMouseAsTouch(lua_State* L) {
    lua::Runtime::getEngine(L).getPointerEmulation().setMouseAsTouch(lua::Stack::read<bool>(L, 1));
    return 0;
}

int InputBinding::mouseAsTouch(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPointerEmulation().isMouseAsTouch());
    return 1;
}

int InputBinding::setTouchAsMouse(lua_State* L) {
    lua::Runtime::getEngine(L).getPointerEmulation().setTouchAsMouse(lua::Stack::read<bool>(L, 1));
    return 0;
}

int InputBinding::touchAsMouse(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getPointerEmulation().isTouchAsMouse());
    return 1;
}

int InputBinding::setVirtualButton(lua_State* L) {
    lua::Runtime::getEngine(L).getVirtualInput().setButton(lua::Stack::read<std::string_view>(L, 1), lua::Stack::read<bool>(L, 2));
    return 0;
}

int InputBinding::setVirtualStick(lua_State* L) {
    lua::Runtime::getEngine(L).getVirtualInput().setStick(lua::Stack::read<std::string_view>(L, 1), {lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3)});
    return 0;
}

int InputBinding::clearVirtual(lua_State* L) {
    lua::Runtime::getEngine(L).getVirtualInput().clear();
    return 0;
}

int InputBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"keyDown", &keyDown}, {"keyPressed", &keyPressed}, {"keyReleased", &keyReleased}, {"anyKeyPressed", &anyKeyPressed}, {"modifiers", &modifiers}, {"text", &text}, {"mouseDown", &mouseDown}, {"mousePressed", &mousePressed}, {"mouseReleased", &mouseReleased}, {"mousePosition", &mousePosition}, {"mouseFramebufferPosition", &mouseFramebufferPosition}, {"mouseDelta", &mouseDelta}, {"mouseScroll", &mouseScroll}, {"mouseInside", &mouseInside}, {"pointerCaptured", &pointerCaptured}, {"keyCaptured", &keyCaptured}, {"gamepadButtonCaptured", &gamepadButtonCaptured}, {"gamepadAxisCaptured", &gamepadAxisCaptured}, {"touches", &touches}, {"findTouch", &findTouch}, {"gestures", &lua::Binding::native<&gestures>}, {"setGestureSettings", &lua::Binding::native<&setGestureSettings>}, {"gestureSettings", &gestureSettings}, {"gamepadConnected", &gamepadConnected}, {"gamepadName", &gamepadName}, {"gamepadLabels", &gamepadLabels}, {"rumble", &lua::Binding::native<&rumble>}, {"gamepadDown", &gamepadDown}, {"gamepadPressed", &gamepadPressed}, {"gamepadReleased", &gamepadReleased}, {"gamepadAxis", &gamepadAxis}, {"gamepadStick", &gamepadStick}, {"setGamepadDeadzone", &lua::Binding::native<&setGamepadDeadzone>}, {"gamepadDeadzone", &gamepadDeadzone}, {"setScrollSpeed", &lua::Binding::native<&setScrollSpeed>}, {"scrollSpeed", &scrollSpeed}, {"lastDevice", &lastDevice}, {"editText", &lua::Binding::native<&editText>}, {"finishText", &finishText}, {"editingText", &editingText}, {"setMouseAsTouch", &setMouseAsTouch}, {"mouseAsTouch", &mouseAsTouch}, {"setTouchAsMouse", &setTouchAsMouse}, {"touchAsMouse", &touchAsMouse}, {"findGamepadPress", &findGamepadPress}, {"setVirtualButton", &setVirtualButton}, {"setVirtualStick", &setVirtualStick}, {"clearVirtual", &clearVirtual}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    ActionMapBinding::addFunctions(L);
    CommandBufferBinding::addFunctions(L);
    InputRecordingBinding::addFunctions(L);
    GamepadMotionBinding::addFunctions(L);
    return 1;
}

void InputBinding::install(lua_State* L) {
    ActionMapBinding::install(L);
    CommandBufferBinding::install(L);
    InputRecordingBinding::install(L);
    GamepadMotionBinding::install(L);
    lua::Binding::preload(L, "haylen.input", &open);
}

void InputBinding::pushEvent(lua_State* L, const haylen::platform::Event& event) {
    const haylen::graphics::Viewport& viewport = lua::Runtime::getEngine(L).getViewport();
    lua_createtable(L, 0, 8);
    lua::Stack::push(L, event.type);
    lua_setfield(L, -2, "type");

    switch (event.type) {
    case haylen::platform::Event::Type::KeyDown:
    case haylen::platform::Event::Type::KeyUp:
        lua::Stack::push(L, event.key);
        lua_setfield(L, -2, "key");
        lua::Stack::push(L, event.repeat);
        lua_setfield(L, -2, "repeat");
        break;
    case haylen::platform::Event::Type::Character: {
        std::string character;
        haylen::core::Utf8::append(character, event.character);
        lua::Stack::push(L, character);
        lua_setfield(L, -2, "character");
        break;
    }
    case haylen::platform::Event::Type::MouseDown:
    case haylen::platform::Event::Type::MouseUp: {
        const haylen::math::Vec2 position = viewport.toDesign(event.position);
        lua::Stack::push(L, event.mouseButton);
        lua_setfield(L, -2, "button");
        lua::Stack::push(L, position.x);
        lua_setfield(L, -2, "x");
        lua::Stack::push(L, position.y);
        lua_setfield(L, -2, "y");
        break;
    }
    case haylen::platform::Event::Type::MouseMove: {
        const haylen::math::Vec2 position = viewport.toDesign(event.position);
        const haylen::math::Vec2 delta = event.delta / viewport.getPixelsPerUnit();
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
    case haylen::platform::Event::Type::MouseScroll:
        lua::Stack::push(L, event.scroll.x);
        lua_setfield(L, -2, "scrollX");
        lua::Stack::push(L, event.scroll.y);
        lua_setfield(L, -2, "scrollY");
        lua::Stack::push(L, event.preciseScroll);
        lua_setfield(L, -2, "precise");
        break;
    case haylen::platform::Event::Type::TouchBegan:
    case haylen::platform::Event::Type::TouchMoved:
    case haylen::platform::Event::Type::TouchEnded:
    case haylen::platform::Event::Type::TouchCancelled:
        lua_createtable(L, static_cast<int>(event.touchCount), 0);
        for (std::size_t index = 0; index < event.touchCount; ++index) {
            const haylen::platform::TouchPoint& point = event.touches[index];
            const haylen::math::Vec2 position = viewport.toDesign(point.position);
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
    case haylen::platform::Event::Type::TextEdited:
        lua::Stack::push(L, event.textEdit.field);
        lua_setfield(L, -2, "field");
        lua::Stack::push(L, event.textEdit.text);
        lua_setfield(L, -2, "text");
        lua::Stack::push(L, event.textEdit.selectionStart);
        lua_setfield(L, -2, "selectionStart");
        lua::Stack::push(L, event.textEdit.selectionEnd);
        lua_setfield(L, -2, "selectionEnd");
        if (event.textEdit.isComposing()) {
            lua::Stack::push(L, event.textEdit.compositionStart);
            lua_setfield(L, -2, "compositionStart");
            lua::Stack::push(L, event.textEdit.compositionEnd);
            lua_setfield(L, -2, "compositionEnd");
        }

        return;
    case haylen::platform::Event::Type::TextAction:
        lua::Stack::push(L, event.textEdit.field);
        lua_setfield(L, -2, "field");
        lua::Stack::push(L, event.textAction);
        lua_setfield(L, -2, "action");
        return;
    case haylen::platform::Event::Type::KeyboardChanged: {
        const haylen::math::Rect area = haylen::math::Rect::fromMinMax(viewport.toDesign(event.keyboardFrame.getMin()), viewport.toDesign(event.keyboardFrame.getMax()));
        lua::Stack::push(L, area);
        lua_setfield(L, -2, "frame");
        return;
    }
    case haylen::platform::Event::Type::NetworkChanged:
        lua::Stack::push(L, event.online);
        lua_setfield(L, -2, "online");
        return;
    case haylen::platform::Event::Type::LowMemory:
        lua::Stack::push(L, haylen::platform::Event::memoryLevelName(event.memoryLevel));
        lua_setfield(L, -2, "level");
        return;
    default:
        return;
    }

    // Every input event carries the modifier keys held when it happened.
    pushModifiers(L, event.modifiers);
    lua_setfield(L, -2, "modifiers");
}

} // namespace haylen::lua::input
