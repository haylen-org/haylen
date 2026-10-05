#pragma once

#include <cstddef>

#include "haylen/ui/CollectionLayout.hpp"

namespace haylen::ui {

// One item per line across the whole cross length, so a line and an item are the same and arranging costs nothing.
class LinearLayout final : public CollectionLayout {
  public:
    void arrange(const Input& input, std::size_t firstChanged) override;

    [[nodiscard]] std::size_t getLineCount() const override {
        return count;
    }
    [[nodiscard]] std::size_t getLine(std::size_t index) const override {
        return index;
    }
    [[nodiscard]] std::size_t getFirstIndex(std::size_t line) const override {
        return line;
    }
    [[nodiscard]] Slot getSlot(std::size_t) const override {
        return {.crossStart = 0.0F, .crossLength = crossLength};
    }

  private:
    std::size_t count = 0;
    float crossLength = 0.0F;
};

} // namespace haylen::ui
