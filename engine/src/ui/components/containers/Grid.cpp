#include "ui/components/containers/Grid.hpp"

#include <algorithm>
#include <cmath>

#include "haylen/ui/Context.hpp"

namespace haylen::ui {

void Grid::readProperties(PropertyReader& reader) {
    if (reader.has("columns")) {
        reader.read("columns", columns, 1, kMaxColumns);
        columnsSet = true;
    }
    reader.read("minColumnWidth", minColumnWidth, 0.0F, 10000.0F);
    if (reader.has("gap")) {
        float value = 0.0F;
        reader.read("gap", value, 0.0F, 10000.0F);
        gap = value;
    }
    if (reader.has("rowGap")) {
        float value = 0.0F;
        reader.read("rowGap", value, 0.0F, 10000.0F);
        rowGap = value;
    }
    reader.read("padding", padding);
    if (reader.has("alignItems")) {
        Alignment value = Alignment::Start;
        reader.readChoice<Alignment>("alignItems", value, kAlignments);
        alignItems = value;
    }
}

float Grid::getGap(Context& context) const {
    return gap.value_or(context.getMetric(Theme::Metric::ItemSpacing));
}

float Grid::getRowGap(Context& context) const {
    return rowGap.value_or(getGap(context));
}

Alignment Grid::getChildAlignment(const Component& child) const noexcept {
    return child.getCommon().align.value_or(alignItems.value_or(child.getAlignment()));
}

// A minimum column width fits as many columns as cells of that width, at most `columns` when it is set, and an unbounded width keeps the column count.
std::size_t Grid::countColumns(Context& context, float width) const {
    if (minColumnWidth <= 0.0F || width >= CommonProperties::kUnbounded) {
        return static_cast<std::size_t>(columns);
    }
    const float spacing = getGap(context);
    const auto fitting = static_cast<int>(std::floor((width + spacing) / (minColumnWidth + spacing)));
    return static_cast<std::size_t>(std::clamp(fitting, 1, columnsSet ? columns : kMaxColumns));
}

math::Vec2 Grid::measureContent(Context& context, float availableWidth) {
    const float inner = std::max(0.0F, availableWidth - padding.getHorizontal());
    const std::size_t count = countColumns(context, inner);
    (void)measureRows(context, inner);
    float height = 0.0F;
    for (const float row : rows) {
        height += row;
    }
    height += rows.empty() ? 0.0F : getRowGap(context) * static_cast<float>(rows.size() - 1);
    const auto across = static_cast<float>(std::min<std::size_t>(count, static_cast<std::size_t>(std::ranges::distance(getLayoutChildren()))));
    const float width = across > 0.0F ? widest * across + getGap(context) * (across - 1.0F) : 0.0F;
    return {width + padding.getHorizontal(), height + padding.getVertical()};
}

// Cells fill every row from its start, the right in a right-to-left UI, and every child sits in its cell by its alignment in both directions.
void Grid::render(Context& context, const math::Rect& bounds) {
    const math::Rect inner = bounds.inset(padding);
    const std::size_t count = countColumns(context, inner.width);
    const float cell = measureRows(context, inner.width);
    float y = inner.y;
    std::size_t index = 0;
    for (Component* child : getLayoutChildren()) {
        const std::size_t row = index / count;
        const std::size_t column = index % count;
        if (column == 0 && row > 0) {
            y += rows[row - 1] + getRowGap(context);
        }
        ++index;

        const float height = rows[row];
        const math::Rect area = context.mirror({inner.x + (cell + getGap(context)) * static_cast<float>(column), y, cell, height}, inner);
        const Alignment alignment = getChildAlignment(*child);
        if (alignment == Alignment::Stretch) {
            child->draw(context, {std::floor(area.x), std::floor(area.y), std::min(child->clampWidth(cell), cell), std::min(child->clampHeight(height), height)});
            continue;
        }
        const math::Vec2 size = math::Vec2::min(child->measure(context, cell), area.getSize());
        child->draw(context, {std::floor(context.alignHorizontally(alignment, area.x, cell, size.x)), std::floor(align(alignment, area.y, height, size.y)), size.x, size.y});
    }
}

// An unbounded width, such as the one of a horizontal scroll, stays unbounded for the cells, so every child measures at its natural width.
float Grid::measureRows(Context& context, float width) {
    const std::size_t count = countColumns(context, width);
    const auto across = static_cast<float>(count);
    const float cell = width < CommonProperties::kUnbounded ? std::max(0.0F, (width - getGap(context) * (across - 1.0F)) / across) : width;
    rows.clear();
    widest = 0.0F;
    std::size_t index = 0;
    for (Component* child : getLayoutChildren()) {
        const math::Vec2 size = child->measure(context, cell);
        const std::size_t row = index / count;
        ++index;
        if (row >= rows.size()) {
            rows.push_back(0.0F);
        }
        rows[row] = std::max(rows[row], size.y);
        widest = std::max(widest, size.x);
    }
    return cell;
}

} // namespace haylen::ui
