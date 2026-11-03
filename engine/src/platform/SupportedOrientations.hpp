#pragma once

#include <cstdint>

#include "haylen/platform/Orientation.hpp"

namespace haylen::platform {

// The orientations that an iPhone or iPad app allows, as the bits of `UIInterfaceOrientationMask`, which its application delegate and the view controller of its window both answer UIKit with, so UIKit never lays the window out in an orientation the app refuses. Before the app locks the orientation they are those of `app.json`, and afterwards those of the lock. The Info.plist lists every orientation, since a foldable iPhone shows an app that lists fewer in a box of the size of the outer screen on its inner screen.
class SupportedOrientations final {
  public:
    static constexpr std::uint32_t kPortrait = 1U << 1U;
    static constexpr std::uint32_t kPortraitUpsideDown = 1U << 2U;
    static constexpr std::uint32_t kLandscapeRight = 1U << 3U;
    static constexpr std::uint32_t kLandscapeLeft = 1U << 4U;
    static constexpr std::uint32_t kLandscape = kLandscapeLeft | kLandscapeRight;
    static constexpr std::uint32_t kAll = kPortrait | kPortraitUpsideDown | kLandscape;

    // The orientations of a lock. Phones never turn upside down, as the system keeps calls in the natural orientation there.
    [[nodiscard]] static std::uint32_t fromLock(Orientation value, bool pad) noexcept;

    [[nodiscard]] std::uint32_t get() const noexcept {
        return locked != 0 ? locked : listed;
    }

    // Allows the orientations of the app as it opens its window, until it locks another.
    void prepare(Orientation value, bool pad) noexcept {
        listed = fromLock(value, pad);
        locked = 0;
    }
    void lock(Orientation value, bool pad) noexcept {
        locked = fromLock(value, pad);
    }

  private:
    std::uint32_t listed = kAll;
    std::uint32_t locked = 0;
};

} // namespace haylen::platform
