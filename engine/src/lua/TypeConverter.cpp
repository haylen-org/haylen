#include "haylen/lua/TypeConverter.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"

namespace haylen::lua {

const TypeConverter::NameTable<graphics::Texture::Wrap, 3> TypeConverter::kWraps = {{{"clamp", graphics::Texture::Wrap::Clamp}, {"repeat", graphics::Texture::Wrap::Repeat}, {"mirror", graphics::Texture::Wrap::Mirror}}};
const TypeConverter::NameTable<graphics::Viewport::ScalingPolicy, 5> TypeConverter::kScaling = {{{"fit", graphics::Viewport::ScalingPolicy::Fit}, {"fill", graphics::Viewport::ScalingPolicy::Fill}, {"stretch", graphics::Viewport::ScalingPolicy::Stretch}, {"expand", graphics::Viewport::ScalingPolicy::Expand}, {"pixel_perfect", graphics::Viewport::ScalingPolicy::PixelPerfect}}};
const TypeConverter::NameTable<graphics2d::Renderer::SortMode, 3> TypeConverter::kSortModes = {{{"layer", graphics2d::Renderer::SortMode::Layer}, {"depth", graphics2d::Renderer::SortMode::Depth}, {"y", graphics2d::Renderer::SortMode::Y}}};
const TypeConverter::NameTable<graphics2d::ImageBlend::Pattern, 5> TypeConverter::kBlendPatterns = {{{"dissolve", graphics2d::ImageBlend::Pattern::Dissolve}, {"pixelate", graphics2d::ImageBlend::Pattern::Pixelate}, {"radial", graphics2d::ImageBlend::Pattern::Radial}, {"iris", graphics2d::ImageBlend::Pattern::Iris}, {"pageTurn", graphics2d::ImageBlend::Pattern::PageTurn}}};
const TypeConverter::NameTable<graphics2d::SceneTransition::Kind, 24> TypeConverter::kTransitions = {{
    {"fade", graphics2d::SceneTransition::Kind::Fade}, {"crossFade", graphics2d::SceneTransition::Kind::CrossFade}, {"moveIn", graphics2d::SceneTransition::Kind::MoveIn}, {"slideIn", graphics2d::SceneTransition::Kind::SlideIn}, {"push", graphics2d::SceneTransition::Kind::Push}, {"shrinkGrow", graphics2d::SceneTransition::Kind::ShrinkGrow}, {"flipX", graphics2d::SceneTransition::Kind::FlipX}, {"flipY", graphics2d::SceneTransition::Kind::FlipY}, {"zoomFlip", graphics2d::SceneTransition::Kind::ZoomFlip}, {"rotoZoom", graphics2d::SceneTransition::Kind::RotoZoom}, {"jumpZoom", graphics2d::SceneTransition::Kind::JumpZoom}, {"splitColumns", graphics2d::SceneTransition::Kind::SplitColumns}, {"splitRows", graphics2d::SceneTransition::Kind::SplitRows}, {"turnOffTiles", graphics2d::SceneTransition::Kind::TurnOffTiles}, {"fadeTiles", graphics2d::SceneTransition::Kind::FadeTiles}, {"pageTurn", graphics2d::SceneTransition::Kind::PageTurn}, {"radialClockwise", graphics2d::SceneTransition::Kind::RadialClockwise}, {"radialCounterclockwise", graphics2d::SceneTransition::Kind::RadialCounterclockwise}, {"wipe", graphics2d::SceneTransition::Kind::Wipe}, {"inOut", graphics2d::SceneTransition::Kind::InOut}, {"outIn", graphics2d::SceneTransition::Kind::OutIn}, {"iris", graphics2d::SceneTransition::Kind::Iris}, {"dissolve", graphics2d::SceneTransition::Kind::Dissolve}, {"pixelate", graphics2d::SceneTransition::Kind::Pixelate},
}};
const TypeConverter::NameTable<graphics2d::SceneTransition::Direction, 8> TypeConverter::kDirections = {{{"left", graphics2d::SceneTransition::Direction::Left}, {"right", graphics2d::SceneTransition::Direction::Right}, {"up", graphics2d::SceneTransition::Direction::Up}, {"down", graphics2d::SceneTransition::Direction::Down}, {"upLeft", graphics2d::SceneTransition::Direction::UpLeft}, {"upRight", graphics2d::SceneTransition::Direction::UpRight}, {"downLeft", graphics2d::SceneTransition::Direction::DownLeft}, {"downRight", graphics2d::SceneTransition::Direction::DownRight}}};
const TypeConverter::NameTable<text::TextAlign, 6> TypeConverter::kAligns = {{{"start", text::TextAlign::Start}, {"end", text::TextAlign::End}, {"left", text::TextAlign::Left}, {"center", text::TextAlign::Center}, {"right", text::TextAlign::Right}, {"fill", text::TextAlign::Fill}}};
const TypeConverter::NameTable<text::Direction, 3> TypeConverter::kTextDirections = {{{"auto", text::Direction::Auto}, {"ltr", text::Direction::LeftToRight}, {"rtl", text::Direction::RightToLeft}}};
const TypeConverter::NameTable<input::InputDevice, 3> TypeConverter::kDevices = {{{"keyboard_mouse", input::InputDevice::KeyboardMouse}, {"touch", input::InputDevice::Touch}, {"gamepad", input::InputDevice::Gamepad}}};
const TypeConverter::NameTable<input::TouchPhase, 5> TypeConverter::kPhases = {{{"began", input::TouchPhase::Began}, {"moved", input::TouchPhase::Moved}, {"stationary", input::TouchPhase::Stationary}, {"ended", input::TouchPhase::Ended}, {"cancelled", input::TouchPhase::Cancelled}}};
const TypeConverter::NameTable<platform::Window::Cursor, 11> TypeConverter::kCursors = {{
    {"default", platform::Window::Cursor::Default},
    {"arrow", platform::Window::Cursor::Arrow},
    {"ibeam", platform::Window::Cursor::IBeam},
    {"crosshair", platform::Window::Cursor::Crosshair},
    {"pointing_hand", platform::Window::Cursor::PointingHand},
    {"resize_horizontal", platform::Window::Cursor::ResizeHorizontal},
    {"resize_vertical", platform::Window::Cursor::ResizeVertical},
    {"resize_diagonal_down", platform::Window::Cursor::ResizeDiagonalDown},
    {"resize_diagonal_up", platform::Window::Cursor::ResizeDiagonalUp},
    {"resize_all", platform::Window::Cursor::ResizeAll},
    {"not_allowed", platform::Window::Cursor::NotAllowed},
}};

const TypeConverter::NameTable<platform::Window::Passthrough, 3> TypeConverter::kPassthroughs = {{{"off", platform::Window::Passthrough::Off}, {"whole", platform::Window::Passthrough::Whole}, {"regions", platform::Window::Passthrough::Regions}}};

const TypeConverter::NameTable<platform::Event::Type, 28> TypeConverter::kEvents = {{
    {"key_down", platform::Event::Type::KeyDown}, {"key_up", platform::Event::Type::KeyUp}, {"character", platform::Event::Type::Character}, {"mouse_down", platform::Event::Type::MouseDown}, {"mouse_up", platform::Event::Type::MouseUp}, {"mouse_move", platform::Event::Type::MouseMove}, {"mouse_scroll", platform::Event::Type::MouseScroll}, {"mouse_enter", platform::Event::Type::MouseEnter}, {"mouse_leave", platform::Event::Type::MouseLeave}, {"touch_began", platform::Event::Type::TouchBegan}, {"touch_moved", platform::Event::Type::TouchMoved}, {"touch_ended", platform::Event::Type::TouchEnded}, {"touch_cancelled", platform::Event::Type::TouchCancelled}, {"resized", platform::Event::Type::Resized}, {"suspended", platform::Event::Type::Suspended}, {"resumed", platform::Event::Type::Resumed}, {"focus_gained", platform::Event::Type::FocusGained}, {"focus_lost", platform::Event::Type::FocusLost}, {"quit_requested", platform::Event::Type::QuitRequested}, {"low_memory", platform::Event::Type::LowMemory}, {"text_edited", platform::Event::Type::TextEdited}, {"text_action", platform::Event::Type::TextAction}, {"keyboard_changed", platform::Event::Type::KeyboardChanged}, {"network_changed", platform::Event::Type::NetworkChanged}, {"interruption_began", platform::Event::Type::InterruptionBegan}, {"interruption_ended", platform::Event::Type::InterruptionEnded}, {"window_moved", platform::Event::Type::WindowMoved}, {"monitors_changed", platform::Event::Type::MonitorsChanged},
}};

const TypeConverter::NameTable<platform::Orientation, 3> TypeConverter::kOrientations = {{{"landscape", platform::Orientation::Landscape}, {"portrait", platform::Orientation::Portrait}, {"any", platform::Orientation::Any}}};
const TypeConverter::NameTable<platform::TextInput::Action, 4> TypeConverter::kTextActions = {{{"submit", platform::TextInput::Action::Submit}, {"next", platform::TextInput::Action::Next}, {"cancel", platform::TextInput::Action::Cancel}, {"dismissed", platform::TextInput::Action::Dismissed}}};

const TypeConverter::NameTable<core::ProcessMode, 5> TypeConverter::kProcessModes = {{{"inherit", core::ProcessMode::Inherit}, {"pausable", core::ProcessMode::Pausable}, {"whenPaused", core::ProcessMode::WhenPaused}, {"always", core::ProcessMode::Always}, {"disabled", core::ProcessMode::Disabled}}};

void TypeConverter::pushComponent(lua_State* L, int table, const char* name, lua_Integer position) {
    if (lua_getfield(L, table, name) == LUA_TNIL) {
        lua_pop(L, 1);
        lua_rawgeti(L, table, position);
    }
}

float TypeConverter::numberComponent(lua_State* L, int table, const char* name, lua_Integer position, std::optional<float> fallback) {
    pushComponent(L, table, name, position);
    if (lua_isnil(L, -1) && fallback) {
        lua_pop(L, 1);
        return *fallback;
    }
    if (lua_type(L, -1) != LUA_TNUMBER) {
        luaL_error(L, "Expected a number in field '%s'.", name);
    }
    const auto value = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return value;
}

math::Vec2 TypeConverter::pointComponent(lua_State* L, int table, const char* name, lua_Integer position) {
    pushComponent(L, table, name, position);
    if (!Stack::is<math::Vec2>(L, -1)) {
        luaL_error(L, "Expected a point in field '%s'.", name);
    }
    const math::Vec2 value = Stack::read<math::Vec2>(L, -1);
    lua_pop(L, 1);
    return value;
}

void TypeConverter::checkWithExtras(lua_State* L, int table, Table::FieldNames own, std::initializer_list<Table::FieldNames> extraFields) {
    std::vector<Table::FieldNames> allowed{own};
    allowed.insert(allowed.end(), extraFields.begin(), extraFields.end());
    Table::checkFields(L, table, allowed);
}

math::Vec2 Converter<math::Vec2>::read(lua_State* L, int index) {
    if (const math::Vec2* value = Userdata::test<math::Vec2>(L, index)) {
        return *value;
    }
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "Vec2 or table with x and y");
    }
    const int table = lua_absindex(L, index);
    return {TypeConverter::numberComponent(L, table, "x", 1), TypeConverter::numberComponent(L, table, "y", 2)};
}

