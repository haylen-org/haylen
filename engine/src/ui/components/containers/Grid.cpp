#include "ui/components/containers/Grid.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Grid::readProperties(PropertyReader& reader) {
    reader.read("columns", columns, 1, 64);
    if (reader.has("gap")) {
        float value = 0.0F;
        reader.read("gap", value, 0.0F, 10000.0F);
        gap = value;
    }
    reader.read("padding", padding);
}

math::Vec2 Grid::measureContent(Context& context, float availableWidth) {
    const Layout layout = measureRows(context, std::max(0.0F, availableWidth - padding.getHorizontal()));
    float height = 0.0F;
    for (const float row : layout.rows) {
        height += row;
    }
    height += layout.rows.empty() ? 0.0F : getGap(context) * static_cast<float>(layout.rows.size() - 1);
    return {layout.widest * static_cast<float>(std::min<std::size_t>(static_cast<std::size_t>(columns), getLayoutChildren().size())) + padding.getHorizontal(), height + padding.getVertical()};
}

void Grid::render(Context& context, const math::Rect& bounds) {
    const math::Rect inner = bounds.inset(padding);
    const Layout layout = measureRows(context, inner.width);
    const std::vector<Component*> visible = getLayoutChildren();
    float y = inner.y;
    for (std::size_t index = 0; index < visible.size(); ++index) {
        const std::size_t row = index / static_cast<std::size_t>(columns);
        const std::size_t column = index % static_cast<std::size_t>(columns);
        if (column == 0 && row > 0) {
            y += layout.rows[row - 1] + getGap(context);
        }
        Component& child = *visible[index];
        const math::Vec2 size = child.measure(context, layout.cell);
        const float x = inner.x + (layout.cell + getGap(context)) * static_cast<float>(column);
        const float width = child.getAlignment() == Alignment::Stretch ? layout.cell : std::min(size.x, layout.cell);
        child.draw(context, {std::floor(align(child.getAlignment(), x, layout.cell, width)), std::floor(y), width, layout.rows[row]});
    }
}

float Grid::getGap(Context& context) const {
    return gap.value_or(context.getMetric(Theme::Metric::ItemSpacing));
}

Grid::Layout Grid::measureRows(Context& context, float width) {
    Layout layout;
    const auto count = static_cast<float>(columns);
    layout.cell = std::max(0.0F, (width - getGap(context) * (count - 1.0F)) / count);
    const std::vector<Component*> visible = getLayoutChildren();
    for (std::size_t index = 0; index < visible.size(); ++index) {
        const math::Vec2 size = visible[index]->measure(context, layout.cell);
        const std::size_t row = index / static_cast<std::size_t>(columns);
        if (row >= layout.rows.size()) {
            layout.rows.push_back(0.0F);
        }
        layout.rows[row] = std::max(layout.rows[row], size.y);
        layout.widest = std::max(layout.widest, size.x);
    }
    return layout;
}

} // namespace haylen::ui
