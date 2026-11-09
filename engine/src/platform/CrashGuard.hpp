#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <string_view>

#if defined(_WIN32)
struct _EXCEPTION_POINTERS;
#elif !defined(__EMSCRIPTEN__)
#include <signal.h>
#endif

namespace haylen::platform {

// Captures the native crashes of the process into a small JSON report that the next start of the app reads: fatal signals on macOS, iOS, tvOS, Mac Catalyst, Linux and Android, and unhandled exceptions on Windows. The report holds the reason and the context that the frame thread published last, such as breadcrumbs and log lines, and the handler writes it from text prepared before the crash with calls that are safe while the process crashes. The crash then goes on to the handler that was there before, with the signal information and the context of the crash itself, or to the system, so the crash reporters of the system and of plugins still see the real crash. Browsers have no native crashes to catch, so the guard does nothing on the web.
class CrashGuard final {
  public:
    static constexpr std::size_t kContextSize = 32768;
    static constexpr std::size_t kPathSize = 4096;

    // Installs the handlers once per process. Every platform runtime calls it as the process starts, before the native parts of plugins load, so a crash reporter that a plugin installs finds the guard and hands each crash on to it.
    static void install();

    // Points the handlers at the report file of the app that plays, replacing the file of an earlier app of the process. A path longer than `kPathSize` turns the reports off.
    static void setReport(const std::filesystem::path& report);

    // Publishes the JSON object that a report carries as its context, or `{}` when it is larger than `kContextSize` bytes.
    static void setContext(std::string_view json);

    // Writes the report of a crash with the reason to the file of `setReport`, which is all the handler does before the crash goes on.
    static void writeReport(std::string_view reason) noexcept;

  private:
    struct Context {
        std::array<char, kContextSize> text{};
        std::size_t size = 0;
    };

    static void installHandlers();

#if !defined(__EMSCRIPTEN__)
    static void writeAll(int descriptor, std::string_view text) noexcept;
#endif

#if defined(_WIN32)
    using ExceptionFilter = long(__stdcall*)(_EXCEPTION_POINTERS* info);

    static long __stdcall handleException(_EXCEPTION_POINTERS* info);
    [[nodiscard]] static std::string_view describeException(unsigned long code) noexcept;

    static ExceptionFilter previousFilter;
#elif !defined(__EMSCRIPTEN__)
    static constexpr std::array<int, 6> kSignals{SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP};

    static void handleSignal(int signal, siginfo_t* info, void* context);
    static void forwardSignal(int signal, siginfo_t* info, void* context);
    [[nodiscard]] static std::string_view describeSignal(int signal) noexcept;

    static std::array<struct sigaction, kSignals.size()>& previousActions;
#endif

    static std::array<Context, 2>& contexts;
    static std::array<std::filesystem::path::value_type, kPathSize>& path;
    static std::atomic<int> published;
    static std::atomic<bool> installed;
    static std::atomic<bool> crashing;
};

} // namespace haylen::platform
