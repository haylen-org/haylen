#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/platform/VideoDecoder.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {

class Device;

// A video file that plays into a texture, which sprites, UI images and materials draw like any other, with its sound through the mixer and a bus. The decoder of the platform opens on the I/O pool and decodes ahead of the playback on the task pool, and the frame thread only presents the frame of the current time, so neither decoding nor opening ever holds a frame up. The texture changes once per frame at most. Every method runs on the frame thread, and so does every signal.
class Video final {
  public:
    enum class State : std::uint8_t {
        Loading,
        Ready,
        Failed,
    };

    struct Options {
        std::string bus = "music";
        float volume = 1.0F;
        bool muted = false;
        bool loop = false;
        float rate = 1.0F;

        // Plays as soon as the video is ready.
        bool autoplay = false;

        // Whether the video plays while the engine is paused, for its picture and its sound, where `Inherit` plays as `Pausable`.
        core::ProcessMode processMode = core::ProcessMode::Inherit;
    };

    // Opens the decoder on a worker of the I/O pool. It returns null where the platform has no decoder and throws for a file it cannot decode.
    using Opener = std::function<std::unique_ptr<platform::VideoDecoder>()>;

    static constexpr float kMinimumRate = 0.25F;
    static constexpr float kMaximumRate = 4.0F;

    // Throws `std::invalid_argument` for a rate outside `kMinimumRate` to `kMaximumRate` or a volume that is negative. The video loads once `load` hands it the opener of its decoder.
    Video(core::JobSystem& jobs, Device& device, audio::Mixer& mixer, const Options& options);

    // Stops the sound. Decoding that still runs finishes on its worker and lets go of the decoder there.
    ~Video();

    Video(const Video&) = delete;
    Video& operator=(const Video&) = delete;

    // Opens the decoder on a worker of the I/O pool, once.
    void load(Opener opener);

    // Stops the sound and the decoding for good, such as when the owner of the video ends.
    void close();

    // Play from the current time, or from the start once the video ended, and pause where it is. A video that is still loading plays once it is ready.
    void play();
    void pause();

    // Moves the playback to the time in seconds, kept within the video, and shows the frame of that time as soon as it is decoded. Throws `std::invalid_argument` for a time that is not finite.
    void seek(double seconds);

    void setLoop(bool value) noexcept {
        loop = value;
    }
    [[nodiscard]] bool isLooping() const noexcept {
        return loop;
    }

    // The speed of the playback, whose sound plays faster and higher or slower and lower with it. Throws `std::invalid_argument` outside `kMinimumRate` to `kMaximumRate`.
    void setRate(float value);
    [[nodiscard]] float getRate() const noexcept {
        return rate;
    }

    // Throws `std::invalid_argument` for a negative volume.
    void setVolume(float value);
    [[nodiscard]] float getVolume() const noexcept {
        return volume;
    }
    void setMuted(bool value);
    [[nodiscard]] bool isMuted() const noexcept {
        return muted;
    }

    [[nodiscard]] State getState() const noexcept {
        return state;
    }

    // Why the video failed, such as a file the platform cannot decode, or empty.
    [[nodiscard]] const std::string& getError() const noexcept {
        return error;
    }

    // Whether the platform had no decoder for the video, which `failed` reports with the code `unsupported`.
    [[nodiscard]] bool isUnsupported() const noexcept {
        return unsupported;
    }
    [[nodiscard]] bool isPlaying() const noexcept {
        return playing;
    }
    [[nodiscard]] bool isEnded() const noexcept {
        return atEnd;
    }

    // The length and the size of the video, which are 0 until it is ready.
    [[nodiscard]] double getDuration() const noexcept;
    [[nodiscard]] int getWidth() const noexcept;
    [[nodiscard]] int getHeight() const noexcept;
    [[nodiscard]] double getTime() const noexcept {
        return time;
    }
    [[nodiscard]] core::ProcessMode getProcessMode() const noexcept {
        return processMode;
    }

    // How many frames the texture showed since the video opened.
    [[nodiscard]] std::uint64_t getFrameCount() const noexcept {
        return frameCount;
    }

    // The texture of the picture, transparent until the first frame shows, which keeps its handle when the video loops or seeks.
    [[nodiscard]] const Texture& getTexture() const noexcept {
        return texture;
    }

    // Advances the playback by the real seconds of a frame while `running`, which is whether its process mode runs this frame, presents the frame of the current time and keeps the decoding ahead. The video plugin calls it at the start of every frame.
    void update(double seconds, bool running);

    core::Signal<> ready;
    core::Signal<> ended;
    core::Signal<const std::string&> failed;

  private:
    struct Shared;

    // How many frames the decoding keeps ready ahead of the playback, and how far ahead of the next frame the sound is decoded.
    static constexpr std::size_t kFramesAhead = 4;
    static constexpr double kSoundAhead = 0.3;

    // Decodes on a worker of the task pool until enough frames wait or the video ends.
    static void decode(const std::shared_ptr<Shared>& shared);
    static void pushSound(Shared& shared);

    void finishOpening(std::string failure);
    void fail();
    void present(platform::VideoDecoder::Frame& frame);
    void updateClocked();
    void restartSound();
    void stopSound();
    void reachEnd();
    void schedule();

    core::JobSystem& jobs;
    Device& device;
    audio::Mixer& mixer;
    std::shared_ptr<Shared> shared;
    Texture texture;
    std::string bus;
    std::string error;
    State state = State::Loading;
    double time = 0.0;
    std::uint64_t frameCount = 0;
    audio::Mixer::VoiceId voice = 0;
    core::ProcessMode processMode;
    float volume;
    float rate;
    bool muted;
    bool loop;
    bool playing;
    bool atEnd = false;
    bool unsupported = false;
    bool controlledPlaying = false;
    float controlledRate = 0.0F;
};

} // namespace haylen::graphics
