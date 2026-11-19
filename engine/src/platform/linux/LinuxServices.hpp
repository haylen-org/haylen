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

// The services of Linux desktops: the platform, its displays, its windows among other windows, the links that start a new process of the app, the screen reader of AT-SPI and the fonts of fontconfig. Linux has no handler registry in the language of the platform, so native libraries and C++ plugins answer the methods of apps, and it gates none of the permissions of the engine for an app outside a sandbox.
class LinuxServices final : public port::PlatformService, public port::DisplayService, public port::DesktopService, public port::LinkService, public port::AccessibilityService, public port::FontService {
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
    void requestEveryScreen(std::function<void(SystemAnswer answer)> callback) override;
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

    [[nodiscard]] bool isTreeWanted() override;
    void update(std::span<const AccessibilityElement> elements, std::uint64_t focused) override;
    void announce(std::string_view text, bool interrupt) override;

    [[nodiscard]] std::optional<SystemFont> findSystemFont(char32_t codePoint) override;
    [[nodiscard]] std::vector<std::uint8_t> readSystemFont(const SystemFont& font) override;

  private:
    [[nodiscard]] static std::filesystem::path getContentStoreDirectory(std::string_view identifier);
};

} // namespace haylen::platform
