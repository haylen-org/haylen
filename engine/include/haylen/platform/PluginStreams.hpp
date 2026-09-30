#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "haylen/platform/AudioStream.hpp"
#include "haylen/platform/VideoStream.hpp"

namespace haylen::platform {

// The video and audio streams that the native parts of plugins open, by the id of the plugin and a name, which the app reaches through the handle of the plugin. Streams belong to the process, like the native code that pushes into them, so they outlive the apps that restart in it and native code keeps their handles for good. Every method is thread-safe.
class PluginStreams final {
  public:
    // Returns the video stream of the key, which opens with the format and the size the first time. Throws std::invalid_argument for an empty id or name, a negative size, or a stream of the key that is open with another format.
    static std::shared_ptr<VideoStream> openVideo(std::string_view plugin, std::string_view name, VideoStream::Format format, int width, int height);
    [[nodiscard]] static std::shared_ptr<VideoStream> findVideo(std::string_view plugin, std::string_view name);

    // Returns the audio stream of the key, which opens with the sample rate, the channels, the format and room for capacityFrames frames the first time. Throws std::invalid_argument for an empty id or name, a sample rate, channels or a capacity of 0, or a stream of the key that is open with another rate, channel count or format.
    static std::shared_ptr<AudioStream> openAudio(std::string_view plugin, std::string_view name, std::uint32_t sampleRate, std::uint32_t channels, AudioStream::Format format, std::size_t capacityFrames);
    [[nodiscard]] static std::shared_ptr<AudioStream> findAudio(std::string_view plugin, std::string_view name);

  private:
    // Plugin ids have no dots, so the id and the name joined by a dot, like the methods of plugins, name one stream.
    [[nodiscard]] static std::string makeKey(std::string_view plugin, std::string_view name);

    static std::mutex& mutex;
    static std::map<std::string, std::shared_ptr<VideoStream>, std::less<>>& videos;
    static std::map<std::string, std::shared_ptr<AudioStream>, std::less<>>& audios;
};

} // namespace haylen::platform
