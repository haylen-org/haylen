#pragma once

#include <optional>

#include "haylen/platform/MediaSession.hpp"

namespace haylen::platform {

// The now-playing controls of Apple platforms: the info center and the remote commands of MediaPlayer, which the runtime loads at run time and reaches through the Objective-C runtime, so no app links MediaPlayer for it. The commands of the controls, the media keys of a Mac and the buttons of headsets and remotes reach `MediaSession::receive` on the main thread.
class AppleMediaSession final {
  public:
    static void update(const std::optional<MediaSession::NowPlaying>& value);

  private:
    // Loads MediaPlayer once and returns whether it is there.
    [[nodiscard]] static bool load();

    // Gives every remote command a handler once, and turns on the commands that the app offers.
    static void enableCommands(MediaSession::Commands commands);
    [[nodiscard]] static id makeInfo(const MediaSession::NowPlaying& value);

    static void* library;
    static bool registered;
};

} // namespace haylen::platform
