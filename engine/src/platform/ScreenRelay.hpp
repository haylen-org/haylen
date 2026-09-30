#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/platform/Bridge.hpp"

namespace haylen::platform {

// Keeps the screen of a plugin that shows over the app and carries the ends of screens to the engine. Both belong to the process, so a screen outlives the apps that restart under it, and an end that arrives while no app runs waits for the next app. Native code ends screens from any thread.
class ScreenRelay final {
  public:
    // A screen that shows. A screen that the app gave up, with a cancel or a timeout, ends without reaching any app.
    struct Screen {
        std::uint64_t id = 0;
        std::string plugin;
        std::string name;
        core::Json state;
        bool opaque = true;
        bool abandoned = false;
    };

    // How a screen ended, with the screen as it showed. A screen that the platform restored after the end of the process has the id 0.
    struct Ending {
        Screen screen;
        Bridge::Result result;
    };

    // Records the screen that shows from now on, which the engine does only while none shows.
    static void show(Screen screen);
    [[nodiscard]] static std::optional<Screen> getShowing();

    // Marks the screen given up, so its end reaches no app.
    static void abandon(std::uint64_t id);

    // Forgets a screen that never reached the platform, which therefore never ends it.
    static void forget(std::uint64_t id);

    // Thread-safe entry points for native code. The function `finish` ends the screen that shows with the same id, with a result or a failure the way a platform call is answered, and drops any other end. The function `restore` ends a screen that the platform kept across the end of the process, with the state it saved as JSON text.
    static void finish(std::uint64_t id, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers = {});
    static void restore(std::string plugin, std::string name, std::string_view stateJson, bool ok, std::string_view resultJson, std::vector<std::vector<std::byte>> buffers = {});

    // Hands the ends that arrived since the last call to the engine, in order.
    [[nodiscard]] static std::vector<Ending> take();

  private:
    static std::mutex& mutex;
    static std::optional<Screen>& showing;
    static std::vector<Ending>& endings;
};

} // namespace haylen::platform
