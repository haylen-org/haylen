#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/port/Services.hpp"

namespace haylen::platform {

// The services of the test port, which answers like a console without plugins: a platform with one screen and an audio output, so the engine answers for every other capability. An app that `haylen_add_app` built for the port keeps its package in the folder "app" next to its executable.
class TestPortServices final : public port::PlatformService, public port::DisplayService {
  public:
    explicit TestPortServices(std::filesystem::path folder) noexcept : executableFolder(std::move(folder)) {}

    [[nodiscard]] static port::Services create(std::filesystem::path executableFolder, std::shared_ptr<port::AudioOutput> audio);

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
    void requestEveryScreen(std::function<void(platform::SystemAnswer answer)> callback) override;
    [[nodiscard]] math::Rect getWindowFrame() override;

  private:
    std::filesystem::path executableFolder;
};

} // namespace haylen::platform
