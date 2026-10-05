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
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/ProcessMode.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Controls.hpp"
#include "haylen/input/InputDevice.hpp"
#include "haylen/input/TouchPhase.hpp"
#include "haylen/lua/Converter.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Circle.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Easing.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Segment.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/platform/Orientation.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/platform/Window.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::lua {

// Reads the option tables of engine types and holds the component and name tables their converters share.
class TypeConverter final {
  public:
    static constexpr std::array<std::string_view, 13> kDrawOrderFields{"layer", "depth", "sortOffset", "visibility", "blend", "material", "partMask", "normalMap", "specular", "shininess", "emission", "lightMask", "unshaded"};
    static constexpr std::array<std::string_view, 18> kTextStyleFields{"size", "color", "outlineWidth", "outlineColor", "shadowOffset", "shadowColor", "shadowBlur", "align", "maxWidth", "lineSpacing", "anchor", "rotation", "scale", "bold", "italic", "direction", "language", "pixelSnap"};
    static constexpr std::array<std::string_view, 2> kTextureOptionFields{"filter", "wrap"};
    static constexpr std::array<std::string_view, 13> kSpriteInstanceFields{"x", "y", "width", "height", "source", "pivotX", "pivotY", "rotation", "color", "flash", "flipHorizontal", "flipVertical", "flipDiagonal"};

    // Option readers accept `nil` for the defaults and reject keys outside their own fields and the extra fields the caller reads from the same table.
    [[nodiscard]] static graphics2d::DrawOrder readDrawOrder(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields = {});
    [[nodiscard]] static text::Style readTextStyle(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields = {});
    [[nodiscard]] static graphics::Texture::Options readTextureOptions(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields = {});

    // Reads a sprite table over `base`. A sprite left without a size takes the size of its source, or of the whole texture when it has no source, like a sprite drawn with `graphics.draw`.
    [[nodiscard]] static graphics2d::SpriteInstance readSpriteInstance(lua_State* L, int index, const graphics::Texture& texture, graphics2d::SpriteInstance base = {}, std::initializer_list<Table::FieldNames> extraFields = {});

  private:
    template <typename T> friend struct Converter;
    template <typename T> friend struct EnumNames;

    template <typename T, std::size_t Size> using NameTable = std::array<std::pair<std::string_view, T>, Size>;

    static const NameTable<graphics2d::Renderer::SortMode, 3> kSortModes;
    static const NameTable<graphics2d::ImageBlend::Pattern, 5> kBlendPatterns;
    static const NameTable<graphics2d::SceneTransition::Kind, 24> kTransitions;
    static const NameTable<graphics2d::SceneTransition::Direction, 8> kDirections;
    static const NameTable<input::InputDevice, 3> kDevices;
    static const NameTable<input::TouchPhase, 5> kPhases;
    static const NameTable<platform::Window::Cursor, 11> kCursors;
    static const NameTable<platform::Event::Type, 28> kEvents;
    static const NameTable<platform::Window::Passthrough, 3> kPassthroughs;
    static const NameTable<platform::TextInput::Action, 4> kTextActions;
    static const NameTable<core::ProcessMode, 5> kProcessModes;
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
    [[nodiscard]] static math::Vec2 pointComponent(lua_State* L, int table, const char* name, lua_Integer position);

    static void checkWithExtras(lua_State* L, int table, Table::FieldNames own, std::initializer_list<Table::FieldNames> extraFields);
};

template <> struct Type<math::Vec2> {
    static constexpr const char* name = "haylen.Vec2";
    using Storage = math::Vec2;
};

template <> struct Type<math::Rect> {
    static constexpr const char* name = "haylen.Rect";
    using Storage = math::Rect;
};

template <> struct Type<math::Color> {
    static constexpr const char* name = "haylen.Color";
    using Storage = math::Color;
};

template <> struct Type<math::Transform2D> {
    static constexpr const char* name = "haylen.Transform2D";
    using Storage = math::Transform2D;
};

template <> struct Type<math::Random> {
    static constexpr const char* name = "haylen.Random";
    using Storage = math::Random;
};

template <> struct Type<math::Noise2D> {
    static constexpr const char* name = "haylen.Noise2D";
    using Storage = math::Noise2D;
};

template <> struct Type<graphics::Texture> {
    static constexpr const char* name = "haylen.Texture";
    using Storage = graphics::Texture;
};

