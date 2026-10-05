#pragma once

#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::ui {

// Compares two sequences of item ids, so a collection keeps the sizes, the selection and the focus of the items that stay, wherever they went. An empty id belongs to an item that did not load yet, which is never the same item as another one.
class CollectionDiff final {
  public:
    // The index before of an item that is new.
    static constexpr std::size_t kNew = std::numeric_limits<std::size_t>::max();

    struct Result {
        // For every item after, its index before or `kNew`.
        std::vector<std::size_t> previous;
        std::size_t inserted = 0;
        std::size_t removed = 0;
    };

    // Throws `std::invalid_argument` for an id that repeats after, named as an id of the items of `collection`. It costs linear time.
    [[nodiscard]] static Result compare(std::span<const std::string_view> before, std::span<const std::string_view> after, std::string_view collection);

    // Throws for the first id that repeats, the way `compare` does.
    static void checkUnique(std::span<const std::string_view> ids, std::string_view collection);

  private:
    static constexpr std::size_t kEmpty = std::numeric_limits<std::size_t>::max();

    // An open addressing table of the positions of ids, with room for twice as many ids as it holds, so lookups stay short.
    class Table final {
      public:
        explicit Table(std::span<const std::string_view> keys);

        // Returns the position of the id, or `kEmpty`.
        [[nodiscard]] std::size_t find(std::string_view id) const noexcept;

        // Adds the position of the key at that position and returns the position that already holds the same id, or `kEmpty`.
        std::size_t add(std::size_t position);

      private:
        std::span<const std::string_view> ids;
        std::vector<std::size_t> slots;
        std::size_t mask = 0;
    };

    [[noreturn]] static void failRepeated(std::string_view id, std::string_view collection);
};

} // namespace haylen::ui
