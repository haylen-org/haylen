#include "haylen/core/JsonValidator.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace haylen::core {

void JsonValidator::requireKnownKeys(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view context) {
    if (!object.is_object()) {
        throw std::invalid_argument(std::string(context) + " must be a JSON object.");
    }
    for (const auto& [key, value] : object.items()) {
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end()) {
            throw std::invalid_argument("Unknown key '" + key + "' in " + std::string(context) + ".");
        }
    }
}

} // namespace haylen::core
