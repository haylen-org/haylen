#include "haylen/platform/DialogResult.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, DialogResult::Code>, 4> DialogResult::kCodeNames{{{"unsupported", Code::Unsupported}, {"cancelled", Code::Cancelled}, {"timeout", Code::Timeout}, {"failed", Code::Failed}}};

std::string_view DialogResult::codeName(Code value) noexcept {
    return std::ranges::find(kCodeNames, value, &std::pair<std::string_view, Code>::second)->first;
}

} // namespace haylen::platform
