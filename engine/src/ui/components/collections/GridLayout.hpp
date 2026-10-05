#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "haylen/ui/CollectionLayout.hpp"

namespace haylen::ui {

// Fills the lanes across the axis and starts a new line when the next item does not fit, where an item that spans the whole line takes a line of its own. Without spans the line of an item is its index divided by the lanes, and with spans the layout keeps the first item of every line.
class GridLayout final : public CollectionLayout {
  public:
    static constexpr std::size_t kMaxLanes = 64;

    // Returns how many lanes at least `minimum` long fit across a cross length with a gap between them, which is at least one.
    [[nodiscard]] static std::size_t fitLanes(float crossLength, float minimum, float gap) noexcept;

    void arrange(const Input& input, std::size_t firstChanged) override;

    [[nodiscard]] std::size_t getLineCount() const override;
    [[nodiscard]] std::size_t getLine(std::size_t index) const override;
    [[nodiscard]] std::size_t getFirstIndex(std::size_t line) const override;
    [[nodiscard]] Slot getSlot(std::size_t index) const override;

  private:
    [[nodiscard]] bool isUniform() const noexcept {
        return spans.empty();
    }
    [[nodiscard]] std::size_t getSpan(std::size_t index) const noexcept;

    std::size_t count = 0;
    std::size_t lanes = 1;
    float crossLength = 0.0F;
    float gap = 0.0F;
    float laneLength = 0.0F;

    // The spans of the items and the first item of every line, kept only while some item spans more than one lane.
    std::vector<std::uint16_t> spans;
    std::vector<std::size_t> lineStarts;
};

} // namespace haylen::ui
