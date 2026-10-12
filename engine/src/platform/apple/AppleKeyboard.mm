#import "platform/apple/AppleKeyboard.hpp"

#if !TARGET_OS_OSX
#include <unordered_map>

#include "haylen/platform/Event.hpp"
#include "platform/sokol/SokolRuntime.hpp"

namespace haylen::platform {

void AppleKeyboard::observe() {
    [NSNotificationCenter.defaultCenter addObserverForName:GCKeyboardDidConnectNotification object:nil queue:NSOperationQueue.mainQueue usingBlock:^(NSNotification* notification) { observeKeyboard(notification.object); }];
    if (GCKeyboard.coalescedKeyboard != nil) {
        observeKeyboard(GCKeyboard.coalescedKeyboard);
    }
}

input::Key AppleKeyboard::toKey(GCKeyCode code) {
    static const std::unordered_map<GCKeyCode, input::Key>& keys = *new const std::unordered_map<GCKeyCode, input::Key>{
        {GCKeyCodeKeyA, input::Key::A}, {GCKeyCodeKeyB, input::Key::B}, {GCKeyCodeKeyC, input::Key::C}, {GCKeyCodeKeyD, input::Key::D}, {GCKeyCodeKeyE, input::Key::E}, {GCKeyCodeKeyF, input::Key::F}, {GCKeyCodeKeyG, input::Key::G}, {GCKeyCodeKeyH, input::Key::H}, {GCKeyCodeKeyI, input::Key::I}, {GCKeyCodeKeyJ, input::Key::J}, {GCKeyCodeKeyK, input::Key::K}, {GCKeyCodeKeyL, input::Key::L}, {GCKeyCodeKeyM, input::Key::M}, {GCKeyCodeKeyN, input::Key::N}, {GCKeyCodeKeyO, input::Key::O}, {GCKeyCodeKeyP, input::Key::P}, {GCKeyCodeKeyQ, input::Key::Q}, {GCKeyCodeKeyR, input::Key::R}, {GCKeyCodeKeyS, input::Key::S}, {GCKeyCodeKeyT, input::Key::T}, {GCKeyCodeKeyU, input::Key::U}, {GCKeyCodeKeyV, input::Key::V}, {GCKeyCodeKeyW, input::Key::W}, {GCKeyCodeKeyX, input::Key::X}, {GCKeyCodeKeyY, input::Key::Y}, {GCKeyCodeKeyZ, input::Key::Z}, {GCKeyCodeOne, input::Key::Digit1}, {GCKeyCodeTwo, input::Key::Digit2}, {GCKeyCodeThree, input::Key::Digit3}, {GCKeyCodeFour, input::Key::Digit4}, {GCKeyCodeFive, input::Key::Digit5}, {GCKeyCodeSix, input::Key::Digit6}, {GCKeyCodeSeven, input::Key::Digit7}, {GCKeyCodeEight, input::Key::Digit8}, {GCKeyCodeNine, input::Key::Digit9}, {GCKeyCodeZero, input::Key::Digit0}, {GCKeyCodeReturnOrEnter, input::Key::Enter}, {GCKeyCodeEscape, input::Key::Escape}, {GCKeyCodeDeleteOrBackspace, input::Key::Backspace}, {GCKeyCodeTab, input::Key::Tab}, {GCKeyCodeSpacebar, input::Key::Space}, {GCKeyCodeHyphen, input::Key::Minus}, {GCKeyCodeEqualSign, input::Key::Equal}, {GCKeyCodeOpenBracket, input::Key::LeftBracket}, {GCKeyCodeCloseBracket, input::Key::RightBracket}, {GCKeyCodeBackslash, input::Key::Backslash}, {GCKeyCodeSemicolon, input::Key::Semicolon}, {GCKeyCodeQuote, input::Key::Apostrophe}, {GCKeyCodeGraveAccentAndTilde, input::Key::GraveAccent}, {GCKeyCodeComma, input::Key::Comma}, {GCKeyCodePeriod, input::Key::Period}, {GCKeyCodeSlash, input::Key::Slash}, {GCKeyCodeCapsLock, input::Key::CapsLock}, {GCKeyCodeF1, input::Key::F1}, {GCKeyCodeF2, input::Key::F2}, {GCKeyCodeF3, input::Key::F3}, {GCKeyCodeF4, input::Key::F4}, {GCKeyCodeF5, input::Key::F5}, {GCKeyCodeF6, input::Key::F6}, {GCKeyCodeF7, input::Key::F7}, {GCKeyCodeF8, input::Key::F8}, {GCKeyCodeF9, input::Key::F9}, {GCKeyCodeF10, input::Key::F10}, {GCKeyCodeF11, input::Key::F11}, {GCKeyCodeF12, input::Key::F12}, {GCKeyCodePrintScreen, input::Key::PrintScreen}, {GCKeyCodeScrollLock, input::Key::ScrollLock}, {GCKeyCodePause, input::Key::Pause}, {GCKeyCodeInsert, input::Key::Insert}, {GCKeyCodeHome, input::Key::Home}, {GCKeyCodePageUp, input::Key::PageUp}, {GCKeyCodeDeleteForward, input::Key::Delete}, {GCKeyCodeEnd, input::Key::End}, {GCKeyCodePageDown, input::Key::PageDown}, {GCKeyCodeRightArrow, input::Key::Right}, {GCKeyCodeLeftArrow, input::Key::Left}, {GCKeyCodeDownArrow, input::Key::Down}, {GCKeyCodeUpArrow, input::Key::Up}, {GCKeyCodeKeypadNumLock, input::Key::NumLock}, {GCKeyCodeKeypadSlash, input::Key::KeypadDivide}, {GCKeyCodeKeypadAsterisk, input::Key::KeypadMultiply}, {GCKeyCodeKeypadHyphen, input::Key::KeypadSubtract}, {GCKeyCodeKeypadPlus, input::Key::KeypadAdd}, {GCKeyCodeKeypadEnter, input::Key::KeypadEnter}, {GCKeyCodeKeypadPeriod, input::Key::KeypadDecimal}, {GCKeyCodeKeypadEqualSign, input::Key::KeypadEqual}, {GCKeyCodeKeypad0, input::Key::Keypad0}, {GCKeyCodeKeypad1, input::Key::Keypad1}, {GCKeyCodeKeypad2, input::Key::Keypad2}, {GCKeyCodeKeypad3, input::Key::Keypad3}, {GCKeyCodeKeypad4, input::Key::Keypad4}, {GCKeyCodeKeypad5, input::Key::Keypad5}, {GCKeyCodeKeypad6, input::Key::Keypad6}, {GCKeyCodeKeypad7, input::Key::Keypad7}, {GCKeyCodeKeypad8, input::Key::Keypad8}, {GCKeyCodeKeypad9, input::Key::Keypad9}, {GCKeyCodeLeftControl, input::Key::LeftControl}, {GCKeyCodeLeftShift, input::Key::LeftShift}, {GCKeyCodeLeftAlt, input::Key::LeftAlt}, {GCKeyCodeLeftGUI, input::Key::LeftSuper}, {GCKeyCodeRightControl, input::Key::RightControl}, {GCKeyCodeRightShift, input::Key::RightShift}, {GCKeyCodeRightAlt, input::Key::RightAlt}, {GCKeyCodeRightGUI, input::Key::RightSuper}, {GCKeyCodeApplication, input::Key::Menu},
    };
    const auto found = keys.find(code);
    return found != keys.end() ? found->second : input::Key::Unknown;
}

input::KeyModifiers AppleKeyboard::toModifiers(GCKeyboardInput* keyboard) {
    const auto held = [keyboard](GCKeyCode left, GCKeyCode right) { return [keyboard buttonForKeyCode:left].pressed || [keyboard buttonForKeyCode:right].pressed; };
    return {.shift = held(GCKeyCodeLeftShift, GCKeyCodeRightShift), .control = held(GCKeyCodeLeftControl, GCKeyCodeRightControl), .alt = held(GCKeyCodeLeftAlt, GCKeyCodeRightAlt), .super = held(GCKeyCodeLeftGUI, GCKeyCodeRightGUI)};
}

void AppleKeyboard::observeKeyboard(GCKeyboard* keyboard) {
    keyboard.keyboardInput.keyChangedHandler = ^(GCKeyboardInput* state, GCControllerButtonInput*, GCKeyCode code, BOOL pressed) {
#if TARGET_OS_TV
      if (code == GCKeyCodeEscape) {
          return;
      }
#endif
      const input::Key key = toKey(code);
      if (key != input::Key::Unknown) {
          SokolRuntime::handleEvent({.type = pressed ? Event::Type::KeyDown : Event::Type::KeyUp, .key = key, .modifiers = toModifiers(state)});
      }
    };
}

} // namespace haylen::platform
#endif
