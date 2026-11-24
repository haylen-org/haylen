#pragma once

#include <lua.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/2d/graphics/Material.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/Parallax.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SceneTransition.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/graphics/SpriteEffect.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/EnumNames.hpp"
#include "haylen/core/MotionReduction.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/GamepadLabels.hpp"
#include "haylen/input/InputDevice.hpp"
#include "haylen/input/TouchPhase.hpp"
#include "haylen/lua/Converter.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Circle.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Easing.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/FloatRange.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/Gradient.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::lua {

// Reads the option tables of engine types and holds the component and name tables their converters share.
class TypeConverter final {
  public:
    static constexpr std::array<std::string_view, 15> kDrawOrderFields{"layer", "depth", "sortOffset", "sortId", "visibility", "blend", "material", "partMask", "normalMap", "specular", "shininess", "emission", "lightMask", "unshaded", "distortion"};
    static constexpr std::array<std::string_view, 8> kSpriteEffectFields{"dissolve", "dissolveEdge", "dissolveSize", "dissolveColor", "outlineWidth", "outlineColor", "glowSize", "glowColor"};
    static constexpr std::array<std::string_view, 28> kTextStyleFields{"size", "color", "outlineWidth", "outlineColor", "shadowOffset", "shadowColor", "shadowBlur", "gradient", "align", "maxWidth", "maxHeight", "minScale", "wrap", "balance", "lineSpacing", "lineHeight", "anchor", "rotation", "scale", "bold", "italic", "underline", "strike", "direction", "language", "pixelSnap", "ligatures", "code"};
    static constexpr std::array<std::string_view, 3> kTextGradientFields{"stops", "colors", "angle"};
    static constexpr std::array<std::string_view, 3> kTextureOptionFields{"filter", "wrap", "mipmaps"};
    static constexpr std::array<std::string_view, 14> kSpriteInstanceFields{"x", "y", "width", "height", "source", "pivotX", "pivotY", "rotation", "tilt", "color", "flash", "flipHorizontal", "flipVertical", "flipDiagonal"};

    // Option readers accept `nil` for the defaults and reject keys outside their own fields and the extra fields the caller reads from the same table.
    [[nodiscard]] static haylen::graphics2d::DrawOrder readDrawOrder(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields = {});
    [[nodiscard]] static haylen::text::Style readTextStyle(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields = {});

    // Reads a text gradient from `{stops = {{offset, color}, ...}}` or `{colors = {...}}` with an optional `angle`, into the fixed room of its stops, so reading it for every draw allocates nothing.
    [[nodiscard]] static haylen::text::Gradient readTextGradient(lua_State* L, int index);
    [[nodiscard]] static haylen::graphics::Texture::Options readTextureOptions(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields = {});

    // Reads the bytes of a Lua string or of a live data view of Varn, which share their memory instead of copying it, valid while the value stays on the stack. Throws `std::invalid_argument` for any other value.
    [[nodiscard]] static std::string_view readBytes(lua_State* L, int index);

    // Reads a sprite table over `base`. A sprite left without a size takes the size of its source, or of the whole texture when it has no source, like a sprite drawn with `graphics.draw`.
    [[nodiscard]] static haylen::graphics2d::SpriteInstance readSpriteInstance(lua_State* L, int index, const haylen::graphics::Texture& texture, haylen::graphics2d::SpriteInstance base = {}, std::initializer_list<Table::FieldNames> extraFields = {});

  private:
    template <typename T> friend struct Converter;
    template <typename T> friend struct haylen::core::EnumNames;

    template <typename T, std::size_t Size> using NameTable = std::array<std::pair<std::string_view, T>, Size>;

    [[nodiscard]] static std::size_t readTextGradientStops(lua_State* L, std::string_view key, std::array<haylen::text::Gradient::Stop, haylen::text::Gradient::kMaxStops>& stops);

    static const NameTable<haylen::graphics2d::Renderer::SortMode, 3> kSortModes;
    static const NameTable<haylen::graphics2d::ImageBlend::Pattern, 6> kBlendPatterns;
    static const NameTable<haylen::graphics2d::SceneTransition::Kind, 24> kTransitions;
    static const NameTable<haylen::graphics2d::SceneTransition::Direction, 8> kDirections;
    static const NameTable<haylen::input::InputDevice, 3> kDevices;
    static const NameTable<haylen::input::GamepadLabels, 4> kGamepadLabels;
    static const NameTable<haylen::input::TouchPhase, 5> kPhases;
    static const NameTable<haylen::platform::Window::Passthrough, 3> kPassthroughs;
    static const NameTable<haylen::platform::TextInput::Action, 4> kTextActions;
    static const NameTable<haylen::core::MotionReduction, 3> kMotionReductions;
    static constexpr std::array<std::string_view, 8> kEasingFields{"curve", "overshoot", "amplitude", "period", "steps", "position", "cubicBezier", "points"};
    static constexpr std::array<std::string_view, 4> kPartColorFields{"red", "green", "blue", "yellow"};

