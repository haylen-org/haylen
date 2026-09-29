#pragma once

#include <initializer_list>
#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::core {

// Checks the shape of the JSON documents and option objects that configure the engine.
class JsonValidator final {
  public:
    // Throws std::invalid_argument naming the first key of the object that is not allowed, so a misspelled setting never goes unnoticed.
    static void requireKnownKeys(const Json& object, std::initializer_list<std::string_view> allowed, std::string_view context);
};

} // namespace haylen::core
