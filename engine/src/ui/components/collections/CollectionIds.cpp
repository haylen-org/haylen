#include "ui/components/collections/CollectionIds.hpp"

#include <algorithm>

#include "ui/components/collections/CollectionDiff.hpp"

namespace haylen::ui {

void CollectionIds::assign(std::span<const std::string_view> values) {
    ids.clear();
    ids.reserve(values.size());
    for (const std::string_view id : values) {
        add(id);
    }
}

void CollectionIds::check(std::span<const std::string_view> added, std::span<const std::string_view> leaving, std::string_view collection) const {
    CollectionDiff::checkUnique(added, collection);
    for (const std::string_view id : added) {
        if (!id.empty() && ids.contains(id) && std::ranges::find(leaving, id) == leaving.end()) {
            CollectionDiff::failRepeated(id, collection);
        }
    }
}

void CollectionIds::add(std::string_view id) {
    if (!id.empty()) {
        ids.emplace(id);
    }
}

void CollectionIds::remove(std::string_view id) {
    if (const auto found = ids.find(id); found != ids.end()) {
        ids.erase(found);
    }
}

} // namespace haylen::ui