    template <typename T, std::size_t Size> [[nodiscard]] static std::optional<T> fromTable(const NameTable<T, Size>& names, std::string_view name) {
        const auto found = std::ranges::find(names, name, &std::pair<std::string_view, T>::first);
        return found != names.end() ? std::optional(found->second) : std::nullopt;
    }

    // Every value of an enum has its name in the table.
    template <typename T, std::size_t Size> [[nodiscard]] static std::string_view toName(const NameTable<T, Size>& names, T value) {
        return std::ranges::find(names, value, &std::pair<std::string_view, T>::second)->first;
    }

    // Reads a named component, or the positional one when the name is absent, so `{x = 1, y = 2}` and `{1, 2}` both work.
    static void pushComponent(lua_State* L, int table, const char* name, lua_Integer position);
    [[nodiscard]] static float numberComponent(lua_State* L, int table, const char* name, lua_Integer position, std::optional<float> fallback = std::nullopt);
    [[nodiscard]] static haylen::math::Vec2 pointComponent(lua_State* L, int table, const char* name, lua_Integer position);
};

template <> struct Type<haylen::math::Vec2> {
    static constexpr const char* name = "haylen.Vec2";
    using Storage = haylen::math::Vec2;
};

template <> struct Type<haylen::math::Vec3> {
    static constexpr const char* name = "haylen.Vec3";
    using Storage = haylen::math::Vec3;
};

template <> struct Type<haylen::math::Vec4> {
    static constexpr const char* name = "haylen.Vec4";
    using Storage = haylen::math::Vec4;
};

template <> struct Type<haylen::math::Rect> {
    static constexpr const char* name = "haylen.Rect";
    using Storage = haylen::math::Rect;
};

template <> struct Type<haylen::math::Color> {
    static constexpr const char* name = "haylen.Color";
    using Storage = haylen::math::Color;
};

template <> struct Type<haylen::math::Transform2D> {
    static constexpr const char* name = "haylen.Transform2D";
    using Storage = haylen::math::Transform2D;
};

template <> struct Type<haylen::math::Random> {
    static constexpr const char* name = "haylen.Random";
    using Storage = haylen::math::Random;
};

template <> struct Type<haylen::math::Noise2D> {
    static constexpr const char* name = "haylen.Noise2D";
    using Storage = haylen::math::Noise2D;
};

template <> struct Type<haylen::math::EasingCurve> {
    static constexpr const char* name = "haylen.EasingCurve";
    using Storage = haylen::math::EasingCurve;
};

template <> struct Type<haylen::graphics::Texture> {
    static constexpr const char* name = "haylen.Texture";
    using Storage = haylen::graphics::Texture;
};

template <> struct Type<haylen::graphics::RenderTarget> {
    static constexpr const char* name = "haylen.RenderTarget";
    using Storage = haylen::graphics::RenderTarget;
};

template <> struct Type<haylen::graphics::Shader> {
    static constexpr const char* name = "haylen.Shader";
    using Storage = haylen::graphics::Shader;
};

template <> struct Type<haylen::graphics::VectorImage> {
    static constexpr const char* name = "haylen.VectorImage";
    using Storage = haylen::graphics::VectorImage;
};

template <> struct Type<haylen::graphics2d::Material> {
    static constexpr const char* name = "haylen.Material";
    using Storage = haylen::graphics2d::Material;
};

template <> struct Type<haylen::text::Font> {
    static constexpr const char* name = "haylen.Font";
    using Storage = std::shared_ptr<haylen::text::Font>;
};

template <> struct Type<haylen::animation2d::Animation> {
    static constexpr const char* name = "haylen.Animation";
    using Storage = haylen::animation2d::Animation;
};

template <> struct Type<haylen::animation2d::Animator> {
    static constexpr const char* name = "haylen.Animator";
    using Storage = std::shared_ptr<haylen::animation2d::Animator>;
};