template <> struct Type<graphics::RenderTarget> {
    static constexpr const char* name = "haylen.RenderTarget";
    using Storage = graphics::RenderTarget;
};

template <> struct Type<graphics::Shader> {
    static constexpr const char* name = "haylen.Shader";
    using Storage = graphics::Shader;
};

template <> struct Type<graphics2d::Material> {
    static constexpr const char* name = "haylen.Material";
    using Storage = graphics2d::Material;
};

template <> struct Type<text::Font> {
    static constexpr const char* name = "haylen.Font";
    using Storage = std::shared_ptr<text::Font>;
};

template <> struct Type<animation2d::Animation> {
    static constexpr const char* name = "haylen.Animation";
    using Storage = animation2d::Animation;
};

template <> struct Type<animation2d::Animator> {
    static constexpr const char* name = "haylen.Animator";
    using Storage = std::shared_ptr<animation2d::Animator>;
};

template <> struct Type<animation2d::SpriteAtlas> {
    static constexpr const char* name = "haylen.SpriteAtlas";
    using Storage = std::shared_ptr<animation2d::SpriteAtlas>;
};

template <> struct Type<audio::Sound> {
    static constexpr const char* name = "haylen.Sound";
    using Storage = audio::Sound;
};

template <> struct Type<graphics2d::Camera> {
    static constexpr const char* name = "haylen.Camera";
    using Storage = graphics2d::Camera;
};

template <> struct Type<graphics2d::Parallax> {
    static constexpr const char* name = "haylen.Parallax";
    using Storage = graphics2d::Parallax;
};

template <> struct Type<graphics2d::NineSlice> {
    static constexpr const char* name = "haylen.NineSlice";
    using Storage = graphics2d::NineSlice;
};

template <> struct Type<graphics2d::Sprite> {
    static constexpr const char* name = "haylen.Sprite";
    using Storage = graphics2d::Sprite;
};

template <> struct Type<graphics2d::SpriteBatch> {
    static constexpr const char* name = "haylen.SpriteBatch";
    using Storage = std::shared_ptr<graphics2d::SpriteBatch>;
};

template <> struct Type<graphics2d::StaticSpriteBatch> {
    static constexpr const char* name = "haylen.StaticSpriteBatch";
    using Storage = graphics2d::StaticSpriteBatch;
};

template <> struct Type<core::Connection> {
    static constexpr const char* name = "haylen.Connection";
    using Storage = core::Connection;
};

