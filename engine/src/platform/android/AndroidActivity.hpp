#pragma once

#include <game-activity/GameActivity.h>

#include <atomic>
#include <mutex>
#include <optional>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/Orientation.hpp"

namespace haylen::platform {

// The `GameActivity` of the app and what its Java side reports: the safe area, the orientation and the fold of the screen, set on the UI thread, whether the device is a TV, and whether the app has drawn, which the splash screen reads.
class AndroidActivity final {
  public:
    [[nodiscard]] static const GameActivity& getNative();

    static void setSafeAreaInsets(const math::Insets& value);
    [[nodiscard]] static math::Insets getSafeAreaInsets();

    // Turn the window pixels that the Java side reports into framebuffer pixels, which are fewer when the app turns high DPI off. They run on the frame thread, which owns the size of the framebuffer.
    [[nodiscard]] static math::Insets toFramebuffer(const math::Insets& value) noexcept;
    [[nodiscard]] static math::Rect toFramebuffer(const math::Rect& value) noexcept;

    // The pixels of the display per density-independent pixel, which the activity reports when it starts and whenever the configuration changes.
    static void setDensity(float value) noexcept;
    [[nodiscard]] static float getDensity() noexcept;

    static void setOrientation(Orientation value) noexcept;
    [[nodiscard]] static Orientation getOrientation() noexcept;

    // What Jetpack WindowManager reports of the fold of a foldable device across the window: whether there is one, its bounds and how it holds.
    struct FoldReport {
        bool present = false;
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        bool vertical = false;
        bool halfOpened = false;
        bool separating = false;
        bool occluding = false;
    };

    // Keeps the fold that Jetpack WindowManager reports on the UI thread whenever the layout of the window changes.
    static void receiveFold(const FoldReport& report);
    [[nodiscard]] static std::optional<Fold> getFold();

    // A TV has no touch screen, so the player drives the app with a remote or a gamepad.
    static void setTelevision(bool value) noexcept;
    [[nodiscard]] static bool isTelevision() noexcept;

    // Lets the splash screen of the activity fade out over the seconds once the app has drawn a frame, so nothing black shows between the two.
    static void endSplashScreen(float fadeOutSeconds) noexcept;
    [[nodiscard]] static float getSplashFadeOut() noexcept;

    // Keeps the splash screen of a new activity until the app draws in it, since the process outlives its activities.
    static void holdSplashScreen() noexcept;
    [[nodiscard]] static bool isFramePresented() noexcept;

  private:
    static std::mutex& mutex;
    static math::Insets safeAreaInsets;
    static std::optional<Fold> fold;
    static std::atomic<bool> framePresented;
    static std::atomic<float> splashFadeOut;
    static std::atomic<bool> television;
    static std::atomic<float> density;
    static std::atomic<Orientation> orientation;
};

} // namespace haylen::platform