template <> struct Type<haylen::animation2d::SpriteAtlas> {
    static constexpr const char* name = "haylen.SpriteAtlas";
    using Storage = std::shared_ptr<haylen::animation2d::SpriteAtlas>;
};

template <> struct Type<haylen::audio::Sound> {
    static constexpr const char* name = "haylen.Sound";
    using Storage = haylen::audio::Sound;
};

template <> struct Type<haylen::graphics2d::Camera> {
    static constexpr const char* name = "haylen.Camera";
    using Storage = haylen::graphics2d::Camera;
};

template <> struct Type<haylen::graphics2d::Parallax> {
    static constexpr const char* name = "haylen.Parallax";
    using Storage = haylen::graphics2d::Parallax;
};

template <> struct Type<haylen::graphics2d::NineSlice> {
    static constexpr const char* name = "haylen.NineSlice";
    using Storage = haylen::graphics2d::NineSlice;
};

template <> struct Type<haylen::graphics2d::Sprite> {
    static constexpr const char* name = "haylen.Sprite";
    using Storage = haylen::graphics2d::Sprite;
};

template <> struct Type<haylen::graphics2d::SpriteBatch> {
    static constexpr const char* name = "haylen.SpriteBatch";
    using Storage = std::shared_ptr<haylen::graphics2d::SpriteBatch>;
};

template <> struct Type<haylen::graphics2d::StaticSpriteBatch> {
    static constexpr const char* name = "haylen.StaticSpriteBatch";
    using Storage = haylen::graphics2d::StaticSpriteBatch;
};

template <> struct Type<haylen::core::Connection> {
    static constexpr const char* name = "haylen.Connection";
    using Storage = haylen::core::Connection;
};

