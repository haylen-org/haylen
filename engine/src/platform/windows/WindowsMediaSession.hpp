#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <windows.media.h>
#include <wrl/client.h>

#include <atomic>
#include <optional>
#include <string>

#include "haylen/platform/MediaSession.hpp"

namespace haylen::platform {

// The media controls of Windows, the transport controls of the system for the window of the app, which the media keys of keyboards and headsets and the flyout of the volume reach. Their buttons and seeks reach `MediaSession::receive`.
class WindowsMediaSession final {
  public:
    static void update(const std::optional<MediaSession::NowPlaying>& value);

  private:
    // The controls of the window, made once with their handlers, or null where the system has none.
    [[nodiscard]] static ABI::Windows::Media::ISystemMediaTransportControls* getControls();
    static void show(ABI::Windows::Media::ISystemMediaTransportControls& controls, const MediaSession::NowPlaying& value);
    static void showTimeline(ABI::Windows::Media::ISystemMediaTransportControls& controls, const MediaSession::NowPlaying& value);
    static void showArtwork(ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater& updater, const MediaSession::NowPlaying& value);
    static void receiveButton(ABI::Windows::Media::SystemMediaTransportControlsButton button);
    [[nodiscard]] static std::wstring toWide(const std::string& text);

    // Media Foundation and the transport controls count time in units of 100 nanoseconds.
    static constexpr double kUnitsPerSecond = 10000000.0;

    static Microsoft::WRL::ComPtr<ABI::Windows::Media::ISystemMediaTransportControls> controls;
    static bool attempted;
    // The commands the app offers, which the frame thread writes and the handlers of the controls read on a thread of the system.
    static std::atomic<MediaSession::Commands> offered;
};

} // namespace haylen::platform
