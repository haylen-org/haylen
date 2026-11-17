#include "platform/posix/PosixCrashHandlers.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#include "platform/CrashGuard.hpp"

namespace haylen::platform {

std::array<struct sigaction, PosixCrashHandlers::kSignals.size()>& PosixCrashHandlers::previousActions = *new std::array<struct sigaction, PosixCrashHandlers::kSignals.size()>{};

#if !(defined(__APPLE__) && TARGET_OS_TV)
// The stack stays untouched until a handler runs on it, so it takes no memory before.
std::array<std::byte, PosixCrashHandlers::kAlternateStackSize>& PosixCrashHandlers::alternateStack = *new std::array<std::byte, PosixCrashHandlers::kAlternateStackSize>;
#endif

void PosixCrashHandlers::install() {
#if !(defined(__APPLE__) && TARGET_OS_TV)
    prepareAlternateStack();
#endif

    for (std::size_t index = 0; index < kSignals.size(); ++index) {
        struct sigaction action{};
        action.sa_sigaction = &handleSignal;
        action.sa_flags = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        sigaction(kSignals[index], &action, &previousActions[index]);
    }
}

void PosixCrashHandlers::writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept {
    const int descriptor = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (descriptor < 0) {
        return;
    }

    for (const std::string_view piece : pieces) {
        writeAll(descriptor, piece);
    }

    close(descriptor);
}

void PosixCrashHandlers::writeAll(int descriptor, std::string_view text) noexcept {
    while (!text.empty()) {
        const ssize_t written = write(descriptor, text.data(), text.size());
        if (written <= 0) {
            return;
        }

        text.remove_prefix(static_cast<std::size_t>(written));
    }
}

#if !(defined(__APPLE__) && TARGET_OS_TV)
// A stack overflow leaves the thread no stack to run a handler on, and the handlers run on the alternate stack of the thread. A thread that has one already, such as every thread on Android, keeps it.
void PosixCrashHandlers::prepareAlternateStack() {
    stack_t current{};
    sigaltstack(nullptr, &current);
    if ((current.ss_flags & SS_DISABLE) == 0) {
        return;
    }

    stack_t stack{};
    stack.ss_sp = alternateStack.data();
    stack.ss_size = alternateStack.size();
    sigaltstack(&stack, nullptr);
}
#endif

void PosixCrashHandlers::handleSignal(int signal, siginfo_t* info, void* context) {
    CrashGuard::reportCrash(describeSignal(signal));
    forwardSignal(signal, info, context);
}

// The action that was there before takes the signal back, so a fault that happens again once the handler returns reaches it as it would without the guard. Its handler receives the signal information and the context of the crash itself, an ignored signal stays ignored, and the default action ends the process.
void PosixCrashHandlers::forwardSignal(int signal, siginfo_t* info, void* context) {
    const auto found = std::ranges::find(kSignals, signal);
    const struct sigaction& previous = previousActions[static_cast<std::size_t>(found - kSignals.begin())];
    sigaction(signal, &previous, nullptr);

    if (previous.sa_handler == SIG_IGN) {
        return;
    }

    if (previous.sa_handler == SIG_DFL) {
        raise(signal);
        return;
    }

    if ((previous.sa_flags & SA_SIGINFO) != 0) {
        previous.sa_sigaction(signal, info, context);
        return;
    }

    previous.sa_handler(signal);
}

std::string_view PosixCrashHandlers::describeSignal(int signal) noexcept {
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

} // namespace haylen::platform
