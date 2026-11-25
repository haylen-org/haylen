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

#include "haylen/port/Services.hpp"

namespace haylen::platform {

// The services of Android, which reach the activity through JNI: the platform, its displays, the screen of the activity with its insets, orientation, bars, gestures, fold and cutouts, the Android parts of plugins, why it blocked a connection, the audio in the background that the manifest declares, the accessibility tree, the video decoder of the media codecs and the fonts of the font matcher. The activity fills its display and hands the links that open the app to the activity that runs.
class AndroidServices final : public port::PlatformService, public port::DisplayService, public port::ScreenService, public port::BridgeService, public port::NetworkService, public port::BackgroundAudioService, public port::AccessibilityService, public port::VideoService, public port::FontService {
  public:
    [[nodiscard]] static port::Services create();

    [[nodiscard]] std::string_view getName() const noexcept override;
    void initialize() override;
    void shutdown() noexcept override;
    [[nodiscard]] std::shared_ptr<io::Package> openBundledPackage() override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override;
    [[nodiscard]] std::string getUserDataRefusal() override;
    [[nodiscard]] SystemInfo getSystemInfo() override;
    [[nodiscard]] bool hasPointerDevice() const noexcept override;
    [[nodiscard]] bool hasKeyRepeats() const noexcept override;

    [[nodiscard]] std::vector<Display> getDisplays() override;
    [[nodiscard]] std::string getWindowDisplayId() override;
    [[nodiscard]] bool hasUnlistedDisplays() override;
    void requestEveryScreen(std::function<void(SystemAnswer answer)> callback) override;
    [[nodiscard]] math::Rect getWindowFrame() override;

    [[nodiscard]] math::Insets getSafeAreaInsets() override;
    [[nodiscard]] Orientation getOrientation() override;
    void lockOrientation(Orientation value) override;
    void setStatusBar(Window::StatusBar value) override;
    [[nodiscard]] float getStatusBarHeight() override;
    void setSystemGestures(Window::SystemGestures value) override;
    [[nodiscard]] std::optional<Fold> getFold() override;
    void getCutouts(std::vector<math::Rect>& cutouts) override;

    [[nodiscard]] std::vector<std::string> getNativePlugins() override;
    void dispatch(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) override;
    void cancel(std::uint64_t id) override;
    void openScreen(const ScreenRequest& request) override;
    void cancelScreen(std::uint64_t id) override;
    void reportError(const core::Json& report) override;

    [[nodiscard]] std::string explainBlockedSocket(std::intptr_t socket) override;
    [[nodiscard]] std::string getNetworkRequirement() override;

    [[nodiscard]] bool isDeclared() override;
    void setEnabled(bool value) override;

    [[nodiscard]] bool isTreeWanted() override;
    void update(std::span<const AccessibilityElement> elements, std::uint64_t focused) override;
    void announce(std::string_view text, bool interrupt) override;

    [[nodiscard]] std::unique_ptr<VideoDecoder> openVideo(std::shared_ptr<io::PackageReader> file, std::string_view path) override;

    [[nodiscard]] std::optional<SystemFont> findSystemFont(char32_t codePoint) override;
    [[nodiscard]] std::vector<std::uint8_t> readSystemFont(const SystemFont& font) override;

  private:
    [[nodiscard]] static std::filesystem::path getContentStoreDirectory(std::string_view identifier);
};

} // namespace haylen::platform
