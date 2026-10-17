#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"

namespace haylen::graphics2d {

// Describes how a buffer of floats holds sprites: every sprite takes one float for each field, in the order of the fields, and the template sprite and its data give every value the fields leave out. Sprite batches and `drawBatch` read buffers through it, so scripts move thousands of sprites without a table for each one. The texture field counts the textures of a batch from 1, like the sprite tables of Lua, so a buffer that scripts fill reads the same from both languages.
class SpriteLayout final {
  public:
    enum class Field : std::uint8_t {
        X,
        Y,
        Width,
        Height,
        Rotation,
        Tilt,
        PivotX,
        PivotY,
        Red,
        Green,
        Blue,
        Alpha,
        SourceX,
        SourceY,
        SourceWidth,
        SourceHeight,
        Texture,
        Data1,
        Data2,
        Data3,
        Data4,
    };

    // Throws `std::invalid_argument` without fields.
    explicit SpriteLayout(std::vector<Field> layoutFields, const SpriteInstance& sprite = {}, const SpriteData& data = {});

    // Resolves the names `x`, `y`, `width`, `height`, `rotation`, `tilt`, `pivotX`, `pivotY`, `red`, `green`, `blue`, `alpha`, `sourceX`, `sourceY`, `sourceWidth`, `sourceHeight`, `texture`, `data1`, `data2`, `data3` and `data4`.
    [[nodiscard]] static std::optional<Field> fieldFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view fieldName(Field value) noexcept;

    [[nodiscard]] std::size_t getStride() const noexcept {
        return fields.size();
    }

    // Tells whether the sprites have data, from a field of the data or from the template.
    [[nodiscard]] bool hasData() const noexcept {
        return dataFields || baseData != SpriteData{};
    }
    [[nodiscard]] bool hasDataFields() const noexcept {
        return dataFields;
    }

    // Returns how many whole sprites the values hold.
    [[nodiscard]] std::size_t getCount(std::span<const float> values) const noexcept {
        return values.size() / fields.size();
    }

    // Returns the template sprite with the fields the values give the sprite at the index.
    [[nodiscard]] SpriteInstance makeSprite(std::span<const float> values, std::size_t index) const noexcept;

    // Returns the template data with the fields the values give the sprite at the index.
    [[nodiscard]] SpriteData makeData(std::span<const float> values, std::size_t index) const noexcept;

    // Copies the fields of the sprite at the index from the values into a sprite and its data, or from a sprite and its data into the values.
    void apply(std::span<const float> values, std::size_t index, SpriteInstance& sprite, SpriteData& data) const noexcept;
    void store(const SpriteInstance& sprite, const SpriteData& data, std::span<float> values, std::size_t index) const noexcept;

  private:
    static constexpr std::array<std::pair<std::string_view, Field>, 21> kFieldNames{{
        {"x", Field::X}, {"y", Field::Y}, {"width", Field::Width}, {"height", Field::Height}, {"rotation", Field::Rotation}, {"tilt", Field::Tilt}, {"pivotX", Field::PivotX}, {"pivotY", Field::PivotY}, {"red", Field::Red}, {"green", Field::Green}, {"blue", Field::Blue}, {"alpha", Field::Alpha}, {"sourceX", Field::SourceX}, {"sourceY", Field::SourceY}, {"sourceWidth", Field::SourceWidth}, {"sourceHeight", Field::SourceHeight}, {"texture", Field::Texture}, {"data1", Field::Data1}, {"data2", Field::Data2}, {"data3", Field::Data3}, {"data4", Field::Data4},
    }};

    [[nodiscard]] static bool isDataField(Field field) noexcept {
        return field >= Field::Texture;
    }
    static void setData(Field field, float value, SpriteData& data) noexcept;
    [[nodiscard]] static std::size_t dataSlot(Field field) noexcept;

    // Returns the value of the sprite a field names, writable when the sprite is.
    template <typename Sprite> [[nodiscard]] static auto& select(Field field, Sprite& sprite) noexcept {
        switch (field) {
        case Field::X:
            return sprite.position.x;
        case Field::Y:
            return sprite.position.y;
        case Field::Width:
            return sprite.size.x;
        case Field::Height:
            return sprite.size.y;
        case Field::Rotation:
            return sprite.rotation;
        case Field::Tilt:
            return sprite.tilt;
        case Field::PivotX:
            return sprite.pivot.x;
        case Field::PivotY:
            return sprite.pivot.y;
        case Field::Red:
            return sprite.color.r;
        case Field::Green:
            return sprite.color.g;
        case Field::Blue:
            return sprite.color.b;
        case Field::Alpha:
            return sprite.color.a;
        case Field::SourceX:
            return sprite.source.x;
        case Field::SourceY:
            return sprite.source.y;
        case Field::SourceWidth:
            return sprite.source.width;
        case Field::SourceHeight:
        case Field::Texture:
        case Field::Data1:
        case Field::Data2:
        case Field::Data3:
        case Field::Data4:
            break;
        }
        return sprite.source.height;
    }

    std::vector<Field> fields;
    SpriteInstance base;
    SpriteData baseData;
    bool dataFields = false;
};

} // namespace haylen::graphics2d
