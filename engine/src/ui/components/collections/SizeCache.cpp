#include "ui/components/collections/SizeCache.hpp"

#include <algorithm>
#include <bit>
#include <cmath>

#include "haylen/ui/CollectionLayout.hpp"

namespace haylen::ui {

void SizeCache::setEstimates(std::span<const float> declared, float fallbackLength) {
    fallback = fallbackLength;
    estimates.assign(declared.size(), Estimate{});
    for (std::size_t type = 0; type < declared.size(); ++type) {
        estimates[type].declared = declared[type];
        estimates[type].learned = fallbackLength;
    }
    arranged = false;
}

void SizeCache::setFixedLength(std::uint16_t type, std::optional<float> value) {
    if (estimates[type].fixed != value) {
        estimates[type].fixed = value;
        arranged = false;
    }
}

void SizeCache::assign(std::span<const std::uint16_t> types) {
    items.assign(types.size(), Item{});
    for (std::size_t index = 0; index < types.size(); ++index) {
        items[index].type = types[index];
    }
    arranged = false;
}

void SizeCache::insert(std::size_t index, std::span<const std::uint16_t> types) {
    const auto position = items.begin() + static_cast<std::ptrdiff_t>(index);
    const auto inserted = items.insert(position, types.size(), Item{});
    for (std::size_t offset = 0; offset < types.size(); ++offset) {
        inserted[static_cast<std::ptrdiff_t>(offset)].type = types[offset];
    }
    arranged = false;
}

void SizeCache::erase(std::size_t index, std::size_t count) {
    const auto first = items.begin() + static_cast<std::ptrdiff_t>(index);
    items.erase(first, first + static_cast<std::ptrdiff_t>(count));
    arranged = false;
}

void SizeCache::move(std::size_t from, std::size_t to) {
    const auto source = items.begin() + static_cast<std::ptrdiff_t>(from);
    const auto target = items.begin() + static_cast<std::ptrdiff_t>(to);
    if (from < to) {
        std::rotate(source, source + 1, target + 1);
    } else {
        std::rotate(target, source, source + 1);
    }
    arranged = false;
}

void SizeCache::remap(std::span<const std::size_t> previous, std::span<const std::uint16_t> types) {
    std::vector<Item> next(previous.size());
    for (std::size_t index = 0; index < previous.size(); ++index) {
        if (previous[index] != kNew) {
            next[index] = items[previous[index]];
        }
        if (next[index].type != types[index]) {
            next[index] = Item{.length = 0.0F, .type = types[index], .state = State::Unmeasured};
        }
    }
    items = std::move(next);
    arranged = false;
}

void SizeCache::setType(std::size_t index, std::uint16_t type) {
    if (items[index].type != type) {
        items[index] = Item{.length = 0.0F, .type = type, .state = State::Unmeasured};
        arranged = false;
    }
}

// A measured length teaches the estimate of a type that declares none, and an estimate that moved far enough arranges the lines again, so the lengths of the items not measured yet follow it.
void SizeCache::learn(Item& item, float length) {
    if (item.type == kNoType) {
        return;
    }
    Estimate& estimate = estimates[item.type];
    if (estimate.declared > 0.0F || estimate.fixed) {
        return;
    }
    if (item.state == State::Unmeasured) {
        estimate.sum += length;
        ++estimate.count;
    } else {
        estimate.sum += length - item.length;
    }
    const auto mean = static_cast<float>(estimate.sum / static_cast<double>(estimate.count));
    if (std::fabs(mean - estimate.learned) >= kLearningStep) {
        estimate.learned = mean;
        arranged = false;
    }
}

bool SizeCache::measure(std::size_t index, float length) {
    Item& item = items[index];
    if (item.state == State::Measured && item.length == length) {
        return false;
    }
    learn(item, length);
    item.length = length;
    item.state = State::Measured;
    if (!arranged || layout == nullptr) {
        return false;
    }
    const std::size_t line = layout->getLine(index);
    const float measured = measureLine(line) + gap;
    if (measured == lines[line]) {
        return false;
    }
    add(line, static_cast<double>(measured) - static_cast<double>(lines[line]));
    lines[line] = measured;
    return true;
}

void SizeCache::markStale() noexcept {
    for (Item& item : items) {
        if (item.state == State::Measured) {
            item.state = State::Stale;
        }
    }
}

float SizeCache::getItemLength(std::size_t index) const noexcept {
    const Item& item = items[index];
    if (item.type != kNoType && estimates[item.type].fixed) {
        return *estimates[item.type].fixed;
    }
    return item.state == State::Unmeasured ? getEstimate(item.type) : item.length;
}

bool SizeCache::isMeasured(std::size_t index) const noexcept {
    return items[index].state == State::Measured || isFixed(index);
}

bool SizeCache::isFixed(std::size_t index) const noexcept {
    const std::uint16_t type = items[index].type;
    return type != kNoType && estimates[type].fixed.has_value();
}

float SizeCache::getEstimate(std::uint16_t type) const noexcept {
    if (type == kNoType) {
        return fallback;
    }
    const Estimate& estimate = estimates[type];
    if (estimate.fixed) {
        return *estimate.fixed;
    }
    return estimate.declared > 0.0F ? estimate.declared : estimate.learned;
}

float SizeCache::measureLine(std::size_t line) const noexcept {
    float longest = 0.0F;
    const std::size_t end = layout->getFirstIndex(line + 1);
    for (std::size_t index = layout->getFirstIndex(line); index < end; ++index) {
        longest = std::max(longest, getItemLength(index));
    }
    return longest;
}

void SizeCache::arrange(const CollectionLayout& value, float gapLength) {
    layout = &value;
    gap = gapLength;
    lines.resize(layout->getLineCount());
    for (std::size_t line = 0; line < lines.size(); ++line) {
        lines[line] = measureLine(line) + gap;
    }
    build();
    arranged = true;
}

// The tree holds at every position the sum of the lines its lowest set bit covers, built in one pass that adds each node to its parent.
void SizeCache::build() {
    tree.assign(lines.size() + 1, 0.0);
    for (std::size_t line = 0; line < lines.size(); ++line) {
        tree[line + 1] += lines[line];
        const std::size_t parent = (line + 1) + ((line + 1) & (~(line + 1) + 1));
        if (parent < tree.size()) {
            tree[parent] += tree[line + 1];
        }
    }
}

void SizeCache::add(std::size_t line, double delta) noexcept {
    for (std::size_t position = line + 1; position < tree.size(); position += position & (~position + 1)) {
        tree[position] += delta;
    }
}

double SizeCache::getLineOffset(std::size_t line) const noexcept {
    double sum = 0.0;
    for (std::size_t position = std::min(line, lines.size()); position > 0; position -= position & (~position + 1)) {
        sum += tree[position];
    }
    return sum;
}

float SizeCache::getLineLength(std::size_t line) const noexcept {
    return lines[line] - gap;
}

// The search descends the tree from its highest power of two, keeping the lines whose sum still fits in the offset.
std::size_t SizeCache::findLine(double offset) const noexcept {
    if (lines.empty() || offset <= 0.0) {
        return 0;
    }
    std::size_t position = 0;
    double remaining = offset;
    for (std::size_t step = std::bit_floor(lines.size()); step > 0; step >>= 1U) {
        if (position + step <= lines.size() && tree[position + step] <= remaining) {
            position += step;
            remaining -= tree[position];
        }
    }
    return std::min(position, lines.size() - 1);
}

double SizeCache::getTotal() const noexcept {
    return lines.empty() ? 0.0 : getLineOffset(lines.size()) - gap;
}

} // namespace haylen::ui
