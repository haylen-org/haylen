#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace haylen::ui {

// Places the items of a collection in lines along its axis, such as one item per line in a list or several cells per row in a grid, and gives each item its place across the axis. The collection keeps the lengths of the lines, so a layout only decides which items share a line and where each one sits in it.
class CollectionLayout {
  public:
    struct Input {
        std::size_t count = 0;
        float crossLength = 0.0F;
        float gap = 0.0F;

        // The lanes across the axis, such as the columns of a vertical grid.
        std::size_t lanes = 1;

        // The lanes each item spans, where zero spans the whole line. An empty list spans one lane with every item.
        std::span<const std::uint16_t> spans;
    };

    struct Slot {
        float crossStart = 0.0F;
        float crossLength = 0.0F;
    };

    virtual ~CollectionLayout() = default;

    // Places the items again from the first one that changed, which keeps the lines before it.
    virtual void arrange(const Input& input, std::size_t firstChanged) = 0;

    [[nodiscard]] virtual std::size_t getLineCount() const = 0;
    [[nodiscard]] virtual std::size_t getLine(std::size_t index) const = 0;

    // Returns the first item of a line, and the item count for the line after the last one.
    [[nodiscard]] virtual std::size_t getFirstIndex(std::size_t line) const = 0;
    [[nodiscard]] virtual Slot getSlot(std::size_t index) const = 0;
};

} // namespace haylen::ui
