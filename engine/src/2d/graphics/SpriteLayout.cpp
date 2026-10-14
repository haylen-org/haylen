#include "haylen/2d/graphics/SpriteLayout.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace haylen::graphics2d {

SpriteLayout::SpriteLayout(std::vector<Field> layoutFields, const SpriteInstance& sprite, const SpriteData& data) : fields(std::move(layoutFields)), base(sprite), baseData(data) {
    if (fields.empty()) {
        throw std::invalid_argument("A sprite layout needs at least one field.");
    }
    dataFields = std::ranges::any_of(fields, &SpriteLayout::isDataField);
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
    const float* source = values.data() + index * fields.size();
    for (std::size_t field = 0; field < fields.size(); ++field) {
        if (!isDataField(fields[field])) {
            select(fields[field], sprite) = source[field];
        }
    }
    return sprite;
}

SpriteData SpriteLayout::makeData(std::span<const float> values, std::size_t index) const noexcept {
    SpriteData data = baseData;
    const float* source = values.data() + index * fields.size();
    for (std::size_t field = 0; field < fields.size(); ++field) {
        if (isDataField(fields[field])) {
            setData(fields[field], source[field], data);
        }
    }
    return data;
}

void SpriteLayout::apply(std::span<const float> values, std::size_t index, SpriteInstance& sprite, SpriteData& data) const noexcept {
    const float* source = values.data() + index * fields.size();
    for (std::size_t field = 0; field < fields.size(); ++field) {
        if (isDataField(fields[field])) {
            setData(fields[field], source[field], data);
        } else {
            select(fields[field], sprite) = source[field];
        }
    }
}

void SpriteLayout::store(const SpriteInstance& sprite, const SpriteData& data, std::span<float> values, std::size_t index) const noexcept {
    float* target = values.data() + index * fields.size();
    for (std::size_t field = 0; field < fields.size(); ++field) {
        if (fields[field] == Field::Texture) {
            target[field] = static_cast<float>(data.texture) + 1.0F;
        } else if (isDataField(fields[field])) {
            target[field] = data.values[dataSlot(fields[field])];
        } else {
            target[field] = select(fields[field], sprite);
        }
    }
}

// The texture counts from 1 in the values, and a value outside the textures a byte can name stays at its nearest end, so the draw names the texture it lacks.
void SpriteLayout::setData(Field field, float value, SpriteData& data) noexcept {
    if (field == Field::Texture) {
        data.texture = static_cast<std::uint8_t>(std::lround(std::clamp(value, 1.0F, 256.0F)) - 1);
        return;
    }
    data.values[dataSlot(field)] = value;
}

std::size_t SpriteLayout::dataSlot(Field field) noexcept {
    return static_cast<std::size_t>(field) - static_cast<std::size_t>(Field::Data1);
}

} // namespace haylen::graphics2d
