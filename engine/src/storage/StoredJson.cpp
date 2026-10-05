#include "storage/StoredJson.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include "haylen/core/Utf8.hpp"

namespace haylen::storage {

void StoredJson::validate(const core::Json& value) {
    if (value.is_string() && !core::Utf8::isValid(value.get_ref<const std::string&>())) {
        throw std::invalid_argument("A string that is not valid UTF-8 cannot be converted to JSON.");
    }
    if (value.is_number_float() && !std::isfinite(value.get<double>())) {
        throw std::invalid_argument("A number that is not finite cannot be converted to JSON.");
    }
    if (value.is_object()) {
        for (const auto& [key, member] : value.items()) {
            validateKey(key);
            validate(member);
        }
    } else if (value.is_array()) {
        for (const core::Json& element : value) {
            validate(element);
        }
    }
}

void StoredJson::validateKey(std::string_view key) {
    if (!core::Utf8::isValid(key)) {
        throw std::invalid_argument("A key that is not valid UTF-8 cannot be converted to JSON.");
    }
}

} // namespace haylen::storage
