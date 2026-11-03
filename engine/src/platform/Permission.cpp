#include "haylen/platform/Permission.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, Permission::Kind>, 1> Permission::kKindNames{{{"localNetwork", Kind::LocalNetwork}}};
const std::array<std::pair<std::string_view, Permission::State>, 4> Permission::kStateNames{{{"granted", State::Granted}, {"prompt", State::Prompt}, {"denied", State::Denied}, {"unknown", State::Unknown}}};

// A field of the wrong type reads as missing, so a broken answer of native code fails the request instead of the engine.
Permission::Answer Permission::Answer::fromJson(const core::Json& value) {
    Answer answer;
    if (!value.is_object()) {
        answer.message = "The platform answered the permission request with something that is not an answer.";
        return answer;
    }

    answer.state = stateFromName(readText(value, "state")).value_or(State::Unknown);
    answer.message = readText(value, "message");
    const auto missing = value.find("missing");
    if (missing != value.end() && missing->is_array()) {
        for (const core::Json& entry : *missing) {
            if (entry.is_object()) {
                answer.missing.push_back({.kind = readText(entry, "kind"), .name = readText(entry, "name"), .file = readText(entry, "file"), .snippet = readText(entry, "snippet")});
            }
        }
    }
    return answer;
}

core::Json Permission::Answer::missingToJson() const {
    core::Json list = core::Json::array();
    for (const Requirement& requirement : missing) {
        list.push_back({{"kind", requirement.kind}, {"name", requirement.name}, {"file", requirement.file}, {"snippet", requirement.snippet}});
    }
    return list;
}

std::string Permission::readText(const core::Json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
}

std::string_view Permission::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

std::optional<Permission::Kind> Permission::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Permission::stateName(State value) noexcept {
    return std::ranges::find(kStateNames, value, &std::pair<std::string_view, State>::second)->first;
}

std::optional<Permission::State> Permission::stateFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStateNames, name, &std::pair<std::string_view, State>::first);
    return found != kStateNames.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace haylen::platform
