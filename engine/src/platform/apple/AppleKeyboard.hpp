#pragma once

#include <TargetConditionals.h>

#if !TARGET_OS_OSX
#import <GameController/GameController.h>

#include "haylen/input/Key.hpp"
#include "haylen/input/KeyModifiers.hpp"

namespace haylen::platform {

// The hardware keyboard of iPhone, iPad, Apple TV and Mac Catalyst apps, read through `GCKeyboard`, which reports every key with its release, by its place on the keyboard. The presses of UIKit carry only the keys of remotes and game controllers to the view of SDL, so no key arrives twice. On the Apple TV the Escape key reaches the app as the Menu press of the remote, so it goes back the way the remote does.
class AppleKeyboard final {
  public:
    // Starts watching for the keyboard, which GameController announces after launch and whenever it connects again.
    static void observe();

  private:
    [[nodiscard]] static input::Key toKey(GCKeyCode code);
    [[nodiscard]] static input::KeyModifiers toModifiers(GCKeyboardInput* keyboard);
    static void observeKeyboard(GCKeyboard* keyboard);
};

} // namespace haylen::platform
#endif
