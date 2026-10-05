#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"

namespace haylen::ui {

// The items a collection shows: how many there are, the id and the type of each one and the values its cells bind. A source reports every change through `changed` right after it makes it, and an item whose id is empty has not loaded yet, such as an item of a page that is still on its way.
class CollectionSource {
  public:
    struct Change {
        enum class Kind : std::uint8_t {
            Inserted,
            Removed,
            Moved,
            Changed,
            Replaced,
        };

        // The index before of an item that is new in a replacement.
        static constexpr std::size_t kNew = std::numeric_limits<std::size_t>::max();

        Kind kind = Kind::Replaced;

        // The first item inserted, removed, moved or changed, and how many.
        std::size_t index = 0;
        std::size_t count = 0;

        // The index a moved item ends at.
        std::size_t target = 0;

        // For a replacement, the index before of every item now, or `kNew`, valid while the change is reported.
        std::span<const std::size_t> previous;
    };

    CollectionSource() = default;
    virtual ~CollectionSource() = default;

    CollectionSource(const CollectionSource&) = delete;
    CollectionSource& operator=(const CollectionSource&) = delete;

    [[nodiscard]] virtual std::size_t getCount() const = 0;
    [[nodiscard]] virtual std::string_view getId(std::size_t index) const = 0;

    // The type of the item, or an empty name for the only type of its collection.
    [[nodiscard]] virtual std::string_view getType(std::size_t index) const = 0;

    // The value of a field of the item, or null when the item has no such field.
    [[nodiscard]] virtual core::Json getValue(std::size_t index, std::string_view field) const = 0;

    // Stores a value the player changed in a bound control, so the item shows it the next time it is bound.
    virtual void setValue(std::size_t index, std::string_view field, const core::Json& value) = 0;

    // Asks for the items of a range before they are bound, such as pages of a database. Sources that hold every item ignore it.
    virtual void request(std::size_t, std::size_t) {}

    core::Signal<const Change&> changed;
};

} // namespace haylen::ui
