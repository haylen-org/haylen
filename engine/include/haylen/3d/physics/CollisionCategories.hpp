#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::physics3d {

// The names of the collision categories of the app, which `physics3d.categories` of `haylen.json` lists in the order of their bits, so the first name is the bit 1, the second the bit 2 and so on, up to 32 names.
class CollisionCategories final {
  public:
    static constexpr std::string_view kKey = "categories";
    static constexpr std::size_t kMaxCategories = 32;

    // Reads `categories` of the `physics3d` section of `haylen.json`, when it holds one. Throws `std::invalid_argument` with a sentence that names the key as `haylen.json` writes it.
    void readSection(const core::Json& section);

    // Returns the bit of a category. Throws `std::invalid_argument` with the names the app has when none has the name.
    [[nodiscard]] std::uint32_t getBit(std::string_view name) const;

    [[nodiscard]] const std::vector<std::string>& getNames() const noexcept {
        return names;
    }

  private:
    std::vector<std::string> names;
};

} // namespace haylen::physics3d