bool Converter<math::Vec2>::is(lua_State* L, int index) {
    return Userdata::test<math::Vec2>(L, index) != nullptr || lua_istable(L, index);
}

math::Rect Converter<math::Rect>::read(lua_State* L, int index) {
    if (const math::Rect* value = Userdata::test<math::Rect>(L, index)) {
        return *value;
    }
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "Rect or table with x, y, width and height");
    }
    const int table = lua_absindex(L, index);
    return {TypeConverter::numberComponent(L, table, "x", 1), TypeConverter::numberComponent(L, table, "y", 2), TypeConverter::numberComponent(L, table, "width", 3), TypeConverter::numberComponent(L, table, "height", 4)};
}

bool Converter<math::Rect>::is(lua_State* L, int index) {
    return Userdata::test<math::Rect>(L, index) != nullptr || lua_istable(L, index);
}

math::Color Converter<math::Color>::read(lua_State* L, int index) {
    if (const math::Color* value = Userdata::test<math::Color>(L, index)) {
        return *value;
    }
    if (lua_type(L, index) == LUA_TSTRING) {
        const std::optional<math::Color> parsed = math::Color::parse(lua_tostring(L, index));
        if (!parsed) {
            luaL_argerror(L, index, "invalid color text, expected #RRGGBB or #AARRGGBB");
        }
        return *parsed;
    }
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "Color, color text or table with r, g and b");
    }
    const int table = lua_absindex(L, index);
    return {TypeConverter::numberComponent(L, table, "r", 1), TypeConverter::numberComponent(L, table, "g", 2), TypeConverter::numberComponent(L, table, "b", 3), TypeConverter::numberComponent(L, table, "a", 4, 1.0F)};
}

