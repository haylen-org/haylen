#pragma once

#include <array>
#include <cstdint>

namespace haylen::platform {

// Remembers which keys the app took from the views and the system of Android. The UI thread decides at once whether the app takes a key, and its press decides for its repeats and its release, so a release whose press went to a view, such as a focused text field, never reaches the app, and a release whose press reached the app always does. Key codes outside the range of Android go to the views. It compiles on every platform, so the tests of every host check it.
class AndroidKeyClaims final {
  public:
    static constexpr std::int32_t kKeyCodes = 512;

    enum class Action : std::uint8_t {
        Press,
        Repeat,
        Release,
    };

    // Returns whether the app takes the key, where `wanted` tells whether the app wants a new press of it.
    [[nodiscard]] bool take(std::int32_t code, Action action, bool wanted) noexcept;

  private:
    std::array<bool, kKeyCodes> taken{};
};

} // namespace haylen::platform
