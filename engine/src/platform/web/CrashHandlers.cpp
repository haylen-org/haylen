#include "platform/CrashHandlers.hpp"

namespace haylen::platform {

// A page runs in the browser, which ends a failing module itself and catches no native crash for it, so there is no handler to install.
void CrashHandlers::install() {}

void CrashHandlers::writeFile(const std::filesystem::path::value_type*, std::span<const std::string_view>) noexcept {}

} // namespace haylen::platform
