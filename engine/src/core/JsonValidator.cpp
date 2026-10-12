#include "haylen/core/JsonValidator.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::core {

void JsonValidator::requireKnownKeys(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view context) {
    if (!object.is_object()) {
        throw std::invalid_argument("Expected " + std::string(context) + " to be a JSON object.");
    }
    for (const auto& [key, value] : object.items()) {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            throw std::invalid_argument("Unknown key \"" + key + "\" in " + std::string(context) + ".");
        }
    }
}

void JsonValidator::requireKnownOptions(const Json& options, std::initializer_list<std::string_view> allowed, std::string_view context) {
    if (!options.is_object()) {
        throw std::invalid_argument("Expected " + std::string(context) + " to be a JSON object.");
    }
    for (const auto& [key, value] : options.items()) {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            throw std::invalid_argument("Unknown option \"" + key + "\".");
        }
    }
}

} // namespace haylen::core
