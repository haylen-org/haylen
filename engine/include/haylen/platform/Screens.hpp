#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/platform/ScreenRequest.hpp"

namespace haylen::platform {

class Host;

// Opens the screens of plugins, one at a time: native UI that takes over the app until it ends with one result, such as a paywall, a sign-in flow, a payment page or an activity of an SDK. The engine covers the app at the start of the frame after the request and only then hands the screen to the platform, so the app is inactive, halted and muted before the screen shows, and it draws nothing while an opaque screen shows. The result answers the call. A screen whose call no longer exists, because the app restarted or the process ended while it showed, sends its end to the next app as the retained event <plugin>.screenRestored with the screen, the state and the result, or the error instead of the result. Screen ids are unique in the whole process.
class Screens final {
  public:
    // The state is a JSON value that the platform keeps with the screen and hands back with a restored end. An opaque screen hides the app completely, and a timeout gives the screen up once it passes.
    struct Options {
        core::Json state;
        bool opaque = true;
        std::optional<std::chrono::steady_clock::duration> timeout;
    };

    using Callback = std::function<void(Bridge::Result)>;

    // Screens go to the native libraries first and to the platform through the host after them, and the ends of screens that earlier apps opened go out through the bridge. canOpen tells whether the app is active, the only state in which a screen opens.
    Screens(Host& owner, Bridge& channel, std::function<bool()> active);

    // Forgets the screen that the platform has not received yet, while a screen that shows keeps showing for the next app.
    ~Screens();

    Screens(const Screens&) = delete;
    Screens& operator=(const Screens&) = delete;

    // Asks the plugin to open its screen and returns the id of the screen. The callback runs once, in pump, with the result or with a failure: busy while another screen shows, notActive while the app is not active, cancelled or timeout when the app gave the screen up, and the failures of the platform, such as cancelled when the user closed the screen. Throws std::invalid_argument for an empty plugin or screen name, parameters that refer to a buffer they lack, or a timeout that is not positive.
    std::uint64_t open(std::string plugin, std::string screen, Bridge::Payload params, Options options, Callback callback);

    // Gives up a screen, which fails with the code cancelled at the next pump, and asks the platform to dismiss it. The app stays covered until the platform reports the screen gone. Returns false when the screen already settled.
    bool cancel(std::uint64_t id);

    // Whether the screen of a plugin shows over the app, whichever app of the process opened it.
    [[nodiscard]] bool isShowing() const;

    // Hands the screen that the app asked for to the platform, which the engine does at the start of a frame, right after the cover took hold.
    void present();

    void pump();
    [[nodiscard]] std::size_t getPendingCount() const noexcept;

  private:
    struct Pending {
        Callback callback;
        std::string plugin;
        std::string screen;
        std::optional<std::chrono::steady_clock::time_point> deadline;
    };

    static std::atomic<std::uint64_t> nextId;

    // Settles a screen that the app gave up, and dismisses it where it already reached the platform.
    void giveUp(std::uint64_t id, const Pending& entry);

    // Gives up the screens whose timeout passed.
    void expire();

    Host& host;
    Bridge& bridge;
    std::function<bool()> canOpen;
    std::unordered_map<std::uint64_t, Pending> pending;
    std::vector<std::pair<Callback, Bridge::Result>> failed;
    std::optional<ScreenRequest> requested;
};

} // namespace haylen::platform
