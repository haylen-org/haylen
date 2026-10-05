#include "haylen/ui/CollectionItems.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "ui/components/collections/CollectionDiff.hpp"

namespace haylen::ui {

void CollectionItems::checkIndex(std::size_t index, std::size_t limit) const {
    if (index >= limit) {
        throw std::out_of_range("The index " + std::to_string(index) + " is outside the items, which hold " + std::to_string(items.size()) + ".");
    }
}

// Items that did not load yet have no id, so only the ids that exist must be unique.
void CollectionItems::checkIds(const std::vector<Item>& added, std::size_t first, std::size_t last) const {
    std::vector<std::string_view> ids;
    ids.reserve(items.size() - (last - first) + added.size());
    // clang-format off
    const auto collect = [&ids](const Item& item) {
        if (!item.id.empty()) {
            ids.emplace_back(item.id);
        }
    };
    // clang-format on
    std::for_each(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(first), collect);
    std::ranges::for_each(added, collect);
    std::for_each(items.begin() + static_cast<std::ptrdiff_t>(last), items.end(), collect);
    CollectionDiff::checkUnique(ids, {});
}

void CollectionItems::assign(std::vector<Item> value) {
    std::vector<std::string_view> before;
    before.reserve(items.size());
    for (const Item& item : items) {
        before.emplace_back(item.id);
    }
    std::vector<std::string_view> after;
    after.reserve(value.size());
    for (const Item& item : value) {
        after.emplace_back(item.id);
    }
    const CollectionDiff::Result result = CollectionDiff::compare(before, after, {});
    items = std::move(value);
    changed.emit({.kind = Change::Kind::Replaced, .index = 0, .count = items.size(), .target = 0, .previous = result.previous});
}

void CollectionItems::insert(std::size_t index, std::vector<Item> added) {
    checkIndex(index, items.size() + 1);
    checkIds(added, index, index);
    const std::size_t count = added.size();
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
    changed.emit({.kind = Change::Kind::Inserted, .index = index, .count = count});
}

void CollectionItems::remove(std::size_t index, std::size_t count) {
    checkIndex(index, items.size());
    const std::size_t removed = std::min(count, items.size() - index);
    const auto first = items.begin() + static_cast<std::ptrdiff_t>(index);
    items.erase(first, first + static_cast<std::ptrdiff_t>(removed));
    changed.emit({.kind = Change::Kind::Removed, .index = index, .count = removed});
}

void CollectionItems::move(std::size_t from, std::size_t to) {
    checkIndex(from, items.size());
    checkIndex(to, items.size());
    const auto source = items.begin() + static_cast<std::ptrdiff_t>(from);
    const auto target = items.begin() + static_cast<std::ptrdiff_t>(to);
    if (from < to) {
        std::rotate(source, source + 1, target + 1);
    } else {
        std::rotate(target, source, source + 1);
    }
    changed.emit({.kind = Change::Kind::Moved, .index = from, .count = 1, .target = to});
}

// An item that keeps its id changes in place, and one with another id takes the place of the item it replaces.
void CollectionItems::replace(std::size_t index, Item value) {
    checkIndex(index, items.size());
    const std::vector<Item> added{value};
    checkIds(added, index, index + 1);
    const bool same = items[index].id == value.id;
    items[index] = std::move(value);
    if (same) {
        changed.emit({.kind = Change::Kind::Changed, .index = index, .count = 1});
        return;
    }
    std::vector<std::size_t> previous(items.size());
    for (std::size_t position = 0; position < previous.size(); ++position) {
        previous[position] = position == index ? Change::kNew : position;
    }
    changed.emit({.kind = Change::Kind::Replaced, .index = 0, .count = items.size(), .target = 0, .previous = previous});
}

void CollectionItems::update(std::size_t index, std::string_view field, core::Json value) {
    checkIndex(index, items.size());
    items[index].fields[std::string(field)] = std::move(value);
    changed.emit({.kind = Change::Kind::Changed, .index = index, .count = 1});
}

const CollectionItems::Item& CollectionItems::getItem(std::size_t index) const {
    checkIndex(index, items.size());
    return items[index];
}

std::string_view CollectionItems::getId(std::size_t index) const {
    return items[index].id;
}

std::string_view CollectionItems::getType(std::size_t index) const {
    return items[index].type;
}

core::Json CollectionItems::getValue(std::size_t index, std::string_view field) const {
    const core::Json& fields = items[index].fields;
    const auto found = fields.find(field);
    return found != fields.end() ? *found : core::Json();
}

void CollectionItems::setValue(std::size_t index, std::string_view field, const core::Json& value) {
    items[index].fields[std::string(field)] = value;
}

} // namespace haylen::ui
