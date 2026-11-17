#pragma once

#include <signal.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string_view>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace haylen::platform {

// The fatal signals of POSIX systems, which reach `CrashGuard::reportCrash` and then the action that was there before. The thread that installs them, the one that plays the frames, gets a stack of its own for the handlers when it has none, so a stack overflow there is reported too, except on tvOS, whose SDK marks the alternate stacks of signals unavailable.
class PosixCrashHandlers final {
  public:
    static void install();
    static void writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept;

  private:
    static constexpr std::array<int, 6> kSignals{SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};

    static void handleSignal(int signal, siginfo_t* info, void* context);
    static void forwardSignal(int signal, siginfo_t* info, void* context);
    [[nodiscard]] static std::string_view describeSignal(int signal) noexcept;
    static void writeAll(int descriptor, std::string_view text) noexcept;

    static std::array<struct sigaction, kSignals.size()>& previousActions;

#if !(defined(__APPLE__) && TARGET_OS_TV)
    static constexpr std::size_t kAlternateStackSize = 262144;

    static void prepareAlternateStack();

    static std::array<std::byte, kAlternateStackSize>& alternateStack;
#endif
};

} // namespace haylen::platform
