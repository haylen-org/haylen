#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Polygon.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/platform/AccessibilityElement.hpp"
#include "haylen/platform/Display.hpp"
#include "haylen/platform/Fold.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/Permission.hpp"
#include "haylen/platform/ScreenRequest.hpp"
#include "haylen/platform/SystemFont.hpp"
#include "haylen/platform/SystemInfo.hpp"
#include "haylen/platform/VideoDecoder.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/platform/WindowStyle.hpp"
#include "haylen/platform/native/HaylenNative.h"
#include "haylen/port/Services.hpp"
#include "varn/dataview/DataView.h"

namespace haylen::io {
class Package;
class PackageReader;
} // namespace haylen::io

namespace haylen::platform {

// The services of the platform as the player reaches them. Every platform runtime, of the engine or of a port, installs the services of its platform by capability once, and each method answers through the service that has the capability or, where the platform registered none, in the one way that the service of `haylen::port` describes. The services live as long as the process, since other threads may still use them while it exits.
class Services final {
  public:
    // Throws `std::invalid_argument` when the platform or the displays are missing, or when services were installed already.
    static void install(port::Services value);

    [[nodiscard]] static std::string_view getName() noexcept;
    static void initialize();
    static void shutdown() noexcept;
    static void reportError(const core::Json& report);
    [[nodiscard]] static std::vector<std::string> getNativePlugins();
    [[nodiscard]] static std::shared_ptr<io::Package> openBundledPackage();
    [[nodiscard]] static std::filesystem::path getUserDataDirectory(std::string_view identifier);
    static void persistUserData();
    [[nodiscard]] static SystemInfo getSystemInfo();
    [[nodiscard]] static bool hasPointerDevice() noexcept;

    [[nodiscard]] static std::vector<Display> getDisplays();
    [[nodiscard]] static std::string getWindowDisplayId();
    [[nodiscard]] static bool hasUnlistedDisplays();
    static void requestEveryScreen(std::function<void(Permission::Answer answer)> callback);
    [[nodiscard]] static math::Rect getWindowFrame();

    [[nodiscard]] static bool hasDesktop() noexcept;
    static void setWindowStyle(const WindowStyle& value);
    static void setWindowBackground(const math::Color& value);
    static void setWindowFrame(const math::Rect& value);
    static void setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions);
    static void startWindowDrag();
    static void watchWindow();
    static void updateWindow();
    [[nodiscard]] static HaylenNativeWindow getNativeWindow();

    [[nodiscard]] static math::Insets getSafeAreaInsets();
    [[nodiscard]] static Orientation getOrientation();
    static void lockOrientation(Orientation value);
    static void setStatusBar(Window::StatusBar value);
    [[nodiscard]] static float getStatusBarHeight();
    static void setSystemGestures(Window::SystemGestures value);
    [[nodiscard]] static std::optional<Fold> getFold();
    static void getCutouts(std::vector<math::Rect>& cutouts);

    static void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views);
    static void cancel(std::uint64_t id);
    static void openScreen(const ScreenRequest& request);
    static void cancelScreen(std::uint64_t id);

    static void checkPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);
    static void requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback);

    [[nodiscard]] static std::string explainBlockedSocket(std::intptr_t socket);
    [[nodiscard]] static std::string getNetworkRequirement();

    [[nodiscard]] static bool forwardLink(std::string_view identifier, std::string_view url);
    static void listenForLinks(std::string_view identifier);

    static void setBackgroundAudio(bool value);
    static void setNowPlaying(const std::optional<MediaSession::NowPlaying>& value);

    [[nodiscard]] static bool isAccessibilityTreeWanted();
    static void updateAccessibility(std::span<const AccessibilityElement> elements, std::uint64_t focused);
    static void announce(std::string_view text, bool interrupt);

    [[nodiscard]] static std::unique_ptr<VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path);

    [[nodiscard]] static std::optional<SystemFont> findSystemFont(char32_t codePoint);
    [[nodiscard]] static std::vector<std::uint8_t> readSystemFont(const SystemFont& font);

    // The audio output that the platform registered, or null where it registered none.
    [[nodiscard]] static std::shared_ptr<port::AudioOutput> getAudioOutput();

  private:
    // Throws `std::logic_error` before the runtime installed the services, which no runtime of the engine or of a port reaches.
    [[nodiscard]] static const port::Services& getInstalled();

    static port::Services& installed;
};

} // namespace haylen::platform
