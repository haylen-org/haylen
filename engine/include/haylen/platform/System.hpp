#pragma once

#include <array>
#include <bitset>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/input/Input.hpp"
#include "haylen/platform/Battery.hpp"
#include "haylen/platform/SystemAnswer.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/Theme.hpp"

namespace haylen::core {
class EventBus;
}

namespace haylen::platform {

class Host;

// The operating system of the device as the app sees it: what the device is, its theme, its battery, its power saving mode, its time zone and the languages the person prefers, and the services it offers apps. The engine owns it and publishes `systemThemeChanged`, `batteryChanged`, `powerSavingChanged`, `timeZoneChanged` and `systemLanguagesChanged` once per change. Every method runs on the frame thread, and so does every callback.
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

    // Whether the power saving mode of the system is on, or empty where the platform does not tell.
    [[nodiscard]] std::optional<bool> getPowerSaving() const noexcept;

    // The IANA name of the time zone of the device, such as `America/Sao_Paulo`, or an empty text where the platform does not tell.
    [[nodiscard]] const std::string& getTimeZone() const noexcept;
    // The languages the person prefers on the system, most preferred first, in the canonical form of BCP 47, such as `pt-BR`. The list is empty where the platform tells none.
    [[nodiscard]] const std::vector<std::string>& getLanguages() const noexcept;

    // The text on the clipboard for a paste of the person into a field of the UI, or an empty string when it holds no text.
    [[nodiscard]] std::string getClipboard() const;

    // Reads the text on the clipboard for the app and calls back at a later pump with `Granted` and the text, or with `Denied` or `Unsupported` and the sentence that says why. Some platforms ask the person before an app reads it, such as iOS and browsers.
    void readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback);

    // Puts the text on the clipboard of the system and calls back at a later pump with `Granted` once it got there, or with `Denied` or `Unsupported` and the sentence that says why. A browser refuses a page outside a secure context or without a recent input of the person, such as a click or a key.
    void setClipboard(std::string_view text, std::function<void(SystemAnswer answer)> callback);

    // Runs the low and the high frequency motors of a gamepad, each from 0 to 1, for the seconds, where the gamepad and the platform rumble. Throws `std::invalid_argument` for a gamepad outside `input::Input::kMaxGamepads`, an intensity outside 0 to 1 or seconds that are not positive.
    void rumbleGamepad(std::size_t index, float low, float high, float seconds);

    // Stops every gamepad that rumbles, which the engine does when the app loses the focus or stops.
    void stopRumbles();

    // Captures the native crashes of the process into the report file, where the platform catches them, with the JSON object of the context that the frame thread publishes last.
    void guardCrashes(const std::filesystem::path& report);
    void setCrashContext(std::string_view json);

    // Keeps the callback of a call that a platform may gate until its answer, and returns what the platform calls with the answer from any thread, which reaches the callback at the next pump, such as the answers of the fullscreen of the window.
    [[nodiscard]] std::function<void(SystemAnswer)> awaitAnswer(std::function<void(SystemAnswer)> callback);

    // Delivers the answers the platform gave since the last pump and publishes the changes of the theme, the battery, the power saving, the time zone and the languages. The engine calls it at the start of every frame.
    void pump();

    [[nodiscard]] static std::string_view themeName(Theme value) noexcept;

  private:
    using Callback = std::function<void(SystemAnswer answer, std::string text)>;

    // The answer of the platform to a call, with the text that a read of the clipboard brings.
    struct Reply {
        std::uint64_t id = 0;
        SystemAnswer answer;
        std::string text;
    };

    // Answers wait here, shared with the callbacks the platform holds, so an answer that comes after the engine is gone touches nothing.
    struct Inbox {
        std::mutex mutex;
        std::vector<Reply> replies;
    };

    static const std::array<std::pair<std::string_view, Theme>, 2> kThemeNames;

    // Keeps the callback of a call until its answer, and returns what the platform calls with the answer, which waits for the next pump.
    [[nodiscard]] Callback awaitReply(Callback callback);

    // Publishes what the platform reported since the last pump, each value once per change.
    void publishChanges();

    Host& host;
    core::EventBus& events;
    SystemInfo info;
    Theme theme;
    Battery battery;
    std::optional<bool> powerSaving;
    std::string timeZone;
    std::vector<std::string> languages;
    std::uint64_t languagesRevision = 0;
    std::bitset<input::Input::kMaxGamepads> rumbling;
    std::uint64_t nextRequest = 1;
    std::unordered_map<std::uint64_t, Callback> pending;
    std::shared_ptr<Inbox> inbox = std::make_shared<Inbox>();
};

} // namespace haylen::platform
