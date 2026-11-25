#pragma once

#include <TargetConditionals.h>

#if TARGET_OS_IOS
#include <cstdint>
#include <functional>

#include "haylen/platform/SystemAnswer.hpp"

namespace haylen::platform {

// The pointer of a mouse or a trackpad over the window of an iPad or a Mac Catalyst app, which hides through the style of the pointer interaction of the view of the app and locks where the system lets the scene of the app keep it, as UIKit reports through the pointer lock state of the scene. An iPhone has no pointer that an app controls. Every method runs on the main thread, which is the frame thread of SDL.
class ApplePointer final {
  public:
    // Whether the device shows a pointer that the app may hide or lock, which iPads and Macs do.
    [[nodiscard]] static bool isControllable();

    // Asks UIKit again for the style of the pointer over the view of the app, which shows or hides with the cursor of SDL.
    static void refreshStyle();

    // Answers `Granted` once the scene of the window keeps the pointer locked, or `Denied` with the conditions of the system once `kLockSeconds` pass without the lock. A newer request answers in place of one that waits, which answers `Denied`.
    static void awaitLock(std::function<void(SystemAnswer answer)> callback);

  private:
    // How long UIKit takes at most to lock the pointer of a scene that it lets keep the lock, since it reports only the locks it grants.
    static constexpr double kLockSeconds = 1.0;

    [[nodiscard]] static bool isLocked();
    static void settle(SystemAnswer answer);

    static std::function<void(SystemAnswer answer)>& pending;
    static std::uint64_t request;
    static bool observing;
};

} // namespace haylen::platform
#endif
