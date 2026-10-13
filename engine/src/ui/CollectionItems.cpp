#include "haylen/ui/CollectionItems.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include "ui/components/collections/CollectionDiff.hpp"
#include "ui/components/collections/CollectionIds.hpp"

namespace haylen::ui {

void CollectionItems::checkIndex(std::size_t index, std::size_t limit) const {
    if (index >= limit) {
        throw std::out_of_range("The index " + std::to_string(index) + " is outside the items, which hold " + std::to_string(items.size()) + ".");
    }
}

CollectionItems::CollectionItems() : known(std::make_unique<CollectionIds>()) {}

CollectionItems::~CollectionItems() = default;

// Items that did not load yet have no id, so only the ids that exist must be unique.
void CollectionItems::checkIds(const std::vector<Item>& added, std::size_t first, std::size_t last) const {
    std::vector<std::string_view> addedIds;
    addedIds.reserve(added.size());
    for (const Item& item : added) {
        addedIds.emplace_back(item.id);
    }
    std::vector<std::string_view> leaving;
    leaving.reserve(last - first);
    for (std::size_t index = first; index < last; ++index) {
        leaving.emplace_back(items[index].id);
    }
    known->check(addedIds, leaving, {});
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
    known->assign(after);
    items = std::move(value);
    changed.emit({.kind = Change::Kind::Replaced, .index = 0, .count = items.size(), .target = 0, .previous = result.previous});
}

void CollectionItems::insert(std::size_t index, std::vector<Item> added) {
    checkIndex(index, items.size() + 1);
    checkIds(added, index, index);
    const std::size_t count = added.size();
    for (const Item& item : added) {
        known->add(item.id);
    }
    items.insert(items.begin() + static_cast<std::ptrdiff_t>(index), std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
    changed.emit({.kind = Change::Kind::Inserted, .index = index, .count = count});
}

void CollectionItems::remove(std::size_t index, std::size_t count) {
    checkIndex(index, items.size());
    const std::size_t removed = std::min(count, items.size() - index);
    for (std::size_t offset = 0; offset < removed; ++offset) {
        known->remove(items[index + offset].id);
    }
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

// An item that keeps its id changes in place, and one with another id comes in while the item it replaces leaves.
void CollectionItems::replace(std::size_t index, Item value) {
    checkIndex(index, items.size());
    const std::vector<Item> added{value};
    checkIds(added, index, index + 1);
    const bool same = items[index].id == value.id;
    known->remove(items[index].id);
    known->add(value.id);
    items[index] = std::move(value);
    if (same) {
        changed.emit({.kind = Change::Kind::Changed, .index = index, .count = 1});
        return;
    }
    changed.emit({.kind = Change::Kind::Removed, .index = index, .count = 1});
    changed.emit({.kind = Change::Kind::Inserted, .index = index, .count = 1});
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
