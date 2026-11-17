#include "platform/CrashHandlers.hpp"

#include "platform/posix/PosixCrashHandlers.hpp"

namespace haylen::platform {

void CrashHandlers::install() {
    PosixCrashHandlers::install();
}

void CrashHandlers::writeFile(const std::filesystem::path::value_type* file, std::span<const std::string_view> pieces) noexcept {
    PosixCrashHandlers::writeFile(file, pieces);
}

} // namespace haylen::platform
