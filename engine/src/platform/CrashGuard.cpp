#include "platform/CrashGuard.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#elif !defined(__EMSCRIPTEN__)
#include <fcntl.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cstring>

namespace haylen::platform {

std::array<CrashGuard::Context, 2>& CrashGuard::contexts = *new std::array<CrashGuard::Context, 2>;
std::array<std::filesystem::path::value_type, CrashGuard::kPathSize>& CrashGuard::path = *new std::array<std::filesystem::path::value_type, CrashGuard::kPathSize>{};
std::atomic<int> CrashGuard::published = 0;
std::atomic<bool> CrashGuard::installed = false;
std::atomic<bool> CrashGuard::crashing = false;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
std::array<struct sigaction, CrashGuard::kSignals.size()>& CrashGuard::previousActions = *new std::array<struct sigaction, CrashGuard::kSignals.size()>{};
#endif

void CrashGuard::install(const std::filesystem::path& report) {
    const std::filesystem::path::string_type& native = report.native();
    path.fill({});
    if (native.size() >= kPathSize) {
        return;
    }
    std::ranges::copy(native, path.begin());
    if (!installed.exchange(true)) {
        installHandlers();
    }
}

// Only the frame thread publishes, so it writes the context that the handler does not read and then switches the two.
void CrashGuard::setContext(std::string_view json) {
    const int next = 1 - published.load(std::memory_order_acquire);
    Context& context = contexts[static_cast<std::size_t>(next)];
    const std::string_view kept = json.size() <= kContextSize ? json : std::string_view("{}");
    std::memcpy(context.text.data(), kept.data(), kept.size());
    context.size = kept.size();
    published.store(next, std::memory_order_release);
}

#if !defined(__EMSCRIPTEN__)

void CrashGuard::writeReport(std::string_view reason) noexcept {
    if (path[0] == 0) {
        return;
    }
#if defined(_WIN32)
    int descriptor = -1;
    if (_wsopen_s(&descriptor, path.data(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0) {
        return;
    }
#else
    const int descriptor = open(path.data(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0) {
        return;
    }
#endif

    const Context& context = contexts[static_cast<std::size_t>(published.load(std::memory_order_acquire))];
    writeAll(descriptor, R"({"reason":")");
    writeAll(descriptor, reason);
    writeAll(descriptor, R"(","context":)");
    writeAll(descriptor, context.size == 0 ? std::string_view("{}") : std::string_view(context.text.data(), context.size));
    writeAll(descriptor, "}\n");
#if defined(_WIN32)
    _close(descriptor);
#else
    close(descriptor);
#endif
}

void CrashGuard::writeAll(int descriptor, std::string_view text) noexcept {
    while (!text.empty()) {
#if defined(_WIN32)
        const int written = _write(descriptor, text.data(), static_cast<unsigned int>(text.size()));
#else
        const ssize_t written = write(descriptor, text.data(), text.size());
#endif
        if (written <= 0) {
            return;
        }
        text.remove_prefix(static_cast<std::size_t>(written));
    }
}

#endif

#if defined(_WIN32)

void CrashGuard::installHandlers() {
    SetUnhandledExceptionFilter(&handleException);
}

long __stdcall CrashGuard::handleException(_EXCEPTION_POINTERS* info) {
    if (!crashing.exchange(true)) {
        writeReport(describeException(info->ExceptionRecord->ExceptionCode));
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

std::string_view CrashGuard::describeException(unsigned long code) noexcept {
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "accessViolation";
    case EXCEPTION_STACK_OVERFLOW:
        return "stackOverflow";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "illegalInstruction";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "divideByZero";
    case EXCEPTION_BREAKPOINT:
        return "breakpoint";
    default:
        return "exception";
    }
}

#elif !defined(__EMSCRIPTEN__)

void CrashGuard::installHandlers() {
    for (std::size_t index = 0; index < kSignals.size(); ++index) {
        struct sigaction action{};
        action.sa_sigaction = &handleSignal;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        sigaction(kSignals[index], &action, &previousActions[index]);
    }
}

// The handler that was there before takes the signal again once the report is written, and the default one ends the process.
void CrashGuard::handleSignal(int signal, siginfo_t*, void*) {
    if (!crashing.exchange(true)) {
        writeReport(describeSignal(signal));
    }
    const auto found = std::ranges::find(kSignals, signal);
    sigaction(signal, &previousActions[static_cast<std::size_t>(found - kSignals.begin())], nullptr);
    raise(signal);
}

std::string_view CrashGuard::describeSignal(int signal) noexcept {
    switch (signal) {
    case SIGSEGV:
        return "segmentationFault";
    case SIGBUS:
        return "busError";
    case SIGILL:
        return "illegalInstruction";
    case SIGFPE:
        return "arithmeticError";
    case SIGABRT:
        return "abort";
    case SIGTRAP:
        return "trap";
    default:
        return "signal";
    }
}

#else

void CrashGuard::installHandlers() {}

void CrashGuard::writeReport(std::string_view) noexcept {}

#endif

} // namespace haylen::platform
