#pragma once

namespace haylen::core {

// Turns floats into the numbers of JSON documents. A float widened to double as it is prints every digit of its binary value, so this returns the double that prints with the shortest decimal form of the float instead: 0.6F becomes 0.6 and not 0.6000000238418579.
class JsonNumber final {
  public:
    [[nodiscard]] static double fromFloat(float value) noexcept;
};

} // namespace haylen::core
