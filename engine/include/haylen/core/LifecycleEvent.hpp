#pragma once

#include <string_view>

namespace haylen::core {

// The names of the lifecycle events that the engine publishes on its event bus, shared by C++ and Lua. Payloads are described in docs/lifecycle.md.
class LifecycleEvent final {
  public:
    static constexpr std::string_view kAppStarted = "appStarted";
    static constexpr std::string_view kAppActive = "appActive";
    static constexpr std::string_view kAppInactive = "appInactive";
    static constexpr std::string_view kAppBackground = "appBackground";
    static constexpr std::string_view kAppLowMemory = "appLowMemory";
    static constexpr std::string_view kAppQuitRequested = "appQuitRequested";
    static constexpr std::string_view kAppStopping = "appStopping";

    static constexpr std::string_view kPaused = "paused";
    static constexpr std::string_view kUnpaused = "unpaused";

    static constexpr std::string_view kSceneLoading = "sceneLoading";
    static constexpr std::string_view kSceneLoaded = "sceneLoaded";
    static constexpr std::string_view kSceneLoadFailed = "sceneLoadFailed";
    static constexpr std::string_view kSceneEntered = "sceneEntered";
    static constexpr std::string_view kSceneExited = "sceneExited";
    static constexpr std::string_view kSceneUnloaded = "sceneUnloaded";
    static constexpr std::string_view kScenePaused = "scenePaused";
    static constexpr std::string_view kSceneResumed = "sceneResumed";
    static constexpr std::string_view kSceneExitTransitionStarted = "sceneExitTransitionStarted";
    static constexpr std::string_view kSceneEnterTransitionFinished = "sceneEnterTransitionFinished";
    static constexpr std::string_view kSceneCoverStarted = "sceneCoverStarted";
    static constexpr std::string_view kSceneCoverFinished = "sceneCoverFinished";
    static constexpr std::string_view kSceneHoldStarted = "sceneHoldStarted";
    static constexpr std::string_view kSceneHoldFinished = "sceneHoldFinished";
    static constexpr std::string_view kSceneRevealStarted = "sceneRevealStarted";
    static constexpr std::string_view kSceneRevealFinished = "sceneRevealFinished";

    static constexpr std::string_view kPluginStarted = "pluginStarted";
    static constexpr std::string_view kPluginStopped = "pluginStopped";
    static constexpr std::string_view kAutoloadStarted = "autoloadStarted";
    static constexpr std::string_view kAutoloadStopped = "autoloadStopped";

    static constexpr std::string_view kWindowResized = "windowResized";
    static constexpr std::string_view kWindowFocusGained = "windowFocusGained";
    static constexpr std::string_view kWindowFocusLost = "windowFocusLost";
    static constexpr std::string_view kWindowFullscreenChanged = "windowFullscreenChanged";
    static constexpr std::string_view kWindowOrientationChanged = "windowOrientationChanged";
    static constexpr std::string_view kWindowSafeAreaChanged = "windowSafeAreaChanged";
    static constexpr std::string_view kWindowMoved = "windowMoved";
    static constexpr std::string_view kWindowMonitorsChanged = "windowMonitorsChanged";

    static constexpr std::string_view kUiDocumentMounted = "uiDocumentMounted";
    static constexpr std::string_view kUiDocumentUnmounted = "uiDocumentUnmounted";

    static constexpr std::string_view kGamepadConnected = "gamepadConnected";
    static constexpr std::string_view kGamepadDisconnected = "gamepadDisconnected";

    static constexpr std::string_view kAudioInterrupted = "audioInterrupted";
    static constexpr std::string_view kAudioResumed = "audioResumed";
    static constexpr std::string_view kAudioRouteChanged = "audioRouteChanged";

    static constexpr std::string_view kKeyboardShown = "keyboardShown";
    static constexpr std::string_view kKeyboardHidden = "keyboardHidden";

    static constexpr std::string_view kNetworkOnline = "networkOnline";
    static constexpr std::string_view kNetworkOffline = "networkOffline";

    static constexpr std::string_view kWebSocketConnected = "webSocketConnected";
    static constexpr std::string_view kWebSocketDisconnected = "webSocketDisconnected";
    static constexpr std::string_view kWebSocketReconnecting = "webSocketReconnecting";

    static constexpr std::string_view kAssetLoaded = "assetLoaded";
    static constexpr std::string_view kAssetUnloaded = "assetUnloaded";
    static constexpr std::string_view kAssetReloaded = "assetReloaded";

    static constexpr std::string_view kObjectCreated = "objectCreated";
    static constexpr std::string_view kObjectDestroyed = "objectDestroyed";
};

} // namespace haylen::core
