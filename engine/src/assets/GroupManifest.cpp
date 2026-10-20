#include "haylen/assets/GroupManifest.hpp"

#include <stdexcept>

#include "haylen/core/JsonValidator.hpp"

namespace haylen::assets {

std::map<std::string, std::vector<GroupManifest::Entry>> GroupManifest::read(const core::Json& manifest) {
    core::JsonValidator::requireKnownKeys(manifest, {"groups"}, "the asset group manifest");
    if (!manifest.contains("groups") || !manifest.at("groups").is_object()) {
        throw std::invalid_argument("The asset group manifest needs \"groups\", which maps each group name to its list of entries.");
    }

    std::map<std::string, std::vector<Entry>> groups;
    for (const auto& [name, items] : manifest.at("groups").items()) {
        if (!isList(items)) {
            throw std::invalid_argument("The asset group \"" + name + "\" must be a list of entries.");
        }
        std::vector<Entry>& entries = groups[name];
        for (const core::Json& item : items) {
            entries.push_back(readEntry(name, item));
        }
    }
    return groups;
}

// An empty Lua table arrives as an empty object, so it counts as an empty list.
bool GroupManifest::isList(const core::Json& value) noexcept {
    return value.is_array() || (value.is_object() && value.empty());
}

GroupManifest::Entry GroupManifest::readEntry(std::string_view group, const core::Json& item) {
    if (item.is_string()) {
        return {.path = item.get<std::string>()};
    }
    if (item.is_object()) {
        core::JsonValidator::requireKnownKeys(item, {"path", "type", "options"}, "an asset group entry");
    }
    if (!item.is_object() || !item.contains("path") || !item.at("path").is_string() || (item.contains("type") && !item.at("type").is_string()) || (item.contains("options") && !item.at("options").is_object())) {
        throw std::invalid_argument("The asset group \"" + std::string(group) + "\" has the entry " + item.dump(-1, ' ', false, core::Json::error_handler_t::replace) + ", which is not a path or an object with a \"path\" string, an optional \"type\" string and optional \"options\".");
    }
    return {.path = item.at("path").get<std::string>(), .type = item.value("type", std::string{}), .options = item.value("options", core::Json::object())};
}

} // namespace haylen::assets