// Vectors accept a `Vec2` or a table with `x` and `y` fields or the two values in order.
template <> struct Converter<haylen::math::Vec2> {
    static void push(lua_State* L, haylen::math::Vec2 value) {
        Userdata::emplace<haylen::math::Vec2>(L, value);
    }
    static haylen::math::Vec2 read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Vectors accept a `Vec3` or a table with `x`, `y` and `z` fields or the three values in order.
template <> struct Converter<haylen::math::Vec3> {
    static void push(lua_State* L, haylen::math::Vec3 value) {
        Userdata::emplace<haylen::math::Vec3>(L, value);
    }
    static haylen::math::Vec3 read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Vectors accept a `Vec4` or a table with `x`, `y`, `z` and `w` fields or the four values in order.
template <> struct Converter<haylen::math::Vec4> {
    static void push(lua_State* L, haylen::math::Vec4 value) {
        Userdata::emplace<haylen::math::Vec4>(L, value);
    }
    static haylen::math::Vec4 read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Rectangles accept a `Rect` or a table with `x`, `y`, `width` and `height` fields or the same four values in order.
template <> struct Converter<haylen::math::Rect> {
    static void push(lua_State* L, haylen::math::Rect value) {
        Userdata::emplace<haylen::math::Rect>(L, value);
    }
    static haylen::math::Rect read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Colors accept a `Color`, a `#RRGGBB` or `#AARRGGBB` string, or a table with `r`, `g`, `b` and optional `a` fields or the same values in order.
template <> struct Converter<haylen::math::Color> {
    static void push(lua_State* L, haylen::math::Color value) {
        Userdata::emplace<haylen::math::Color>(L, value);
    }
    static haylen::math::Color read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Circles accept a table with `center` and `radius` fields or the same two values in order.
template <> struct Converter<haylen::math::Circle> {
    static haylen::math::Circle read(lua_State* L, int index);
    // A table counts as a circle when it has a `radius` field or holds exactly two values in order.
    static bool is(lua_State* L, int index);
};

// Segments accept a table with `start` and `end` fields or the two points in order.
template <> struct Converter<haylen::math::Segment> {
    static haylen::math::Segment read(lua_State* L, int index);
};

// Insets accept one number for every side, or a table with `left`, `top`, `right` and `bottom` fields or the same four values in order, and push as a table with the four fields.
template <> struct Converter<haylen::math::Insets> {
    static void push(lua_State* L, const haylen::math::Insets& value);
    static haylen::math::Insets read(lua_State* L, int index);
};

// Part colors accept a table with any of the colors `red`, `green`, `blue` and `yellow`, white where it has none, and push as a table with the four colors.
template <> struct Converter<haylen::graphics2d::PartColors> {
    static void push(lua_State* L, const haylen::graphics2d::PartColors& value);
    static haylen::graphics2d::PartColors read(lua_State* L, int index);
};

// Easing curves accept a curve name such as `quadOut`, a function of the progress that returns the eased progress, an `EasingCurve` value, or a table: `{curve = 'backOut', overshoot = 3}`, `{curve = 'elasticOut', amplitude = 1.5, period = 0.4}`, `{steps = 4, position = 'end'}`, `{cubicBezier = {x1, y1, x2, y2}}` or `{points = {{x, y}, ...}}`, where `points` can also be plain numbers spread evenly from 0 to 1. They push back in the same forms, and a curve made from a function pushes as an `EasingCurve` value that Lua calls like the function.
template <> struct Converter<haylen::math::EasingCurve> {
    static void push(lua_State* L, const haylen::math::EasingCurve& value);
    static haylen::math::EasingCurve read(lua_State* L, int index);

    // Makes a curve of the function at index, held through the owner at the owner index like the listeners of an owner, or strongly with an owner index of 0. A curve whose owner is gone returns its progress unchanged.
    static haylen::math::EasingCurve readFunction(lua_State* L, int index, int owner);
};

// Sprite effects accept a table with the keys `dissolve`, `dissolveEdge`, `dissolveSize`, `dissolveColor`, `outlineWidth`, `outlineColor`, `glowSize` and `glowColor`, the defaults where it has none, and push as a table with every key.
template <> struct Converter<haylen::graphics2d::SpriteEffect> {
    static void push(lua_State* L, const haylen::graphics2d::SpriteEffect& value);
    static haylen::graphics2d::SpriteEffect read(lua_State* L, int index);
};

// Ranges accept one number for both ends or a `{min, max}` pair, and push as a `{min, max}` pair.
template <> struct Converter<haylen::math::FloatRange> {
    static void push(lua_State* L, haylen::math::FloatRange value);
    static haylen::math::FloatRange read(lua_State* L, int index);
};

} // namespace haylen::lua

namespace haylen::core {

template <> struct EnumNames<haylen::graphics::BlendMode::Type> {
    static std::optional<haylen::graphics::BlendMode::Type> fromName(std::string_view name) {
        return haylen::graphics::BlendMode::parse(name);
    }
    static std::string_view name(haylen::graphics::BlendMode::Type value) {
        return haylen::graphics::BlendMode::name(value);
    }
};

template <> struct EnumNames<haylen::graphics::Texture::Filter> {
    static std::optional<haylen::graphics::Texture::Filter> fromName(std::string_view name) {
        return haylen::graphics::Texture::filterFromName(name);
    }
    static std::string_view name(haylen::graphics::Texture::Filter value) {
        return haylen::graphics::Texture::filterName(value);
    }
};

template <> struct EnumNames<haylen::graphics::Texture::Wrap> {
    static std::optional<haylen::graphics::Texture::Wrap> fromName(std::string_view name) {
        return haylen::graphics::Texture::wrapFromName(name);
    }
    static std::string_view name(haylen::graphics::Texture::Wrap value) {
        return haylen::graphics::Texture::wrapName(value);
    }
};

template <> struct EnumNames<haylen::graphics::Viewport::ScalingPolicy> {
    static std::optional<haylen::graphics::Viewport::ScalingPolicy> fromName(std::string_view name) {
        return haylen::graphics::Viewport::scalingPolicyFromName(name);
    }
    static std::string_view name(haylen::graphics::Viewport::ScalingPolicy value) {
        return haylen::graphics::Viewport::scalingPolicyName(value);
    }
};

template <> struct EnumNames<haylen::animation2d::Animation::Loop> {
    static std::optional<haylen::animation2d::Animation::Loop> fromName(std::string_view name) {
        return haylen::animation2d::Animation::loopFromName(name);
    }
    static std::string_view name(haylen::animation2d::Animation::Loop value) {
        return haylen::animation2d::Animation::loopName(value);
    }
};

template <> struct EnumNames<haylen::graphics2d::Renderer::SortMode> {
    static std::optional<haylen::graphics2d::Renderer::SortMode> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kSortModes, name);
    }
    static std::string_view name(haylen::graphics2d::Renderer::SortMode value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kSortModes, value);
    }
};

template <> struct EnumNames<haylen::graphics2d::Camera::Anchor> {
    static std::optional<haylen::graphics2d::Camera::Anchor> fromName(std::string_view name) {
        return haylen::graphics2d::Camera::anchorFromName(name);
    }
    static std::string_view name(haylen::graphics2d::Camera::Anchor value) {
        return haylen::graphics2d::Camera::anchorName(value);
    }
};

template <> struct EnumNames<haylen::graphics2d::ImageBlend::Pattern> {
    static std::optional<haylen::graphics2d::ImageBlend::Pattern> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kBlendPatterns, name);
    }
    static std::string_view name(haylen::graphics2d::ImageBlend::Pattern value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kBlendPatterns, value);
    }
};

