#include "haylen/2d/graphics/SpriteLayout.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace haylen::graphics2d {

SpriteLayout::SpriteLayout(std::vector<Field> layoutFields, const SpriteInstance& sprite) : fields(std::move(layoutFields)), base(sprite) {
    if (fields.empty()) {
        throw std::invalid_argument("A sprite layout needs at least one field.");
    }
}

std::optional<SpriteLayout::Field> SpriteLayout::fieldFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFieldNames, name, &std::pair<std::string_view, Field>::first);
    return found != kFieldNames.end() ? std::optional<Field>(found->second) : std::nullopt;
}

std::string_view SpriteLayout::fieldName(Field value) noexcept {
    const auto found = std::ranges::find(kFieldNames, value, &std::pair<std::string_view, Field>::second);
    return found != kFieldNames.end() ? found->first : std::string_view{};
}

SpriteInstance SpriteLayout::makeSprite(std::span<const float> values, std::size_t index) const noexcept {
    SpriteInstance sprite = base;
    apply(values, index, sprite);
    return sprite;
}

void SpriteLayout::apply(std::span<const float> values, std::size_t index, SpriteInstance& sprite) const noexcept {
    const float* source = values.data() + index * fields.size();
    for (std::size_t field = 0; field < fields.size(); ++field) {
        select(fields[field], sprite) = source[field];
    }
}

void SpriteLayout::store(const SpriteInstance& sprite, std::span<float> values, std::size_t index) const noexcept {
    float* target = values.data() + index * fields.size();
    for (std::size_t field = 0; field < fields.size(); ++field) {
        target[field] = select(fields[field], sprite);
    }
}

} // namespace haylen::graphics2d
