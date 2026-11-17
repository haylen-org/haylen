#include "platform/CrashHandlers.hpp"

#include "platform/windows/WindowsCrashHandlers.hpp"

namespace haylen::platform {

void CrashHandlers::install() {
    WindowsCrashHandlers::install();
}

void CrashHandlers::writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept {
    WindowsCrashHandlers::writeFile(file, pieces);
}

} // namespace haylen::platform
