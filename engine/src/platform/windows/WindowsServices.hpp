#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/port/Services.hpp"

namespace haylen::platform {

// The services of Windows: the platform, its displays, its windows among other windows, the links that start a new process of the app, the media controls of the system, UI Automation, the fonts of DirectWrite and the video decoder of Media Foundation. Windows has no handler registry in the language of the platform, so native libraries and C++ plugins answer the methods of apps, and it gates none of the permissions of the engine for a desktop app.
class WindowsServices final : public port::PlatformService, public port::DisplayService, public port::DesktopService, public port::LinkService, public port::MediaService, public port::AccessibilityService, public port::VideoService, public port::FontService {
  public:
    [[nodiscard]] static port::Services create();

    [[nodiscard]] std::string_view getName() const noexcept override;
    void initialize() override;
    void shutdown() noexcept override;
    [[nodiscard]] std::shared_ptr<io::Package> openBundledPackage() override;
    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override;
    void persistUserData() override;
    [[nodiscard]] SystemInfo getSystemInfo() override;
    [[nodiscard]] bool hasPointerDevice() const noexcept override;

    [[nodiscard]] std::vector<Display> getDisplays() override;
    [[nodiscard]] std::string getWindowDisplayId() override;
    [[nodiscard]] bool hasUnlistedDisplays() override;
    void requestEveryScreen(std::function<void(Permission::Answer answer)> callback) override;
    [[nodiscard]] math::Rect getWindowFrame() override;

    void setWindowStyle(const WindowStyle& value) override;
    void setWindowBackground(const math::Color& value) override;
    void setWindowFrame(const math::Rect& value) override;
    void setMousePassthrough(Window::Passthrough mode, std::span<const math::Polygon::Outline> regions) override;
    void startWindowDrag() override;
    void watchWindow() override;
    void updateWindow() override;
    [[nodiscard]] HaylenNativeWindow getNativeWindow() override;

    [[nodiscard]] bool forwardLink(std::string_view identifier, std::string_view url) override;
    void listenForLinks(std::string_view identifier) override;

    void setBackgroundAudio(bool value) override;
    void setNowPlaying(const std::optional<MediaSession::NowPlaying>& value) override;

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
