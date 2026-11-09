#pragma once

#include <functional>
#include <optional>

#include "haylen/platform/Permission.hpp"

namespace haylen::platform::sdl {

// The change of fullscreen that the app asked for and that the window has not reported yet. Systems such as macOS animate the window into its mode after SDL returns, so the answer waits until SDL reports that the window entered or left fullscreen. A newer request replaces the one that waits, and a window that closes ends it, so every answer arrives exactly once.
class FullscreenRequest final {
  public:
    using Callback = std::function<void(Permission::Answer answer)>;

    FullscreenRequest() = default;
    FullscreenRequest(const FullscreenRequest&) = delete;
    FullscreenRequest& operator=(const FullscreenRequest&) = delete;
    ~FullscreenRequest();

    [[nodiscard]] bool isPending() const noexcept {
        return wanted.has_value();
    }

    // Whether the request that waits asks the window to enter fullscreen.
    [[nodiscard]] bool isEntering() const noexcept {
        return wanted.value_or(false);
    }

    // Waits for the window to enter fullscreen or to leave it, and answers the request that waited before with `Denied`.
    void begin(bool fullscreen, Callback callback);

    // Answers the request that waits with `Granted` when the window reached the mode it asked for.
    void reach(bool fullscreen);

    // Answers the request that waits with the answer, such as a system that refused the change.
    void fail(Permission::Answer answer);

  private:
    void settle(Permission::Answer answer);

    std::optional<bool> wanted;
    Callback waiting;
};

} // namespace haylen::platform::sdl
