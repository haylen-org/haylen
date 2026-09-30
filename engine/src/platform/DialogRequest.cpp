#include "haylen/platform/DialogRequest.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, DialogRequest::MessageKind>, 3> DialogRequest::kMessageKindNames{{{"info", MessageKind::Info}, {"warning", MessageKind::Warning}, {"error", MessageKind::Error}}};

std::string_view DialogRequest::messageKindName(MessageKind value) noexcept {
    return std::ranges::find(kMessageKindNames, value, &std::pair<std::string_view, MessageKind>::second)->first;
}

std::optional<DialogRequest::MessageKind> DialogRequest::messageKindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kMessageKindNames, name, &std::pair<std::string_view, MessageKind>::first);
    return found != kMessageKindNames.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace haylen::platform
