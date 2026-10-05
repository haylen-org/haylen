#include "ui/components/collections/GridLayout.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::ui {

std::size_t GridLayout::fitLanes(float crossLength, float minimum, float gap) noexcept {
    if (minimum <= 0.0F) {
        return 1;
    }
    const float fitting = std::floor((crossLength + gap) / (minimum + gap));
    return static_cast<std::size_t>(std::clamp(fitting, 1.0F, static_cast<float>(kMaxLanes)));
}

// The lines before the one that holds the first changed item stay as they were, unless the lanes changed.
void GridLayout::arrange(const Input& input, std::size_t firstChanged) {
    const std::size_t previousLanes = lanes;
    count = input.count;
    lanes = std::clamp<std::size_t>(input.lanes, 1, kMaxLanes);
    crossLength = input.crossLength;
    gap = input.gap;
    laneLength = std::max(0.0F, (crossLength - gap * static_cast<float>(lanes - 1)) / static_cast<float>(lanes));
    if (std::ranges::all_of(input.spans, [](std::uint16_t span) { return span == 1; })) {
        spans.clear();
        lineStarts.clear();
        return;
    }

    std::size_t kept = 0;
    std::size_t start = 0;
    if (lanes == previousLanes && !lineStarts.empty()) {
        const auto found = std::ranges::upper_bound(lineStarts, firstChanged);
        kept = found == lineStarts.begin() ? 0 : static_cast<std::size_t>(found - lineStarts.begin() - 1);
        start = lineStarts[kept];
    }
    spans.assign(input.spans.begin(), input.spans.end());
    lineStarts.resize(kept);

    std::size_t used = lanes;
    for (std::size_t index = start; index < count; ++index) {
        const std::size_t span = getSpan(index);
        if (used + span > lanes) {
            lineStarts.push_back(index);
            used = 0;
        }
        used += span;
    }
}

std::size_t GridLayout::getSpan(std::size_t index) const noexcept {
    const std::uint16_t span = spans[index];
    return span == 0 ? lanes : std::min<std::size_t>(span, lanes);
}

std::size_t GridLayout::getLineCount() const {
    return isUniform() ? (count + lanes - 1) / lanes : lineStarts.size();
}

std::size_t GridLayout::getLine(std::size_t index) const {
    if (isUniform()) {
        return index / lanes;
    }
    const auto found = std::ranges::upper_bound(lineStarts, index);
    return found == lineStarts.begin() ? 0 : static_cast<std::size_t>(found - lineStarts.begin() - 1);
}

std::size_t GridLayout::getFirstIndex(std::size_t line) const {
    if (isUniform()) {
        return std::min(line * lanes, count);
    }
    return line < lineStarts.size() ? lineStarts[line] : count;
}

CollectionLayout::Slot GridLayout::getSlot(std::size_t index) const {
    std::size_t position = index % lanes;
    std::size_t span = 1;
    if (!isUniform()) {
        position = 0;
        for (std::size_t before = lineStarts[getLine(index)]; before < index; ++before) {
            position += getSpan(before);
        }
        span = getSpan(index);
    }
    const auto start = static_cast<float>(position);
    const auto lanesSpanned = static_cast<float>(span);
    return {.crossStart = start * (laneLength + gap), .crossLength = lanesSpanned * laneLength + (lanesSpanned - 1.0F) * gap};
}

} // namespace haylen::ui
