#include "haylen/platform/Permission.hpp"

#include <algorithm>
#include <cstddef>
#include <format>

namespace haylen::platform {

const std::array<std::pair<std::string_view, Permission::Kind>, 3> Permission::kKindNames{{{"motion", Kind::Motion}, {"screens", Kind::Screens}, {"localNetwork", Kind::LocalNetwork}}};
const std::array<std::string_view, 3> Permission::kKindDescriptions{"the motion sensors", "every screen", "the local network"};
const std::array<std::pair<std::string_view, Permission::State>, 5> Permission::kStateNames{{{"granted", State::Granted}, {"prompt", State::Prompt}, {"denied", State::Denied}, {"unknown", State::Unknown}, {"unsupported", State::Unsupported}}};

// A field of the wrong type reads as missing, and an answer that is no object fails as unsupported, so a broken answer of native code fails the request instead of the engine.
Permission::Answer Permission::Answer::fromJson(const core::Json& value) {
    Answer answer;
    if (!value.is_object()) {
        answer.state = State::Unsupported;
        answer.message = "The platform answered with something that is not an answer.";
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

std::string_view Permission::describe(Kind value) noexcept {
    return kKindDescriptions[static_cast<std::size_t>(value)];
}

std::string Permission::refusal(Kind value) {
    return refusal(describe(value), kindName(value));
}

std::string Permission::refusal(std::string_view capability, std::string_view name) {
    return std::format("The person has not allowed {}. Ask for it with \"system.requestPermission('{}')\", which shows the prompt of the system while the system still asks, or the person allows it in the settings of the system.", capability, name);
}

std::string Permission::unknown(std::string_view name) {
    std::string kinds;
    for (std::size_t index = 0; index < kKindNames.size(); ++index) {
        const std::string_view separator = index == 0 ? "" : index + 1 == kKindNames.size() ? " and " : ", ";
        kinds += std::format("{}\"{}\"", separator, kKindNames[index].first);
    }
    return std::format("The permission \"{}\" is neither one of the engine, which are {}, nor one that a plugin of the app declares. A plugin declares the permissions it owns in the \"permissions\" of its \"plugin.json\".", name, kinds);
}

bool Permission::isPluginName(std::string_view name) noexcept {
    if (name.empty() || name.front() < 'a' || name.front() > 'z' || kindFromName(name)) {
        return false;
    }
    // clang-format off
    return std::ranges::all_of(name, [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9');
    });
    // clang-format on
}

} // namespace haylen::platform
