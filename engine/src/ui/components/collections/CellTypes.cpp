#include "ui/components/collections/CellTypes.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/ui/CollectionCell.hpp"

namespace haylen::ui {

CellTypes::CellTypes(const ComponentRegistry& registry, const core::Json& definitions) {
    if (!definitions.is_object() || definitions.empty() || definitions.size() > kMaxTypes) {
        throw std::invalid_argument("The property \"types\" of a \"collection\" must map type names to type definitions.");
    }
    for (const auto& [name, definition] : definitions.items()) {
        templates.push_back(std::make_unique<CellTemplate>(registry, name, definition));
    }
    pools.resize(templates.size());
}

std::optional<std::uint16_t> CellTypes::find(std::string_view name) const noexcept {
    const auto found = std::ranges::find_if(templates, [name](const std::unique_ptr<CellTemplate>& type) { return type->getName() == name; });
    if (found == templates.end()) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(found - templates.begin());
}

std::shared_ptr<CollectionCell> CellTypes::acquire(std::uint16_t type) {
    std::vector<std::shared_ptr<CollectionCell>>& pool = pools[type];
    if (pool.empty()) {
        return templates[type]->build();
    }
    std::shared_ptr<CollectionCell> cell = std::move(pool.back());
    pool.pop_back();
    return cell;
}

void CellTypes::release(const std::shared_ptr<CollectionCell>& cell) {
    const auto type = static_cast<std::size_t>(std::ranges::find_if(templates, [&cell](const std::unique_ptr<CellTemplate>& owner) { return owner.get() == &cell->type; }) - templates.begin());
    pools[type].push_back(cell);
}

void CellTypes::trim(std::size_t limit) {
    for (std::vector<std::shared_ptr<CollectionCell>>& pool : pools) {
        if (pool.size() > limit) {
            pool.resize(limit);
        }
    }
}

} // namespace haylen::ui
