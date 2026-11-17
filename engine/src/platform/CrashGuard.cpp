#include "platform/CrashGuard.hpp"

#include <algorithm>
#include <cstring>

#include "platform/CrashHandlers.hpp"

namespace haylen::platform {

std::array<CrashGuard::Context, 2>& CrashGuard::contexts = *new std::array<CrashGuard::Context, 2>;
std::array<std::filesystem::path::value_type, CrashGuard::kPathSize>& CrashGuard::path = *new std::array<std::filesystem::path::value_type, CrashGuard::kPathSize>{};
std::atomic<int> CrashGuard::published = 0;
std::atomic<bool> CrashGuard::installed = false;
std::atomic<bool> CrashGuard::crashing = false;

void CrashGuard::install() {
    if (!installed.exchange(true)) {
        CrashHandlers::install();
    }
}

void CrashGuard::setReport(const std::filesystem::path& report) {
    const std::filesystem::path::string_type& native = report.native();
    path.fill({});

    if (native.size() >= kPathSize) {
        return;
    }

    std::ranges::copy(native, path.begin());
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

void CrashGuard::writeReport(std::string_view reason) noexcept {
    if (path[0] == 0) {
        return;
    }

    const Context& context = contexts[static_cast<std::size_t>(published.load(std::memory_order_acquire))];
    const std::string_view text = context.size == 0 ? std::string_view("{}") : std::string_view(context.text.data(), context.size);
    const std::array<std::string_view, 5> pieces{R"({"reason":")", reason, R"(","context":)", text, "}\n"};
    CrashHandlers::writeFile(path.data(), pieces);
}

void CrashGuard::reportCrash(std::string_view reason) noexcept {
    if (!crashing.exchange(true)) {
        writeReport(reason);
    }
}

} // namespace haylen::platform
