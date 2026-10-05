#include "ui/components/collections/CollectionDiff.hpp"

#include <algorithm>
#include <bit>
#include <functional>
#include <stdexcept>

namespace haylen::ui {

CollectionDiff::Table::Table(std::span<const std::string_view> keys) : ids(keys), slots(std::bit_ceil(keys.size() * 2 + 1), kEmpty), mask(slots.size() - 1) {}

std::size_t CollectionDiff::Table::find(std::string_view id) const noexcept {
    for (std::size_t slot = std::hash<std::string_view>{}(id)&mask;; slot = (slot + 1) & mask) {
        if (slots[slot] == kEmpty || ids[slots[slot]] == id) {
            return slots[slot];
        }
    }
}

std::size_t CollectionDiff::Table::add(std::size_t position) {
    const std::string_view id = ids[position];
    for (std::size_t slot = std::hash<std::string_view>{}(id)&mask;; slot = (slot + 1) & mask) {
        if (slots[slot] == kEmpty) {
            slots[slot] = position;
            return kEmpty;
        }
        if (ids[slots[slot]] == id) {
            return slots[slot];
        }
    }
}

void CollectionDiff::failRepeated(std::string_view id, std::string_view collection) {
    const std::string owner = collection.empty() ? "The items" : "The items of the collection \"" + std::string(collection) + "\"";
    throw std::invalid_argument(owner + " use the id \"" + std::string(id) + "\" more than once.");
}

void CollectionDiff::checkUnique(std::span<const std::string_view> ids, std::string_view collection) {
    Table table(ids);
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (!ids[index].empty() && table.add(index) != kEmpty) {
            failRepeated(ids[index], collection);
        }
    }
}

CollectionDiff::Result CollectionDiff::compare(std::span<const std::string_view> before, std::span<const std::string_view> after, std::string_view collection) {
    checkUnique(after, collection);
    Table known(before);
    for (std::size_t index = 0; index < before.size(); ++index) {
        if (!before[index].empty()) {
            (void)known.add(index);
        }
    }

    Result result;
    result.previous.resize(after.size());
    std::size_t kept = 0;
    for (std::size_t index = 0; index < after.size(); ++index) {
        const std::size_t found = after[index].empty() ? kEmpty : known.find(after[index]);
        result.previous[index] = found == kEmpty ? kNew : found;
        if (found == kEmpty) {
            ++result.inserted;
        } else {
            ++kept;
        }
    }
    result.removed = static_cast<std::size_t>(std::ranges::count_if(before, [](std::string_view id) { return !id.empty(); })) - kept;
    return result;
}

} // namespace haylen::ui
