#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/platform/Orientation.hpp"

namespace haylen::platform {

// The orientations that an iPhone or iPad app allows, as the bits of `UIInterfaceOrientationMask`, which its application delegate and the view controller of its window both answer UIKit with, so UIKit never lays the window out in an orientation the app refuses. Before the app locks the orientation they are those that the Info.plist lists from `app.json`, and afterwards those of the lock.
class SupportedOrientations final {
  public:
    static constexpr std::uint32_t kPortrait = 1U << 1U;
    static constexpr std::uint32_t kPortraitUpsideDown = 1U << 2U;
    static constexpr std::uint32_t kLandscapeRight = 1U << 3U;
    static constexpr std::uint32_t kLandscapeLeft = 1U << 4U;
    static constexpr std::uint32_t kLandscape = kLandscapeLeft | kLandscapeRight;
    static constexpr std::uint32_t kAll = kPortrait | kPortraitUpsideDown | kLandscape;

    // Reads the names of `UISupportedInterfaceOrientations`, such as `UIInterfaceOrientationLandscapeLeft`. A list without a known name allows every orientation, as UIKit does.
    [[nodiscard]] static std::uint32_t fromInfoPlist(std::span<const std::string_view> names) noexcept;

    // The orientations of a lock. Phones never turn upside down, as the system keeps calls in the natural orientation there.
    [[nodiscard]] static std::uint32_t fromLock(Orientation value, bool pad) noexcept;

    [[nodiscard]] std::uint32_t get() const noexcept {
        return locked != 0 ? locked : listed;
    }
    void setListed(std::uint32_t value) noexcept {
        listed = value;
    }
    void lock(Orientation value, bool pad) noexcept {
        locked = fromLock(value, pad);
    }

  private:
    static constexpr std::array<std::pair<std::string_view, std::uint32_t>, 4> kNames{{
        {"UIInterfaceOrientationPortrait", kPortrait},
        {"UIInterfaceOrientationPortraitUpsideDown", kPortraitUpsideDown},
        {"UIInterfaceOrientationLandscapeLeft", kLandscapeLeft},
        {"UIInterfaceOrientationLandscapeRight", kLandscapeRight},
    }};

    std::uint32_t listed = kAll;
    std::uint32_t locked = 0;
};

} // namespace haylen::platform
