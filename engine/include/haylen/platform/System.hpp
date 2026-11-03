#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/input/Input.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/Permission.hpp"
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

    // The text on the clipboard of the system, or an empty string when it holds no text.
    [[nodiscard]] std::string getClipboard() const;

    // Puts the text on the clipboard of the system and calls back at a later pump with whether it got there. A browser refuses a page outside a secure context or without a recent input of the person, such as a click or a key.
    void setClipboard(std::string_view text, std::function<void(bool written)> callback);

    // The state of a permission that the person grants at run time, as far as the platform tells it.
    [[nodiscard]] Permission::State getPermission(Permission::Kind kind) const;

    // Asks the person for a permission with the prompt of the system where the platform has one, and calls back at a later pump with the state the person left, or with the requirements the project of the app lacks, such as the line of the Android manifest that declares the permission.
    void requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);

    // Vibrates the device for the given seconds where it can vibrate. Throws `std::invalid_argument` unless the seconds are positive.
    void vibrate(float seconds);

    // Runs the low and the high frequency motors of a gamepad, each from 0 to 1, for the seconds, where the gamepad and the platform rumble. Throws `std::invalid_argument` for a gamepad outside `input::Input::kMaxGamepads`, an intensity outside 0 to 1 or seconds that are not positive.
    void rumbleGamepad(std::size_t index, float low, float high, float seconds);

    // Stops every gamepad that rumbles, which the engine does when the app loses the focus or stops.
    void stopRumbles();

    // Captures the native crashes of the process into the report file, where the platform catches them, with the JSON object of the context that the frame thread publishes last.
    void guardCrashes(const std::filesystem::path& report);
    void setCrashContext(std::string_view json);

    // Delivers the answers the platform gave since the last pump and publishes the changes of the theme and the battery. The engine calls it at the start of every frame.
    void pump();

    [[nodiscard]] static std::string_view themeName(Theme value) noexcept;

  private:
    struct Answer {
        std::uint64_t id = 0;
        bool succeeded = false;
    };

    // Answers wait here, shared with the callbacks the platform holds, so an answer that comes after the engine is gone touches nothing.
    struct Inbox {
        std::mutex mutex;
        std::vector<Answer> answers;
        std::vector<std::pair<std::uint64_t, Permission::Answer>> permissions;
    };

    static const std::array<std::pair<std::string_view, Theme>, 2> kThemeNames;

    // Keeps the callback of a request until its answer, and returns what the platform calls with the answer, which waits for the next pump.
    [[nodiscard]] std::function<void(bool)> awaitAnswer(std::function<void(bool)> callback);

    Host& host;
    core::EventBus& events;
    SystemInfo info;
    Theme theme;
    Battery battery;
    std::bitset<input::Input::kMaxGamepads> rumbling;
    std::uint64_t nextRequest = 1;
    std::unordered_map<std::uint64_t, std::function<void(bool)>> pending;
    std::unordered_map<std::uint64_t, std::function<void(Permission::Answer)>> pendingPermissions;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
};

} // namespace haylen::platform
