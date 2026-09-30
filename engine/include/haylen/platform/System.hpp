#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/platform/Battery.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/Theme.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::platform {

class Host;

// The operating system of the device as the app sees it: what the device is, its theme and its battery, and the services it offers apps. The engine owns it and publishes `systemThemeChanged` and `batteryChanged` once per change. Every method runs on the frame thread, and so does every callback.
class System final {
    struct Inbox;

  public:
    // The info comes from the platform, with the name of the GPU the graphics device runs on.
    System(Host& host, core::EventBus& events, std::string gpuName);

    System(const System&) = delete;
    System& operator=(const System&) = delete;

    [[nodiscard]] const SystemInfo& getInfo() const noexcept;
    [[nodiscard]] Theme getTheme() const noexcept;
    [[nodiscard]] const Battery& getBattery() const noexcept;

    // Opens the url with the app the system picks for it, such as the browser for a web page, and calls back at a later pump with whether an app took it. Throws `std::invalid_argument` for an empty url.
    void openUrl(std::string_view url, std::function<void(bool opened)> callback);

    // Vibrates the device for the given seconds where it can vibrate. Throws `std::invalid_argument` unless the seconds are positive.
    void vibrate(float seconds);

    // Delivers the answers the platform gave since the last pump and publishes the changes of the theme and the battery. The engine calls it at the start of every frame.
    void pump();

    [[nodiscard]] static std::string_view themeName(Theme value) noexcept;

  private:
    struct Answer {
        std::uint64_t id = 0;
        bool opened = false;
    };

    // Answers wait here, shared with the callbacks the platform holds, so an answer that comes after the engine is gone touches nothing.
    struct Inbox {
        std::mutex mutex;
        std::vector<Answer> answers;
    };

    static const std::array<std::pair<std::string_view, Theme>, 2> kThemeNames;

    Host& host;
    core::EventBus& events;
    SystemInfo info;
    Theme theme;
    Battery battery;
    std::uint64_t nextUrl = 1;
    std::unordered_map<std::uint64_t, std::function<void(bool)>> pending;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
};

} // namespace haylen::platform
