#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "haylen/platform/SystemInfo.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::port {

// What every platform tells the engine: its name, the package that ships with the app, the folders of the data of each app and what the device is. Every port registers it, and its methods may run on any thread.
class PlatformService {
  public:
    PlatformService() = default;
    PlatformService(const PlatformService&) = delete;
    PlatformService& operator=(const PlatformService&) = delete;
    virtual ~PlatformService() = default;

    // The name of the platform, which `haylen.platform` reports, such as the `PLATFORM` of a port.
    [[nodiscard]] virtual std::string_view getName() const noexcept = 0;

    // Starts following the system as the first player of the process starts, such as its theme, its battery and its languages, which it reports through `port::SystemReports`, and stops as the player ends.
    virtual void initialize() = 0;
    virtual void shutdown() noexcept = 0;

    // Opens the package that ships with the app, which plays when the command line names none, with the local content store of a protected release apart from the user data and in a place the system does not back up. Throws `std::runtime_error` with a sentence that says where it looked.
    [[nodiscard]] virtual std::shared_ptr<io::Package> openBundledPackage() = 0;

    // The folder of the user data of an app.
    [[nodiscard]] virtual std::filesystem::path getUserDataDirectory(std::string_view identifier) = 0;

    // Commits the user data where the platform saves only on request, and does nothing where every write is saved.
    virtual void persistUserData() = 0;

    // The sentence that tells why the apps cannot use their user data now, such as a web page whose user data another tab of the site holds, or an empty text while they can. The storage of an app that starts then refuses every call with it.
    [[nodiscard]] virtual std::string getUserDataRefusal() = 0;

    // What the device and its operating system are, read once per process. The graphics device names the GPU, so `gpuName` stays empty.
    [[nodiscard]] virtual platform::SystemInfo getSystemInfo() = 0;

    // Whether the device has a mouse or a touch screen, which TVs and consoles lack.
    [[nodiscard]] virtual bool hasPointerDevice() const noexcept = 0;
};

} // namespace haylen::port
