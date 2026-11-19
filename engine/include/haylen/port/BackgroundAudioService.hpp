#pragma once

namespace haylen::port {

// The audio of an app in the background on a platform that halts apps in the background unless they declare to the system that they play audio there. A port registers it only for such a platform, and the engine treats a platform without it as one that keeps every app running in the background, whose output then keeps playing there.
class BackgroundAudioService {
  public:
    BackgroundAudioService() = default;
    BackgroundAudioService(const BackgroundAudioService&) = delete;
    BackgroundAudioService& operator=(const BackgroundAudioService&) = delete;
    virtual ~BackgroundAudioService() = default;

    // Whether the app declared to the system that it plays audio in the background, as its bundle or its manifest holds it.
    [[nodiscard]] virtual bool isDeclared() = 0;

    // Keeps the frames of an app that plays audio in the background running there, which the engine asks for only within what the app declared.
    virtual void setEnabled(bool value) = 0;
};

} // namespace haylen::port