template <> struct EnumNames<haylen::graphics2d::SceneTransition::Kind> {
    static std::optional<haylen::graphics2d::SceneTransition::Kind> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kTransitions, name);
    }
    static std::string_view name(haylen::graphics2d::SceneTransition::Kind value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kTransitions, value);
    }
};

template <> struct EnumNames<haylen::graphics2d::SceneTransition::Direction> {
    static std::optional<haylen::graphics2d::SceneTransition::Direction> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kDirections, name);
    }
    static std::string_view name(haylen::graphics2d::SceneTransition::Direction value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kDirections, value);
    }
};

template <> struct EnumNames<haylen::graphics2d::NineSlice::Fill> {
    static std::optional<haylen::graphics2d::NineSlice::Fill> fromName(std::string_view name) {
        return haylen::graphics2d::NineSlice::fillFromName(name);
    }
    static std::string_view name(haylen::graphics2d::NineSlice::Fill value) {
        return haylen::graphics2d::NineSlice::fillName(value);
    }
};

template <> struct EnumNames<haylen::text::Alignment> {
    static std::optional<haylen::text::Alignment> fromName(std::string_view name) {
        return haylen::text::Style::alignmentFromName(name);
    }
    static std::string_view name(haylen::text::Alignment value) {
        return haylen::text::Style::alignmentName(value);
    }
};

template <> struct EnumNames<haylen::text::Direction> {
    static std::optional<haylen::text::Direction> fromName(std::string_view name) {
        return haylen::text::Style::directionFromName(name);
    }
    static std::string_view name(haylen::text::Direction value) {
        return haylen::text::Style::directionName(value);
    }
};

template <> struct EnumNames<haylen::text::Balance> {
    static std::optional<haylen::text::Balance> fromName(std::string_view name) {
        return haylen::text::Style::balanceFromName(name);
    }
    static std::string_view name(haylen::text::Balance value) {
        return haylen::text::Style::balanceName(value);
    }
};

template <> struct EnumNames<haylen::text::LineHeight> {
    static std::optional<haylen::text::LineHeight> fromName(std::string_view name) {
        return haylen::text::Style::lineHeightFromName(name);
    }
    static std::string_view name(haylen::text::LineHeight value) {
        return haylen::text::Style::lineHeightName(value);
    }
};

template <> struct EnumNames<haylen::math::Easing::Type> {
    static std::optional<haylen::math::Easing::Type> fromName(std::string_view name) {
        return haylen::math::Easing::parse(name);
    }
    static std::string_view name(haylen::math::Easing::Type value) {
        return haylen::math::Easing::name(value);
    }
};

template <> struct EnumNames<haylen::math::Easing::StepPosition> {
    static std::optional<haylen::math::Easing::StepPosition> fromName(std::string_view name) {
        return haylen::math::Easing::parseStepPosition(name);
    }
    static std::string_view name(haylen::math::Easing::StepPosition value) {
        return haylen::math::Easing::stepPositionName(value);
    }
};

template <> struct EnumNames<haylen::core::MotionReduction> {
    static std::optional<haylen::core::MotionReduction> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kMotionReductions, name);
    }
    static std::string_view name(haylen::core::MotionReduction value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kMotionReductions, value);
    }
};

template <> struct EnumNames<haylen::platform::Window::Cursor> {
    static std::optional<haylen::platform::Window::Cursor> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(haylen::platform::Window::kCursorNames, name);
    }
    static std::string_view name(haylen::platform::Window::Cursor value) {
        return lua::TypeConverter::toName(haylen::platform::Window::kCursorNames, value);
    }
};

template <> struct EnumNames<haylen::platform::Window::Passthrough> {
    static std::optional<haylen::platform::Window::Passthrough> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kPassthroughs, name);
    }
    static std::string_view name(haylen::platform::Window::Passthrough value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kPassthroughs, value);
    }
};

template <> struct EnumNames<haylen::input::InputDevice> {
    static std::optional<haylen::input::InputDevice> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kDevices, name);
    }
    static std::string_view name(haylen::input::InputDevice value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kDevices, value);
    }
};

