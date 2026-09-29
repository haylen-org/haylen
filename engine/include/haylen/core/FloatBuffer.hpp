#pragma once

#include <cstddef>
#include <span>
#include <vector>

namespace haylen::core {

// A fixed number of floats that Lua and C++ share without copying. Scripts fill it through index access and bulk sets, and bulk APIs such as sprite batches, drawBatch and physics read and write it in place, so thousands of sprites or bodies change without a Lua table for each one.
class FloatBuffer final {
  public:
    explicit FloatBuffer(std::size_t count, float value = 0.0F);

    [[nodiscard]] std::size_t size() const noexcept {
        return values.size();
    }

    // Both throw std::out_of_range for an index past the end.
    [[nodiscard]] float get(std::size_t index) const;
    void set(std::size_t index, float value);

    // Copies the values in from the index on. Throws std::out_of_range when they do not fit.
    void set(std::size_t first, std::span<const float> source);
    void fill(float value) noexcept;

    [[nodiscard]] std::span<float> getValues() noexcept {
        return values;
    }
    [[nodiscard]] std::span<const float> getValues() const noexcept {
        return values;
    }

  private:
    std::vector<float> values;
};

} // namespace haylen::core
