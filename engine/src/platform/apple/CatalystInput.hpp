#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_MACCATALYST
#import <GameController/GameController.h>
#import <UIKit/UIKit.h>

#include "haylen/input/MouseButton.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// The mouse of Mac Catalyst, which turns the left mouse button into touches that the view of the backend passes on and passes nothing else from the mouse. `GCMouse` supplies the other buttons and the wheel, and a hover gesture on the view of the app the pointer position, while `AppleKeyboard` reads the keyboard as on every UIKit platform.
class CatalystInput final {
  public:
    // Starts watching for the mouse, which GameController announces after launch and whenever it connects again. The hover gesture joins the view of the app once its window shows, and never the windows of screens.
    static void observe();

    // Follows the pointer over the view of the app, because `GCMouse` reports only how far it moves.
    static void trackPointer(UIHoverGestureRecognizer* recognizer);

  private:
    static math::Vec2 pointer;

    static void observeMouse(GCMouse* mouse);
    static void observeMouseButton(GCControllerButtonInput* button, input::MouseButton which);
};

} // namespace haylen::platform
#endif
