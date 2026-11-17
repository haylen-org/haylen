#pragma once

#include <filesystem>
#include <span>
#include <string_view>

struct _EXCEPTION_POINTERS;

namespace haylen::platform {

// The unhandled exceptions of Windows, which reach `CrashGuard::reportCrash` and then the filter that was there before, or the system without one.
class WindowsCrashHandlers final {
  public:
    static void install();
    static void writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept;

  private:
    using ExceptionFilter = long(__stdcall*)(_EXCEPTION_POINTERS* info);

    static long __stdcall handleException(_EXCEPTION_POINTERS* info);
    [[nodiscard]] static std::string_view describeException(unsigned long code) noexcept;

    static ExceptionFilter previousFilter;
};

} // namespace haylen::platform