bool Converter<math::Color>::is(lua_State* L, int index) {
    return Userdata::test<math::Color>(L, index) != nullptr || lua_type(L, index) == LUA_TSTRING || lua_istable(L, index);
}

math::Circle Converter<math::Circle>::read(lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "table with center and radius");
    }
    const int table = lua_absindex(L, index);
    return {TypeConverter::pointComponent(L, table, "center", 1), TypeConverter::numberComponent(L, table, "radius", 2)};
}

bool Converter<math::Circle>::is(lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        return false;
    }
    const bool named = lua_getfield(L, index, "radius") != LUA_TNIL;
    lua_pop(L, 1);
    return named || lua_rawlen(L, index) == 2;
}

math::Segment Converter<math::Segment>::read(lua_State* L, int index) {
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "table with start and end");
    }
    const int table = lua_absindex(L, index);
    return {TypeConverter::pointComponent(L, table, "start", 1), TypeConverter::pointComponent(L, table, "end", 2)};
}

math::Insets Converter<math::Insets>::read(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TNUMBER) {
        return math::Insets::uniform(static_cast<float>(lua_tonumber(L, index)));
    }
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "number or table with left, top, right and bottom");
    }
    const int table = lua_absindex(L, index);
    return {TypeConverter::numberComponent(L, table, "left", 1), TypeConverter::numberComponent(L, table, "top", 2), TypeConverter::numberComponent(L, table, "right", 3), TypeConverter::numberComponent(L, table, "bottom", 4)};
}

