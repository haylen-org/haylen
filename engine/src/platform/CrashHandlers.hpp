#pragma once

#include <filesystem>
#include <span>
#include <string_view>

namespace haylen::platform {

// The handlers of native crashes of the system of the build, which hand every crash to `CrashGuard::reportCrash` and then on to the handler that was there before. The build compiles the handlers of its system: the fatal signals of `PosixCrashHandlers`, the unhandled exceptions of `WindowsCrashHandlers` and none on the web.
class CrashHandlers final {
  public:
    static void install();

    // Writes the pieces of a report into the file at the path, replacing what it held, with calls that are safe while the process crashes.
    static void writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept;
};

} // namespace haylen::platform
