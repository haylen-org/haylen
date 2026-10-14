#include "platform/windows/WindowsMediaSession.hpp"

#include <roapi.h>
#include <shcore.h>
#include <shlwapi.h>
#include <systemmediatransportcontrolsinterop.h>
#include <windows.foundation.h>
#include <windows.storage.streams.h>
#include <wrl/event.h>
#include <wrl/wrappers/corewrappers.h>

#include <cmath>

#include "sokol_app.h"

namespace haylen::platform {

using ABI::Windows::Media::ISystemMediaTransportControls;
using ABI::Windows::Media::ISystemMediaTransportControls2;
using ABI::Windows::Media::ISystemMediaTransportControlsButtonPressedEventArgs;
using ABI::Windows::Media::ISystemMediaTransportControlsDisplayUpdater;
using ABI::Windows::Media::SystemMediaTransportControlsButton;
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

ComPtr<ISystemMediaTransportControls> WindowsMediaSession::controls;
bool WindowsMediaSession::attempted = false;
MediaSession::Commands WindowsMediaSession::offered = 0;

std::wstring WindowsMediaSession::toWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

// The play and pause buttons send the toggle of an app that offers only the toggle, since the controls have no toggle button and the play and pause key sends the one the status shows.
void WindowsMediaSession::receiveButton(SystemMediaTransportControlsButton button) {
    using Command = MediaSession::Command;
    // clang-format off
    const auto send = [](Command command) {
        const bool toggled = (command == Command::Play || command == Command::Pause) && (offered & MediaSession::bit(command)) == 0 && (offered & MediaSession::bit(Command::TogglePlayPause)) != 0;
        MediaSession::receive(toggled ? Command::TogglePlayPause : command, 0.0);
    };
    // clang-format on
    switch (button) {
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Play:
        send(Command::Play);
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Pause:
        send(Command::Pause);
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Stop:
        send(Command::Stop);
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Next:
        send(Command::Next);
        break;
    case ABI::Windows::Media::SystemMediaTransportControlsButton_Previous:
        send(Command::Previous);
        break;
    default:
        break;
    }
}

// The controls belong to the window, so they are made once its window exists, and an older system without them leaves the app out of the controls.
ISystemMediaTransportControls* WindowsMediaSession::getControls() {
    if (attempted) {
        return controls.Get();
    }
    attempted = true;
    const HRESULT started = RoInitialize(RO_INIT_SINGLETHREADED);
    if (FAILED(started) && started != RPC_E_CHANGED_MODE) {
        return nullptr;
    }
    ComPtr<ISystemMediaTransportControlsInterop> interop;
    const auto window = static_cast<HWND>(const_cast<void*>(sapp_win32_get_hwnd()));
    if (FAILED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControls).Get(), IID_PPV_ARGS(&interop))) || FAILED(interop->GetForWindow(window, IID_PPV_ARGS(&controls)))) {
        controls.Reset();
        return nullptr;
    }

    EventRegistrationToken token{};
    // clang-format off
    controls->add_ButtonPressed(Callback<ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs*>>([](ISystemMediaTransportControls*, ISystemMediaTransportControlsButtonPressedEventArgs* event) {
        SystemMediaTransportControlsButton button{};
        if (SUCCEEDED(event->get_Button(&button))) {
            receiveButton(button);
        }
        return S_OK;
    }).Get(), &token);
    ComPtr<ISystemMediaTransportControls2> seeking;
    if (SUCCEEDED(controls.As(&seeking))) {
        seeking->add_PlaybackPositionChangeRequested(Callback<ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::Media::SystemMediaTransportControls*, ABI::Windows::Media::PlaybackPositionChangeRequestedEventArgs*>>([](ISystemMediaTransportControls*, ABI::Windows::Media::IPlaybackPositionChangeRequestedEventArgs* event) {
            ABI::Windows::Foundation::TimeSpan position{};
            if ((offered & MediaSession::bit(MediaSession::Command::Seek)) != 0 && SUCCEEDED(event->get_RequestedPlaybackPosition(&position))) {
                MediaSession::receive(MediaSession::Command::Seek, static_cast<double>(position.Duration) / kUnitsPerSecond);
            }
            return S_OK;
        }).Get(), &token);
    }
    // clang-format on
    return controls.Get();
}

void WindowsMediaSession::update(const std::optional<MediaSession::NowPlaying>& value) {
    ISystemMediaTransportControls* found = getControls();
    if (found == nullptr) {
        return;
    }
    if (!value) {
        offered = 0;
        ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
        if (SUCCEEDED(found->get_DisplayUpdater(&updater))) {
            updater->ClearAll();
        }
        found->put_PlaybackStatus(ABI::Windows::Media::MediaPlaybackStatus_Closed);
        found->put_IsEnabled(false);
        return;
    }
    show(*found, *value);
}