// Vectors accept a `Vec2` or a table with `x` and `y` fields or the two values in order.
template <> struct Converter<math::Vec2> {
    static void push(lua_State* L, math::Vec2 value) {
        Userdata::emplace<math::Vec2>(L, value);
    }
    static math::Vec2 read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Rectangles accept a `Rect` or a table with `x`, `y`, `width` and `height` fields or the same four values in order.
template <> struct Converter<math::Rect> {
    static void push(lua_State* L, math::Rect value) {
        Userdata::emplace<math::Rect>(L, value);
    }
    static math::Rect read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Colors accept a `Color`, a `#RRGGBB` or `#AARRGGBB` string, or a table with `r`, `g`, `b` and optional `a` fields or the same values in order.
template <> struct Converter<math::Color> {
    static void push(lua_State* L, math::Color value) {
        Userdata::emplace<math::Color>(L, value);
    }
    static math::Color read(lua_State* L, int index);
    static bool is(lua_State* L, int index);
};

// Circles accept a table with `center` and `radius` fields or the same two values in order.
template <> struct Converter<math::Circle> {
    static math::Circle read(lua_State* L, int index);
    // A table counts as a circle when it has a `radius` field or holds exactly two values in order.
    static bool is(lua_State* L, int index);
};

// Segments accept a table with `start` and `end` fields or the two points in order.
template <> struct Converter<math::Segment> {
    static math::Segment read(lua_State* L, int index);
};

// Insets accept one number for every side, or a table with `left`, `top`, `right` and `bottom` fields or the same four values in order, and push as a table with the four fields.
template <> struct Converter<math::Insets> {
    static void push(lua_State* L, const math::Insets& value);
    static math::Insets read(lua_State* L, int index);
};

// Part colors accept a table with any of the colors `red`, `green`, `blue` and `yellow`, white where it has none, and push as a table with the four colors.
template <> struct Converter<graphics2d::PartColors> {
    static void push(lua_State* L, const graphics2d::PartColors& value);
    static graphics2d::PartColors read(lua_State* L, int index);
};

// Easing curves accept a curve name such as `quadOut`, a function of the progress that returns the eased progress, or a table: `{curve = 'backOut', overshoot = 3}`, `{curve = 'elasticOut', amplitude = 1.5, period = 0.4}`, `{steps = 4, position = 'end'}`, `{cubicBezier = {x1, y1, x2, y2}}` or `{points = {{x, y}, ...}}`, where `points` can also be plain numbers spread evenly from 0 to 1.
template <> struct Converter<math::EasingCurve> {
    static math::EasingCurve read(lua_State* L, int index);
};

template <> struct EnumNames<graphics::BlendMode::Type> {
    static std::optional<graphics::BlendMode::Type> fromName(std::string_view name) {
        return graphics::BlendMode::parse(name);
    }
    static std::string_view name(graphics::BlendMode::Type value) {
        return graphics::BlendMode::name(value);
    }
};

template <> struct EnumNames<graphics::Texture::Filter> {
    static std::optional<graphics::Texture::Filter> fromName(std::string_view name) {
        return graphics::Texture::filterFromName(name);
    }
    static std::string_view name(graphics::Texture::Filter value) {
        return graphics::Texture::filterName(value);
    }
};

template <> struct EnumNames<graphics::Texture::Wrap> {
    static std::optional<graphics::Texture::Wrap> fromName(std::string_view name) {
        return graphics::Texture::wrapFromName(name);
    }
    static std::string_view name(graphics::Texture::Wrap value) {
        return graphics::Texture::wrapName(value);
    }
};

template <> struct EnumNames<graphics::Viewport::ScalingPolicy> {
    static std::optional<graphics::Viewport::ScalingPolicy> fromName(std::string_view name) {
        return graphics::Viewport::scalingPolicyFromName(name);
    }
    static std::string_view name(graphics::Viewport::ScalingPolicy value) {
        return graphics::Viewport::scalingPolicyName(value);
    }
};

template <> struct EnumNames<animation2d::Animation::Loop> {
    static std::optional<animation2d::Animation::Loop> fromName(std::string_view name) {
        return animation2d::Animation::loopFromName(name);
    }
    static std::string_view name(animation2d::Animation::Loop value) {
        return animation2d::Animation::loopName(value);
    }
};

template <> struct EnumNames<graphics2d::Renderer::SortMode> {
    static std::optional<graphics2d::Renderer::SortMode> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kSortModes, name);
    }
    static std::string_view name(graphics2d::Renderer::SortMode value) {
        return TypeConverter::toName(TypeConverter::kSortModes, value);
    }
};

template <> struct EnumNames<graphics2d::Camera::Anchor> {
    static std::optional<graphics2d::Camera::Anchor> fromName(std::string_view name) {
        return graphics2d::Camera::anchorFromName(name);
    }
    static std::string_view name(graphics2d::Camera::Anchor value) {
        return graphics2d::Camera::anchorName(value);
    }
};

template <> struct EnumNames<graphics2d::ImageBlend::Pattern> {
    static std::optional<graphics2d::ImageBlend::Pattern> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kBlendPatterns, name);
    }
    static std::string_view name(graphics2d::ImageBlend::Pattern value) {
        return TypeConverter::toName(TypeConverter::kBlendPatterns, value);
    }
};

template <> struct EnumNames<graphics2d::SceneTransition::Kind> {
    static std::optional<graphics2d::SceneTransition::Kind> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kTransitions, name);
    }
    static std::string_view name(graphics2d::SceneTransition::Kind value) {
        return TypeConverter::toName(TypeConverter::kTransitions, value);
    }
};

template <> struct EnumNames<graphics2d::SceneTransition::Direction> {
    static std::optional<graphics2d::SceneTransition::Direction> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kDirections, name);
    }
    static std::string_view name(graphics2d::SceneTransition::Direction value) {
        return TypeConverter::toName(TypeConverter::kDirections, value);
    }
};

template <> struct EnumNames<graphics2d::NineSlice::Fill> {
    static std::optional<graphics2d::NineSlice::Fill> fromName(std::string_view name) {
        return graphics2d::NineSlice::fillFromName(name);
    }
    static std::string_view name(graphics2d::NineSlice::Fill value) {
        return graphics2d::NineSlice::fillName(value);
    }
};

template <> struct EnumNames<text::Alignment> {
    static std::optional<text::Alignment> fromName(std::string_view name) {
        return text::Style::alignmentFromName(name);
    }
    static std::string_view name(text::Alignment value) {
        return text::Style::alignmentName(value);
    }
};

