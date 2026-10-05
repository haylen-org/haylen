#include "haylen/ui/CollectionCell.hpp"

#include <stdexcept>

#include "ui/components/collections/CellTemplate.hpp"

namespace haylen::ui {

debug::ObjectCounter& CollectionCell::counter = *new debug::ObjectCounter("UiCell", debug::ObjectCounter::Kind::Native);

CollectionCell::CollectionCell(const CellTemplate& owner) : type(owner) {}

std::size_t CollectionCell::requirePart(std::string_view part) const {
    const std::optional<std::size_t> node = type.findPart(part);
    if (!node) {
        throw std::invalid_argument("The template of the type \"" + type.getName() + "\" has no part named \"" + std::string(part) + "\".");
    }
    return *node;
}

Component* CollectionCell::findPart(std::string_view part) const {
    const std::optional<std::size_t> node = type.findPart(part);
    return node ? nodes[*node] : nullptr;
}

void CollectionCell::set(std::string_view part, const core::Json& properties) {
    type.set(*this, requirePart(part), properties);
}

const std::shared_ptr<Transform>& CollectionCell::getTransform(std::string_view part) {
    return type.getTransform(*this, requirePart(part));
}

std::string_view CollectionCell::getType() const noexcept {
    return type.getName();
}

} // namespace haylen::ui