template <> struct EnumNames<haylen::input::GamepadLabels> {
    static std::optional<haylen::input::GamepadLabels> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kGamepadLabels, name);
    }
    static std::string_view name(haylen::input::GamepadLabels value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kGamepadLabels, value);
    }
};

template <> struct EnumNames<haylen::input::TouchPhase> {
    static std::optional<haylen::input::TouchPhase> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kPhases, name);
    }
    static std::string_view name(haylen::input::TouchPhase value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kPhases, value);
    }
};

template <> struct EnumNames<haylen::input::Key> {
    static std::optional<haylen::input::Key> fromName(std::string_view name) {
        return haylen::input::Controls::keyFromName(name);
    }
    static std::string_view name(haylen::input::Key value) {
        return haylen::input::Controls::keyName(value);
    }
};

template <> struct EnumNames<haylen::input::MouseButton> {
    static std::optional<haylen::input::MouseButton> fromName(std::string_view name) {
        return haylen::input::Controls::mouseButtonFromName(name);
    }
    static std::string_view name(haylen::input::MouseButton value) {
        return haylen::input::Controls::mouseButtonName(value);
    }
};

template <> struct EnumNames<haylen::input::GamepadButton> {
    static std::optional<haylen::input::GamepadButton> fromName(std::string_view name) {
        return haylen::input::Controls::gamepadButtonFromName(name);
    }
    static std::string_view name(haylen::input::GamepadButton value) {
        return haylen::input::Controls::gamepadButtonName(value);
    }
};

template <> struct EnumNames<haylen::input::GamepadAxis> {
    static std::optional<haylen::input::GamepadAxis> fromName(std::string_view name) {
        return haylen::input::Controls::gamepadAxisFromName(name);
    }
    static std::string_view name(haylen::input::GamepadAxis value) {
        return haylen::input::Controls::gamepadAxisName(value);
    }
};

template <> struct EnumNames<haylen::input::MotionSensor> {
    static std::optional<haylen::input::MotionSensor> fromName(std::string_view name) {
        return haylen::input::Controls::motionSensorFromName(name);
    }
    static std::string_view name(haylen::input::MotionSensor value) {
        return haylen::input::Controls::motionSensorName(value);
    }
};

template <> struct EnumNames<haylen::platform::Event::Type> {
    static std::optional<haylen::platform::Event::Type> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(haylen::platform::Event::kTypeNames, name);
    }
    static std::string_view name(haylen::platform::Event::Type value) {
        return lua::TypeConverter::toName(haylen::platform::Event::kTypeNames, value);
    }
};

template <> struct EnumNames<haylen::platform::Window::StatusBar> {
    static std::optional<haylen::platform::Window::StatusBar> fromName(std::string_view name) {
        return haylen::platform::Window::statusBarFromName(name);
    }
    static std::string_view name(haylen::platform::Window::StatusBar value) {
        return haylen::platform::Window::statusBarName(value);
    }
};

template <> struct EnumNames<haylen::platform::Window::SystemGestures> {
    static std::optional<haylen::platform::Window::SystemGestures> fromName(std::string_view name) {
        return haylen::platform::Window::systemGesturesFromName(name);
    }
    static std::string_view name(haylen::platform::Window::SystemGestures value) {
        return haylen::platform::Window::systemGesturesName(value);
    }
};

template <> struct EnumNames<haylen::platform::Window::Redraw> {
    static std::optional<haylen::platform::Window::Redraw> fromName(std::string_view name) {
        return haylen::platform::Window::redrawFromName(name);
    }
    static std::string_view name(haylen::platform::Window::Redraw value) {
        return haylen::platform::Window::redrawName(value);
    }
};

template <> struct EnumNames<haylen::platform::Orientation> {
    static std::optional<haylen::platform::Orientation> fromName(std::string_view name) {
        return haylen::platform::Window::orientationFromName(name);
    }
    static std::string_view name(haylen::platform::Orientation value) {
        return haylen::platform::Window::orientationName(value);
    }
};

template <> struct EnumNames<haylen::platform::TextInput::Action> {
    static std::optional<haylen::platform::TextInput::Action> fromName(std::string_view name) {
        return lua::TypeConverter::fromTable(lua::TypeConverter::kTextActions, name);
    }
    static std::string_view name(haylen::platform::TextInput::Action value) {
        return lua::TypeConverter::toName(lua::TypeConverter::kTextActions, value);
    }
};

} // namespace haylen::core
