#include "platform/windows/WindowsCrashHandlers.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>

#include "platform/CrashGuard.hpp"

namespace haylen::platform {

WindowsCrashHandlers::ExceptionFilter WindowsCrashHandlers::previousFilter = nullptr;

void WindowsCrashHandlers::install() {
    previousFilter = SetUnhandledExceptionFilter(&handleException);
}

void WindowsCrashHandlers::writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept {
    int descriptor = -1;
    if (_wsopen_s(&descriptor, file, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0) {
        return;
    }

    for (std::string_view piece : pieces) {
        while (!piece.empty()) {
            const int written = _write(descriptor, piece.data(), static_cast<unsigned int>(piece.size()));
            if (written <= 0) {
                break;
            }

            piece.remove_prefix(static_cast<std::size_t>(written));
        }
    }

    _close(descriptor);
}

// The filter that was there before decides what happens to the exception, and without one the system handles it.
long __stdcall WindowsCrashHandlers::handleException(_EXCEPTION_POINTERS* info) {
    CrashGuard::reportCrash(describeException(info->ExceptionRecord->ExceptionCode));

    if (previousFilter != nullptr) {
        return previousFilter(info);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

std::string_view WindowsCrashHandlers::describeException(unsigned long code) noexcept {
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

} // namespace haylen::platform
