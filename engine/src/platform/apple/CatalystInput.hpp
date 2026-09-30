#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_MACCATALYST
#import <GameController/GameController.h>
#import <UIKit/UIKit.h>

#include "haylen/input/Key.hpp"
#include "haylen/input/KeyModifiers.hpp"
#include "haylen/input/MouseButton.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// The keyboard and mouse of Mac Catalyst, which turns the left mouse button into touches that `sokol_app` passes on and passes nothing else from them. `GCKeyboard` supplies the keys, `GCMouse` the other buttons and the wheel, and a hover gesture on the view of the app the pointer position.
class CatalystInput final {
  public:
    // Starts watching for the keyboard and the mouse, which GameController announces after launch and whenever they connect again. The hover gesture joins the view of the app once its window shows, and never the windows of screens.
    static void observe();

    // Follows the pointer over the view of the app, because `GCMouse` reports only how far it moves.
    static void trackPointer(UIHoverGestureRecognizer* recognizer);

  private:
    static math::Vec2 pointer;

    [[nodiscard]] static input::Key toKey(GCKeyCode code);
    [[nodiscard]] static input::KeyModifiers toModifiers(GCKeyboardInput* keyboard);
    static void observeKeyboard(GCKeyboard* keyboard);
    static void observeMouse(GCMouse* mouse);
    static void observeMouseButton(GCControllerButtonInput* button, input::MouseButton which);
};

} // namespace haylen::platform
#endif
