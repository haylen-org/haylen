#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace haylen::ui {

// The ids a collection source holds, so a change checks the ids it adds against the items in constant time per id instead of comparing every item. Empty ids belong to items that did not load yet and are never held.
class CollectionIds final {
  public:
    void assign(std::span<const std::string_view> values);

    // Throws `std::invalid_argument` for an added id that repeats among the added ones or that an item keeps, which is every item except those that `leaving` names, with the message of `CollectionDiff`.
    void check(std::span<const std::string_view> added, std::span<const std::string_view> leaving, std::string_view collection) const;

    void add(std::string_view id);
    void remove(std::string_view id);

  private:
    struct Hash {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
            return std::hash<std::string_view>{}(value);
        }
    };

    std::unordered_set<std::string, Hash, std::equal_to<>> ids;
};

} // namespace haylen::ui
