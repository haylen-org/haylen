#pragma once

#include <android/native_activity.h>

#include <atomic>
#include <mutex>

#include "haylen/math/Insets.hpp"
#include "haylen/platform/Orientation.hpp"

namespace haylen::platform {

// The native activity of the app and what its Java side reports: the safe area and the orientation of the screen, set on the UI thread, whether the device is a TV, and whether the app has drawn, which the splash screen reads.
class AndroidActivity final {
  public:
    [[nodiscard]] static const ANativeActivity& getNative();

    static void setSafeAreaInsets(const math::Insets& value);
    [[nodiscard]] static math::Insets getSafeAreaInsets();

    static void setOrientation(Orientation value) noexcept;
    [[nodiscard]] static Orientation getOrientation() noexcept;

    // A TV has no touch screen, so the player drives the app with a remote or a gamepad.
    static void setTelevision(bool value) noexcept;
    [[nodiscard]] static bool isTelevision() noexcept;

    // Lets the splash screen of the activity end once the app has drawn a frame, so nothing black shows between the two. The runtime calls it after every frame.
    static void endSplashScreen() noexcept;

    // Keeps the splash screen of a new activity until the app draws in it, since the process outlives its activities.
    static void holdSplashScreen() noexcept;
    [[nodiscard]] static bool isFramePresented() noexcept;

  private:
    static std::mutex mutex;
    static math::Insets safeAreaInsets;
    static std::atomic<bool> framePresented;
    static std::atomic<bool> television;
    static std::atomic<Orientation> orientation;
};

} // namespace haylen::platform