math::EasingCurve Converter<math::EasingCurve>::read(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TSTRING) {
        return math::EasingCurve(Stack::read<math::Easing::Type>(L, index));
    }
    if (lua_isfunction(L, index)) {
        auto function = std::make_shared<Reference>(L, index);
        // clang-format off
        return math::EasingCurve::custom([function](float t) {
            lua_State* main = function->getState();
            function->push(main);
            lua_pushnumber(main, static_cast<lua_Number>(t));
            Runtime::protectedCall(main, 1, 1);
            const bool number = lua_type(main, -1) == LUA_TNUMBER;
            const auto value = static_cast<float>(lua_tonumber(main, -1));
            lua_pop(main, 1);
            if (!number) {
                throw std::runtime_error("An easing function must return a number.");
            }
            return value;
        });
        // clang-format on
    }
    if (!lua_istable(L, index)) {
        luaL_typeerror(L, index, "easing curve name, function or table");
    }

    const int table = lua_absindex(L, index);
    Table::checkFields(L, table, {TypeConverter::kEasingFields});
    if (lua_getfield(L, table, "steps") != LUA_TNIL) {
        lua_pop(L, 1);
        int count = 0;
        Table::readField(L, table, "steps", count);
        math::Easing::StepPosition position = math::Easing::StepPosition::End;
        Table::readField(L, table, "position", position);
        return math::EasingCurve::steps(count, position);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, table, "bezier") != LUA_TNIL) {
        const std::vector<float> handles = Stack::read<std::vector<float>>(L, -1);
        lua_pop(L, 1);
        if (handles.size() != 4) {
            luaL_error(L, "A bezier curve needs the four numbers x1, y1, x2 and y2.");
        }
        return math::EasingCurve::cubicBezier(handles[0], handles[1], handles[2], handles[3]);
    }
    lua_pop(L, 1);
    if (lua_getfield(L, table, "points") != LUA_TNIL) {
        luaL_checktype(L, -1, LUA_TTABLE);
        std::vector<math::Vec2> points;
        const auto count = static_cast<lua_Integer>(lua_rawlen(L, -1));
        for (lua_Integer point = 1; point <= count; ++point) {
            lua_rawgeti(L, -1, point);
            if (lua_type(L, -1) == LUA_TNUMBER) {
                const float x = count > 1 ? static_cast<float>(point - 1) / static_cast<float>(count - 1) : 0.0F;
                points.push_back({x, static_cast<float>(lua_tonumber(L, -1))});
            } else {
                points.push_back(Stack::read<math::Vec2>(L, -1));
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        return math::EasingCurve::points(std::move(points));
    }
    lua_pop(L, 1);

    math::Easing::Type curve = math::Easing::Type::Linear;
    Table::readField(L, table, "curve", curve);
    if (lua_getfield(L, table, "overshoot") != LUA_TNIL) {
        const auto overshoot = static_cast<float>(luaL_checknumber(L, -1));
        lua_pop(L, 1);
        return math::EasingCurve::back(curve, overshoot);
    }
    lua_pop(L, 1);
    float amplitude = math::Easing::kElasticAmplitude;
    float period = math::Easing::kElasticPeriod;
    lua_getfield(L, table, "amplitude");
    lua_getfield(L, table, "period");
    const bool elastic = !lua_isnil(L, -1) || !lua_isnil(L, -2);
    lua_pop(L, 2);
    if (elastic) {
        Table::readField(L, table, "amplitude", amplitude);
        Table::readField(L, table, "period", period);
        return math::EasingCurve::elastic(curve, amplitude, period);
    }
    return math::EasingCurve(curve);
}

graphics2d::DrawOrder TypeConverter::readDrawOrder(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields) {
    graphics2d::DrawOrder order;
    if (lua_isnoneornil(L, index)) {
        return order;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    checkWithExtras(L, table, kDrawOrderFields, extraFields);
    Table::readField(L, table, "layer", order.layer);
    Table::readField(L, table, "depth", order.depth);
    Table::readField(L, table, "sortOffset", order.sortOffset);
    Table::readField(L, table, "visibility", order.visibility);
    Table::readField(L, table, "blend", order.blend);
    Table::readField(L, table, "material", order.material);
    Table::readField(L, table, "normalMap", order.normalMap);
    Table::readField(L, table, "specular", order.specular);
    Table::readField(L, table, "shininess", order.shininess);
    Table::readField(L, table, "emission", order.emission);
    Table::readField(L, table, "lightMask", order.lightMask);
    Table::readField(L, table, "unshaded", order.unshaded);
    return order;
}

text::TextStyle TypeConverter::readTextStyle(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields) {
    text::TextStyle style;
    if (lua_isnoneornil(L, index)) {
        return style;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    checkWithExtras(L, table, kTextStyleFields, extraFields);
    Table::readField(L, table, "size", style.size);
    Table::readField(L, table, "color", style.color);
    Table::readField(L, table, "outlineWidth", style.outlineWidth);
    Table::readField(L, table, "outlineColor", style.outlineColor);
    Table::readField(L, table, "shadowOffset", style.shadowOffset);
    Table::readField(L, table, "shadowColor", style.shadowColor);
    Table::readField(L, table, "shadowBlur", style.shadowBlur);
    Table::readField(L, table, "align", style.align);
    Table::readField(L, table, "maxWidth", style.maxWidth);
    Table::readField(L, table, "lineSpacing", style.lineSpacing);
    Table::readField(L, table, "anchor", style.anchor);
    Table::readField(L, table, "rotation", style.rotation);
    Table::readField(L, table, "bold", style.bold);
    Table::readField(L, table, "italic", style.italic);
    Table::readField(L, table, "direction", style.direction);
    Table::readField(L, table, "language", style.language);
    return style;
}

graphics::Texture::Options TypeConverter::readTextureOptions(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields) {
    graphics::Texture::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    checkWithExtras(L, table, kTextureOptionFields, extraFields);
    Table::readField(L, table, "filter", options.filter);
    Table::readField(L, table, "wrap", options.wrap);
    return options;
}

graphics2d::SpriteInstance TypeConverter::readSpriteInstance(lua_State* L, int index, const graphics::Texture& texture, graphics2d::SpriteInstance base, std::initializer_list<Table::FieldNames> extraFields) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    checkWithExtras(L, table, kSpriteInstanceFields, extraFields);
    Table::readField(L, table, "x", base.position.x);
    Table::readField(L, table, "y", base.position.y);
    Table::readField(L, table, "width", base.size.x);
    Table::readField(L, table, "height", base.size.y);
    Table::readField(L, table, "source", base.source);
    Table::readField(L, table, "pivotX", base.pivot.x);
    Table::readField(L, table, "pivotY", base.pivot.y);
    Table::readField(L, table, "rotation", base.rotation);
    Table::readField(L, table, "color", base.color);
    Table::readField(L, table, "flash", base.flash);

    Table::readField(L, table, "flipX", base.flip.horizontal);
    Table::readField(L, table, "flipY", base.flip.vertical);

    if (base.size.isZero()) {
        base.size = base.source.isEmpty() ? texture.getSize() : base.source.getSize();
    }
    return base;
}

} // namespace haylen::lua
