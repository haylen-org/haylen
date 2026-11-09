#pragma once

#include <functional>
#include <optional>
#include <string>

#include <SDL3/SDL_video.h>

#include "haylen/platform/Permission.hpp"

namespace haylen::platform::sdl {

// The change of fullscreen that the app asked for and that the window has not reported yet. Systems such as macOS animate the window into its mode after SDL returns, so the answer waits until SDL reports that the window entered or left fullscreen. A window that covers another display first leaves the fullscreen it holds, and the request keeps that display until SDL reports that the window left. A newer request replaces the one that waits, and a window that closes ends it, so every answer arrives exactly once.
class FullscreenRequest final {
  public:
    using Callback = std::function<void(Permission::Answer answer)>;

    // The display that the window covers and the exclusive mode it covers it in, with the id of the display that the app named.
    struct Destination {
        SDL_DisplayID display = 0;
        std::optional<SDL_DisplayMode> mode;
        std::string displayId;
    };

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

    // Whether the request waits for the window to leave the fullscreen it holds before it covers its destination.
    [[nodiscard]] bool isLeaving() const noexcept {
        return destination.has_value();
    }

    // Waits for the window to enter fullscreen or to leave it, and answers the request that waited before with `Denied`.
    void begin(bool fullscreen, Callback callback);

    // Waits for the window to leave the fullscreen it holds and then to cover the destination, and answers the request that waited before with `Denied`.
    void beginLeaving(Destination value, Callback callback);

    // Hands over the destination of a request that waited for the window to leave fullscreen, which then waits for the window to enter it.
    [[nodiscard]] Destination takeDestination();

    // Answers the request that waits with `Granted` when the window reached the mode it asked for.
    void reach(bool fullscreen);

    // Answers the request that waits with the answer, such as a system that refused the change.
    void fail(Permission::Answer answer);

  private:
    void settle(Permission::Answer answer);

    std::optional<bool> wanted;
    std::optional<Destination> destination;
    Callback waiting;
};

} // namespace haylen::platform::sdl
