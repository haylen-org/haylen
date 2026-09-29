#pragma once

#include <string_view>

namespace haylen::core {

// The names of the lifecycle events that the engine publishes on its event bus, shared by C++ and Lua. Payloads are described in docs/lifecycle.md.
class LifecycleEvent final {
  public:
    static constexpr std::string_view kAppStarted = "app_started";
    static constexpr std::string_view kAppActive = "app_active";
    static constexpr std::string_view kAppInactive = "app_inactive";
    static constexpr std::string_view kAppBackground = "app_background";
    static constexpr std::string_view kAppLowMemory = "app_low_memory";
    static constexpr std::string_view kAppQuitRequested = "app_quit_requested";
    static constexpr std::string_view kAppStopping = "app_stopping";

    static constexpr std::string_view kPaused = "paused";
    static constexpr std::string_view kUnpaused = "unpaused";

    static constexpr std::string_view kSceneLoading = "scene_loading";
    static constexpr std::string_view kSceneLoaded = "scene_loaded";
    static constexpr std::string_view kSceneLoadFailed = "scene_load_failed";
    static constexpr std::string_view kSceneEntered = "scene_entered";
    static constexpr std::string_view kSceneExited = "scene_exited";
    static constexpr std::string_view kSceneUnloaded = "scene_unloaded";
    static constexpr std::string_view kScenePaused = "scene_paused";
    static constexpr std::string_view kSceneResumed = "scene_resumed";
    static constexpr std::string_view kSceneExitTransitionStarted = "scene_exit_transition_started";
    static constexpr std::string_view kSceneEnterTransitionFinished = "scene_enter_transition_finished";
    static constexpr std::string_view kSceneCoverStarted = "scene_cover_started";
    static constexpr std::string_view kSceneCoverFinished = "scene_cover_finished";
    static constexpr std::string_view kSceneHoldStarted = "scene_hold_started";
    static constexpr std::string_view kSceneHoldFinished = "scene_hold_finished";
    static constexpr std::string_view kSceneRevealStarted = "scene_reveal_started";
    static constexpr std::string_view kSceneRevealFinished = "scene_reveal_finished";

    static constexpr std::string_view kPluginStarted = "plugin_started";
    static constexpr std::string_view kPluginStopped = "plugin_stopped";
    static constexpr std::string_view kAutoloadStarted = "autoload_started";
    static constexpr std::string_view kAutoloadStopped = "autoload_stopped";

    static constexpr std::string_view kWindowResized = "window_resized";
    static constexpr std::string_view kWindowFocusGained = "window_focus_gained";
    static constexpr std::string_view kWindowFocusLost = "window_focus_lost";
    static constexpr std::string_view kWindowFullscreenChanged = "window_fullscreen_changed";
    static constexpr std::string_view kWindowOrientationChanged = "window_orientation_changed";
    static constexpr std::string_view kWindowSafeAreaChanged = "window_safe_area_changed";

    static constexpr std::string_view kUiDocumentMounted = "ui_document_mounted";
    static constexpr std::string_view kUiDocumentUnmounted = "ui_document_unmounted";

    static constexpr std::string_view kGamepadConnected = "gamepad_connected";
    static constexpr std::string_view kGamepadDisconnected = "gamepad_disconnected";

    static constexpr std::string_view kAudioInterrupted = "audio_interrupted";
    static constexpr std::string_view kAudioResumed = "audio_resumed";
    static constexpr std::string_view kAudioRouteChanged = "audio_route_changed";

    static constexpr std::string_view kKeyboardShown = "keyboard_shown";
    static constexpr std::string_view kKeyboardHidden = "keyboard_hidden";

    static constexpr std::string_view kNetworkOnline = "network_online";
    static constexpr std::string_view kNetworkOffline = "network_offline";

    static constexpr std::string_view kWebSocketConnected = "websocket_connected";
    static constexpr std::string_view kWebSocketDisconnected = "websocket_disconnected";
    static constexpr std::string_view kWebSocketReconnecting = "websocket_reconnecting";

    static constexpr std::string_view kAssetLoaded = "asset_loaded";
    static constexpr std::string_view kAssetUnloaded = "asset_unloaded";
    static constexpr std::string_view kAssetReloaded = "asset_reloaded";

    static constexpr std::string_view kObjectCreated = "object_created";
    static constexpr std::string_view kObjectDestroyed = "object_destroyed";
};

} // namespace haylen::core
