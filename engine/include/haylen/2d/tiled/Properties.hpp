#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/tiled/Property.hpp"

namespace haylen::tiled {

// The custom properties of a map, layer, object, tileset or tile, in the order Tiled lists them.
class Properties final {
  public:
    Properties() = default;
    explicit Properties(std::vector<Property> values) : items(std::move(values)) {}

    [[nodiscard]] const std::vector<Property>& getItems() const noexcept {
        return items;
    }
    [[nodiscard]] const Property* find(std::string_view name) const noexcept;
    [[nodiscard]] bool has(std::string_view name) const noexcept {
        return find(name) != nullptr;
    }
    [[nodiscard]] bool getBool(std::string_view name, bool fallback) const;
    [[nodiscard]] double getNumber(std::string_view name, double fallback) const;
    [[nodiscard]] std::string getString(std::string_view name, std::string_view fallback) const;

    // Returns these properties with the ones from overrides replacing properties of the same name.
    [[nodiscard]] Properties merged(const Properties& overrides) const;

  private:
    std::vector<Property> items;
};

} // namespace haylen::tiled
