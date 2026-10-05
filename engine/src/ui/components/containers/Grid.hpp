#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Insets.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Places children in cells of equal width, a number of columns or as many as fit at a minimum width, with each row as tall as its tallest child.
class Grid final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "grid";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    static constexpr int kMaxColumns = 64;
    static constexpr std::array<std::pair<std::string_view, Alignment>, 4> kAlignments{{
        {"start", Alignment::Start},
        {"center", Alignment::Center},
        {"end", Alignment::End},
        {"stretch", Alignment::Stretch},
    }};

    [[nodiscard]] float getGap(Context& context) const;
    [[nodiscard]] float getRowGap(Context& context) const;
    [[nodiscard]] std::size_t countColumns(Context& context, float width) const;
    [[nodiscard]] Alignment getChildAlignment(const Component& child) const noexcept;

    // Measures the rows at a width into `rows` and returns the width of a cell.
    float measureRows(Context& context, float width);

    int columns = 2;
    bool columnsSet = false;
    float minColumnWidth = 0.0F;
    std::optional<float> gap;
    std::optional<float> rowGap;
    math::Insets padding;
    std::optional<Alignment> alignItems;

    // The heights of the rows of the last measure, kept so a frame reuses their storage.
    std::vector<float> rows;
    float widest = 0.0F;
};

} // namespace haylen::ui
