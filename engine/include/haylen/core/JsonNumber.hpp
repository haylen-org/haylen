#pragma once

#include <string_view>

#include "haylen/core/Json.hpp"

namespace haylen::core {

// Turns floats into the numbers of JSON documents and back. A float widened to double as it is prints every digit of its binary value, so this returns the double that prints with the shortest decimal form of the float instead: 0.6F becomes 0.6 and not 0.6000000238418579.
class JsonNumber final {
  public:
    [[nodiscard]] static double fromFloat(float value) noexcept;

    // Reads a number of a JSON document as a float. A number past the range of floats throws `std::invalid_argument`, since converting it is undefined, and a value that is not a number throws the type error of the JSON library.
    [[nodiscard]] static float toFloat(const Json& value);

    // Reads the number of a key of an object as a float, or returns the fallback when the object lacks the key.
    [[nodiscard]] static float toFloat(const Json& object, std::string_view key, float fallback);
};

} // namespace haylen::core
