#pragma once

namespace haylen::lua::math {

// Writes the numbers of math values for the text that `tostring` gives them.
class NumberText final {
  public:
    // Returns the number as a double without the sign of a negative zero, so a component that rounds to zero never prints as "-0.0".
    [[nodiscard]] static constexpr double plain(float value) noexcept {
        return static_cast<double>(value) + 0.0;
    }
};

} // namespace haylen::lua::math
