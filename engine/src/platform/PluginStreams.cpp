#include "haylen/platform/PluginStreams.hpp"

#include <stdexcept>

namespace haylen::platform {

std::mutex& PluginStreams::mutex = *new std::mutex();
std::map<std::string, std::shared_ptr<VideoStream>, std::less<>>& PluginStreams::videos = *new std::map<std::string, std::shared_ptr<VideoStream>, std::less<>>();
std::map<std::string, std::shared_ptr<AudioStream>, std::less<>>& PluginStreams::audios = *new std::map<std::string, std::shared_ptr<AudioStream>, std::less<>>();

std::string PluginStreams::makeKey(std::string_view plugin, std::string_view name) {
    if (plugin.empty() || name.empty()) {
        throw std::invalid_argument("A stream needs the id of its plugin and a name.");
    }
    return std::string(plugin) + "." + std::string(name);
}

std::shared_ptr<VideoStream> PluginStreams::openVideo(std::string_view plugin, std::string_view name, VideoStream::Format format, int width, int height) {
    std::string key = makeKey(plugin, name);
    const std::scoped_lock lock(mutex);
    if (const auto found = videos.find(key); found != videos.end()) {
        if (found->second->getFormat() != format) {
            throw std::invalid_argument("The video stream \"" + key + "\" is open with another format.");
        }
        return found->second;
    }
    auto stream = std::make_shared<VideoStream>(format, width, height);
    videos.emplace(std::move(key), stream);
    return stream;
}

std::shared_ptr<VideoStream> PluginStreams::findVideo(std::string_view plugin, std::string_view name) {
    const std::string key = makeKey(plugin, name);
    const std::scoped_lock lock(mutex);
    const auto found = videos.find(key);
    return found != videos.end() ? found->second : nullptr;
}

std::shared_ptr<AudioStream> PluginStreams::openAudio(std::string_view plugin, std::string_view name, std::uint32_t sampleRate, std::uint32_t channels, AudioStream::Format format, std::size_t capacityFrames) {
    std::string key = makeKey(plugin, name);
    const std::scoped_lock lock(mutex);
    if (const auto found = audios.find(key); found != audios.end()) {
        const AudioStream& open = *found->second;
        if (open.getSampleRate() != sampleRate || open.getChannels() != channels || open.getFormat() != format) {
            throw std::invalid_argument("The audio stream \"" + key + "\" is open with another sample rate, channel count or format.");
        }
        return found->second;
    }
    auto stream = std::make_shared<AudioStream>(sampleRate, channels, format, capacityFrames);
    audios.emplace(std::move(key), stream);
    return stream;
}

std::shared_ptr<AudioStream> PluginStreams::findAudio(std::string_view plugin, std::string_view name) {
    const std::string key = makeKey(plugin, name);
    const std::scoped_lock lock(mutex);
    const auto found = audios.find(key);
    return found != audios.end() ? found->second : nullptr;
}

} // namespace haylen::platform
