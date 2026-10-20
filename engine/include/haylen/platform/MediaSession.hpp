#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::core {
class EventBus;
}

namespace haylen::platform {

class Host;

// What the app plays, which the system shows in its now-playing and lock-screen controls and whose media keys, headset buttons and remote commands reach the app as `mediaCommand` events. The engine owns it, every method runs on the frame thread, and the commands of the platform wait until the next pump.
class MediaSession final {
  public:
    enum class Command : std::uint8_t {
        Play,
        Pause,
        TogglePlayPause,
        Stop,
        Next,
        Previous,
        Seek,
    };

    // The controls the system offers, as bits of `Command`.
    using Commands = std::uint8_t;

    // What plays now: the texts the controls show, the duration and the position in seconds with zero for a duration that is not known, whether it plays, the bytes of a PNG or JPEG artwork and the commands the controls offer.
    struct NowPlaying {
        std::string title;
        std::string artist;
        std::string album;
        double duration = 0.0;
        double position = 0.0;
        bool playing = false;
        std::vector<std::byte> artwork;
        Commands commands = kAllCommands;
    };

    static constexpr std::size_t kCommandCount = 7;
    static constexpr Commands kAllCommands = (1U << kCommandCount) - 1U;

    static constexpr std::array<std::pair<std::string_view, Command>, kCommandCount> kCommandNames{{
        {"play", Command::Play},
        {"pause", Command::Pause},
        {"togglePlayPause", Command::TogglePlayPause},
        {"stop", Command::Stop},
        {"next", Command::Next},
        {"previous", Command::Previous},
        {"seek", Command::Seek},
    }};

    MediaSession(Host& host, core::EventBus& events);
    ~MediaSession();

    MediaSession(const MediaSession&) = delete;
    MediaSession& operator=(const MediaSession&) = delete;

    // Shows what plays in the controls of the system, replacing what showed before. Throws `std::invalid_argument` for a negative duration or position.
    void setNowPlaying(NowPlaying value);

    // Takes the app out of the controls of the system, which the engine also does when it stops.
    void clear();

    [[nodiscard]] const std::optional<NowPlaying>& getNowPlaying() const noexcept {
        return nowPlaying;
    }

    // Queues a command of the platform from any thread, with the position of a seek in seconds. Commands of an app that is gone are dropped.
    static void receive(Command command, double position);

    // Publishes the commands the platform sent since the last pump as `mediaCommand` events. The engine calls it at the start of every frame.
    void pump();

    // The track as the JSON text that the media sessions of Android and the web receive it in, without its artwork, where the bytes of a text that is no UTF-8 become replacement characters.
    [[nodiscard]] static std::string writeNowPlaying(const NowPlaying& value);

    [[nodiscard]] static std::optional<Command> commandFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view commandName(Command value) noexcept;
    [[nodiscard]] static constexpr Commands bit(Command value) noexcept {
        return static_cast<Commands>(1U << static_cast<unsigned>(value));
    }

  private:
    struct Received {
        Command command = Command::Play;
        double position = 0.0;
    };

    static std::mutex& mutex;
    static MediaSession* active;

    Host& host;
    core::EventBus& events;
    std::optional<NowPlaying> nowPlaying;
    std::vector<Received> received;
    std::vector<Received> delivering;
};

} // namespace haylen::platform
