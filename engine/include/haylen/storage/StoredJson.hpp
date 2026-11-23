#pragma once

#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::storage {

// Checks values before user storage keeps them as JSON text, which holds only UTF-8 text and finite numbers, so a value that could never be written fails when it is given instead of breaking every later save.
class StoredJson final {
  public:
    // Throws `std::invalid_argument` for the first string or key that is not valid UTF-8 and the first number that is not finite.
    static void validate(const core::Json& value);

    // Throws `std::invalid_argument` when a key is not valid UTF-8.
    static void validateKey(std::string_view key);
};

} // namespace haylen::storage
