#include "haylen/platform/SystemAnswer.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, SystemAnswer::State>, 4> SystemAnswer::kStateNames{{{"granted", State::Granted}, {"prompt", State::Prompt}, {"denied", State::Denied}, {"unsupported", State::Unsupported}}};

// A field of the wrong type reads as missing, and an answer that is no object or names no state fails as unsupported, so a broken answer of native code fails the call instead of the engine.
SystemAnswer SystemAnswer::fromJson(const core::Json& value) {
    SystemAnswer answer;
    if (!value.is_object()) {
        answer.message = "The platform answered with something that is not an answer.";
        return answer;
    }

    const std::optional<State> state = stateFromName(readText(value, "state"));
    answer.message = readText(value, "message");
    if (!state) {
        answer.message = "The platform answered with a state that is none of \"granted\", \"prompt\", \"denied\" and \"unsupported\".";
        return answer;
    }

    answer.state = *state;
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

core::Json SystemAnswer::missingToJson() const {
    core::Json list = core::Json::array();
    for (const Requirement& requirement : missing) {
        list.push_back({{"kind", requirement.kind}, {"name", requirement.name}, {"file", requirement.file}, {"snippet", requirement.snippet}});
    }

    return list;
}

std::string SystemAnswer::readText(const core::Json& object, const char* key) {
    const auto found = object.find(key);
    return found != object.end() && found->is_string() ? found->get<std::string>() : std::string();
}

std::string_view SystemAnswer::stateName(State value) noexcept {
    return std::ranges::find(kStateNames, value, &std::pair<std::string_view, State>::second)->first;
}

std::optional<SystemAnswer::State> SystemAnswer::stateFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStateNames, name, &std::pair<std::string_view, State>::first);
    return found != kStateNames.end() ? std::optional(found->second) : std::nullopt;
}

} // namespace haylen::platform