void WindowsMediaSession::show(ISystemMediaTransportControls& target, const MediaSession::NowPlaying& value) {
    using Command = MediaSession::Command;
    offered = value.commands;
    const auto offers = [&value](Command command) -> boolean { return (value.commands & MediaSession::bit(command)) != 0; };
    const boolean toggles = offers(Command::TogglePlayPause);
    target.put_IsEnabled(true);
    target.put_IsPlayEnabled(offers(Command::Play) || toggles);
    target.put_IsPauseEnabled(offers(Command::Pause) || toggles);
    target.put_IsStopEnabled(offers(Command::Stop));
    target.put_IsNextEnabled(offers(Command::Next));
    target.put_IsPreviousEnabled(offers(Command::Previous));
    target.put_PlaybackStatus(value.playing ? ABI::Windows::Media::MediaPlaybackStatus_Playing : ABI::Windows::Media::MediaPlaybackStatus_Paused);

    ComPtr<ISystemMediaTransportControlsDisplayUpdater> updater;
    ComPtr<ABI::Windows::Media::IMusicDisplayProperties> music;
    if (FAILED(target.get_DisplayUpdater(&updater)) || FAILED(updater->put_Type(ABI::Windows::Media::MediaPlaybackType_Music)) || FAILED(updater->get_MusicProperties(&music))) {
        return;
    }
    const std::wstring title = toWide(value.title);
    const std::wstring artist = toWide(value.artist);
    const std::wstring album = toWide(value.album);
    music->put_Title(HStringReference(title.c_str(), static_cast<unsigned int>(title.size())).Get());
    music->put_Artist(HStringReference(artist.c_str(), static_cast<unsigned int>(artist.size())).Get());
    ComPtr<ABI::Windows::Media::IMusicDisplayProperties2> albums;
    if (SUCCEEDED(music.As(&albums))) {
        albums->put_AlbumTitle(HStringReference(album.c_str(), static_cast<unsigned int>(album.size())).Get());
    }
    showArtwork(*updater.Get(), value);
    updater->Update();
    showTimeline(target, value);
}

// The artwork reaches the controls as a stream of its bytes in memory.
void WindowsMediaSession::showArtwork(ISystemMediaTransportControlsDisplayUpdater& updater, const MediaSession::NowPlaying& value) {
    if (value.artwork.empty()) {
        updater.put_Thumbnail(nullptr);
        return;
    }
    ComPtr<IStream> memory;
    memory.Attach(SHCreateMemStream(reinterpret_cast<const BYTE*>(value.artwork.data()), static_cast<UINT>(value.artwork.size())));
    ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStream> stream;
    ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStreamReferenceStatics> references;
    ComPtr<ABI::Windows::Storage::Streams::IRandomAccessStreamReference> reference;
    if (memory && SUCCEEDED(CreateRandomAccessStreamOverStream(memory.Get(), BSOS_DEFAULT, IID_PPV_ARGS(&stream))) && SUCCEEDED(RoGetActivationFactory(HStringReference(RuntimeClass_Windows_Storage_Streams_RandomAccessStreamReference).Get(), IID_PPV_ARGS(&references))) && SUCCEEDED(references->CreateFromStream(stream.Get(), &reference))) {
        updater.put_Thumbnail(reference.Get());
    }
}

void WindowsMediaSession::showTimeline(ISystemMediaTransportControls& target, const MediaSession::NowPlaying& value) {
    ComPtr<ISystemMediaTransportControls2> seeking;
    ComPtr<IInspectable> created;
    ComPtr<ABI::Windows::Media::ISystemMediaTransportControlsTimelineProperties> timeline;
    if (value.duration <= 0.0 || FAILED(target.QueryInterface(IID_PPV_ARGS(&seeking))) || FAILED(RoActivateInstance(HStringReference(RuntimeClass_Windows_Media_SystemMediaTransportControlsTimelineProperties).Get(), &created)) || FAILED(created.As(&timeline))) {
        return;
    }
    const auto units = [](double seconds) { return ABI::Windows::Foundation::TimeSpan{.Duration = std::llround(seconds * kUnitsPerSecond)}; };
    timeline->put_StartTime(units(0.0));
    timeline->put_MinSeekTime(units(0.0));
    timeline->put_EndTime(units(value.duration));
    timeline->put_MaxSeekTime(units(value.duration));
    timeline->put_Position(units(std::min(value.position, value.duration)));
    seeking->UpdateTimelineProperties(timeline.Get());
}

} // namespace haylen::platform
