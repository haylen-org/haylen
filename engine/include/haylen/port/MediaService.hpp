#pragma once

#include <optional>

#include "haylen/platform/MediaSession.hpp"

namespace haylen::port {

// The media controls of the system and the audio of an app in the background. A port registers it where its platform has now-playing controls or halts apps in the background unless they play audio, and the engine treats a platform without it as one that keeps every app running in the background and shows no controls.
class MediaService {
  public:
    MediaService() = default;
    MediaService(const MediaService&) = delete;
    MediaService& operator=(const MediaService&) = delete;
    virtual ~MediaService() = default;

    // Keeps the frames of an app that plays audio in the background running there, as `audio.background` of the app asks.
    virtual void setBackgroundAudio(bool value) = 0;

    // Shows what the app plays in the controls of the system, or takes the app out of them, which send their commands through `platform::MediaSession::receive`.
    virtual void setNowPlaying(const std::optional<platform::MediaSession::NowPlaying>& value) = 0;
};

} // namespace haylen::port