template <> struct EnumNames<text::Direction> {
    static std::optional<text::Direction> fromName(std::string_view name) {
        return text::Style::directionFromName(name);
    }
    static std::string_view name(text::Direction value) {
        return text::Style::directionName(value);
    }
};

template <> struct EnumNames<math::Easing::Type> {
    static std::optional<math::Easing::Type> fromName(std::string_view name) {
        return math::Easing::parse(name);
    }
    static std::string_view name(math::Easing::Type value) {
        return math::Easing::name(value);
    }
};

template <> struct EnumNames<math::Easing::StepPosition> {
    static std::optional<math::Easing::StepPosition> fromName(std::string_view name) {
        return math::Easing::parseStepPosition(name);
    }
    static std::string_view name(math::Easing::StepPosition value) {
        return math::Easing::stepPositionName(value);
    }
};

template <> struct EnumNames<core::ProcessMode> {
    static std::optional<core::ProcessMode> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kProcessModes, name);
    }
    static std::string_view name(core::ProcessMode value) {
        return TypeConverter::toName(TypeConverter::kProcessModes, value);
    }
};

template <> struct EnumNames<platform::Window::Cursor> {
    static std::optional<platform::Window::Cursor> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kCursors, name);
    }
    static std::string_view name(platform::Window::Cursor value) {
        return TypeConverter::toName(TypeConverter::kCursors, value);
    }
};

template <> struct EnumNames<platform::Window::Passthrough> {
    static std::optional<platform::Window::Passthrough> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kPassthroughs, name);
    }
    static std::string_view name(platform::Window::Passthrough value) {
        return TypeConverter::toName(TypeConverter::kPassthroughs, value);
    }
};

template <> struct EnumNames<input::InputDevice> {
    static std::optional<input::InputDevice> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kDevices, name);
    }
    static std::string_view name(input::InputDevice value) {
        return TypeConverter::toName(TypeConverter::kDevices, value);
    }
};

template <> struct EnumNames<input::TouchPhase> {
    static std::optional<input::TouchPhase> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kPhases, name);
    }
    static std::string_view name(input::TouchPhase value) {
        return TypeConverter::toName(TypeConverter::kPhases, value);
    }
};

template <> struct EnumNames<input::Key> {
    static std::optional<input::Key> fromName(std::string_view name) {
        return input::Controls::keyFromName(name);
    }
    static std::string_view name(input::Key value) {
        return input::Controls::keyName(value);
    }
};

template <> struct EnumNames<input::MouseButton> {
    static std::optional<input::MouseButton> fromName(std::string_view name) {
        return input::Controls::mouseButtonFromName(name);
    }
    static std::string_view name(input::MouseButton value) {
        return input::Controls::mouseButtonName(value);
    }
};

template <> struct EnumNames<input::GamepadButton> {
    static std::optional<input::GamepadButton> fromName(std::string_view name) {
        return input::Controls::gamepadButtonFromName(name);
    }
    static std::string_view name(input::GamepadButton value) {
        return input::Controls::gamepadButtonName(value);
    }
};

template <> struct EnumNames<input::GamepadAxis> {
    static std::optional<input::GamepadAxis> fromName(std::string_view name) {
        return input::Controls::gamepadAxisFromName(name);
    }
    static std::string_view name(input::GamepadAxis value) {
        return input::Controls::gamepadAxisName(value);
    }
};

template <> struct EnumNames<platform::Event::Type> {
    static std::optional<platform::Event::Type> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kEvents, name);
    }
    static std::string_view name(platform::Event::Type value) {
        return TypeConverter::toName(TypeConverter::kEvents, value);
    }
};

template <> struct EnumNames<platform::Orientation> {
    static std::optional<platform::Orientation> fromName(std::string_view name) {
        return platform::Window::orientationFromName(name);
    }
    static std::string_view name(platform::Orientation value) {
        return platform::Window::orientationName(value);
    }
};

template <> struct EnumNames<platform::TextInput::Action> {
    static std::optional<platform::TextInput::Action> fromName(std::string_view name) {
        return TypeConverter::fromTable(TypeConverter::kTextActions, name);
    }
    static std::string_view name(platform::TextInput::Action value) {
        return TypeConverter::toName(TypeConverter::kTextActions, value);
    }
};

} // namespace haylen::lua
