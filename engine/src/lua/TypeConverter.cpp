#include "haylen/lua/TypeConverter.hpp"

#include <memory>
#include <stdexcept>
#include <vector>

#include "haylen/lua/Reference.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"

namespace haylen::lua {

const TypeConverter::NameTable<graphics2d::Renderer::SortMode, 3> TypeConverter::kSortModes = {{{"layer", graphics2d::Renderer::SortMode::Layer}, {"depth", graphics2d::Renderer::SortMode::Depth}, {"y", graphics2d::Renderer::SortMode::Y}}};
const TypeConverter::NameTable<graphics2d::ImageBlend::Pattern, 5> TypeConverter::kBlendPatterns = {{{"dissolve", graphics2d::ImageBlend::Pattern::Dissolve}, {"pixelate", graphics2d::ImageBlend::Pattern::Pixelate}, {"radial", graphics2d::ImageBlend::Pattern::Radial}, {"iris", graphics2d::ImageBlend::Pattern::Iris}, {"pageTurn", graphics2d::ImageBlend::Pattern::PageTurn}}};
const TypeConverter::NameTable<graphics2d::SceneTransition::Kind, 24> TypeConverter::kTransitions = {{
    {"fade", graphics2d::SceneTransition::Kind::Fade}, {"crossFade", graphics2d::SceneTransition::Kind::CrossFade}, {"moveIn", graphics2d::SceneTransition::Kind::MoveIn}, {"slideIn", graphics2d::SceneTransition::Kind::SlideIn}, {"push", graphics2d::SceneTransition::Kind::Push}, {"shrinkGrow", graphics2d::SceneTransition::Kind::ShrinkGrow}, {"flipX", graphics2d::SceneTransition::Kind::FlipX}, {"flipY", graphics2d::SceneTransition::Kind::FlipY}, {"zoomFlip", graphics2d::SceneTransition::Kind::ZoomFlip}, {"rotoZoom", graphics2d::SceneTransition::Kind::RotoZoom}, {"jumpZoom", graphics2d::SceneTransition::Kind::JumpZoom}, {"splitColumns", graphics2d::SceneTransition::Kind::SplitColumns}, {"splitRows", graphics2d::SceneTransition::Kind::SplitRows}, {"turnOffTiles", graphics2d::SceneTransition::Kind::TurnOffTiles}, {"fadeTiles", graphics2d::SceneTransition::Kind::FadeTiles}, {"pageTurn", graphics2d::SceneTransition::Kind::PageTurn}, {"radialClockwise", graphics2d::SceneTransition::Kind::RadialClockwise}, {"radialCounterclockwise", graphics2d::SceneTransition::Kind::RadialCounterclockwise}, {"wipe", graphics2d::SceneTransition::Kind::Wipe}, {"inOut", graphics2d::SceneTransition::Kind::InOut}, {"outIn", graphics2d::SceneTransition::Kind::OutIn}, {"iris", graphics2d::SceneTransition::Kind::Iris}, {"dissolve", graphics2d::SceneTransition::Kind::Dissolve}, {"pixelate", graphics2d::SceneTransition::Kind::Pixelate},
}};
const TypeConverter::NameTable<graphics2d::SceneTransition::Direction, 8> TypeConverter::kDirections = {{{"left", graphics2d::SceneTransition::Direction::Left}, {"right", graphics2d::SceneTransition::Direction::Right}, {"up", graphics2d::SceneTransition::Direction::Up}, {"down", graphics2d::SceneTransition::Direction::Down}, {"upLeft", graphics2d::SceneTransition::Direction::UpLeft}, {"upRight", graphics2d::SceneTransition::Direction::UpRight}, {"downLeft", graphics2d::SceneTransition::Direction::DownLeft}, {"downRight", graphics2d::SceneTransition::Direction::DownRight}}};
const TypeConverter::NameTable<input::InputDevice, 3> TypeConverter::kDevices = {{{"keyboardMouse", input::InputDevice::KeyboardMouse}, {"touch", input::InputDevice::Touch}, {"gamepad", input::InputDevice::Gamepad}}};
const TypeConverter::NameTable<input::TouchPhase, 5> TypeConverter::kPhases = {{{"began", input::TouchPhase::Began}, {"moved", input::TouchPhase::Moved}, {"stationary", input::TouchPhase::Stationary}, {"ended", input::TouchPhase::Ended}, {"cancelled", input::TouchPhase::Cancelled}}};
const TypeConverter::NameTable<platform::Window::Cursor, 11> TypeConverter::kCursors = {{
    {"default", platform::Window::Cursor::Default},
    {"arrow", platform::Window::Cursor::Arrow},
    {"iBeam", platform::Window::Cursor::IBeam},
    {"crosshair", platform::Window::Cursor::Crosshair},
    {"pointingHand", platform::Window::Cursor::PointingHand},
    {"resizeHorizontal", platform::Window::Cursor::ResizeHorizontal},
    {"resizeVertical", platform::Window::Cursor::ResizeVertical},
    {"resizeDiagonalDown", platform::Window::Cursor::ResizeDiagonalDown},
    {"resizeDiagonalUp", platform::Window::Cursor::ResizeDiagonalUp},
    {"resizeAll", platform::Window::Cursor::ResizeAll},
    {"notAllowed", platform::Window::Cursor::NotAllowed},
}};

const TypeConverter::NameTable<platform::Window::Passthrough, 3> TypeConverter::kPassthroughs = {{{"off", platform::Window::Passthrough::Off}, {"whole", platform::Window::Passthrough::Whole}, {"regions", platform::Window::Passthrough::Regions}}};

const TypeConverter::NameTable<platform::Event::Type, 28> TypeConverter::kEvents = {{
    {"keyDown", platform::Event::Type::KeyDown}, {"keyUp", platform::Event::Type::KeyUp}, {"character", platform::Event::Type::Character}, {"mouseDown", platform::Event::Type::MouseDown}, {"mouseUp", platform::Event::Type::MouseUp}, {"mouseMove", platform::Event::Type::MouseMove}, {"mouseScroll", platform::Event::Type::MouseScroll}, {"mouseEnter", platform::Event::Type::MouseEnter}, {"mouseLeave", platform::Event::Type::MouseLeave}, {"touchBegan", platform::Event::Type::TouchBegan}, {"touchMoved", platform::Event::Type::TouchMoved}, {"touchEnded", platform::Event::Type::TouchEnded}, {"touchCancelled", platform::Event::Type::TouchCancelled}, {"resized", platform::Event::Type::Resized}, {"suspended", platform::Event::Type::Suspended}, {"resumed", platform::Event::Type::Resumed}, {"focusGained", platform::Event::Type::FocusGained}, {"focusLost", platform::Event::Type::FocusLost}, {"quitRequested", platform::Event::Type::QuitRequested}, {"lowMemory", platform::Event::Type::LowMemory}, {"textEdited", platform::Event::Type::TextEdited}, {"textAction", platform::Event::Type::TextAction}, {"keyboardChanged", platform::Event::Type::KeyboardChanged}, {"networkChanged", platform::Event::Type::NetworkChanged}, {"interruptionBegan", platform::Event::Type::InterruptionBegan}, {"interruptionEnded", platform::Event::Type::InterruptionEnded}, {"windowMoved", platform::Event::Type::WindowMoved}, {"monitorsChanged", platform::Event::Type::MonitorsChanged},
}};

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
        luaL_error(L, "Expected a number in field \"%s\".", name);
    }
    const auto value = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return value;
}

math::Vec2 TypeConverter::pointComponent(lua_State* L, int table, const char* name, lua_Integer position) {
    pushComponent(L, table, name, position);
    if (!Stack::is<math::Vec2>(L, -1)) {
        luaL_error(L, "Expected a point in field \"%s\".", name);
    }
    const math::Vec2 value = Stack::read<math::Vec2>(L, -1);
    lua_pop(L, 1);
    return value;
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

void Converter<math::Insets>::push(lua_State* L, const math::Insets& value) {
    lua_createtable(L, 0, 4);
    lua_pushnumber(L, static_cast<lua_Number>(value.left));
    lua_setfield(L, -2, "left");
    lua_pushnumber(L, static_cast<lua_Number>(value.top));
    lua_setfield(L, -2, "top");
    lua_pushnumber(L, static_cast<lua_Number>(value.right));
    lua_setfield(L, -2, "right");
    lua_pushnumber(L, static_cast<lua_Number>(value.bottom));
    lua_setfield(L, -2, "bottom");
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

void Converter<graphics2d::PartColors>::push(lua_State* L, const graphics2d::PartColors& value) {
    lua_createtable(L, 0, 4);
    Stack::push(L, value.red);
    lua_setfield(L, -2, "red");
    Stack::push(L, value.green);
    lua_setfield(L, -2, "green");
    Stack::push(L, value.blue);
    lua_setfield(L, -2, "blue");
    Stack::push(L, value.yellow);
    lua_setfield(L, -2, "yellow");
}

graphics2d::PartColors Converter<graphics2d::PartColors>::read(lua_State* L, int index) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int table = lua_absindex(L, index);
    Table::checkFields(L, table, {TypeConverter::kPartColorFields});
    graphics2d::PartColors colors;
    Table::readField(L, table, "red", colors.red);
    Table::readField(L, table, "green", colors.green);
    Table::readField(L, table, "blue", colors.blue);
    Table::readField(L, table, "yellow", colors.yellow);
    return colors;
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
    if (lua_getfield(L, table, "cubicBezier") != LUA_TNIL) {
        const std::vector<float> handles = Stack::read<std::vector<float>>(L, -1);
        lua_pop(L, 1);
        if (handles.size() != 4) {
            luaL_error(L, "A \"cubicBezier\" curve needs the four numbers \"x1\", \"y1\", \"x2\" and \"y2\".");
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
    // clang-format off
    Table::readFields(L, index, kDrawOrderFields, extraFields, [L, &order](std::string_view key) {
        if (key == "layer") {
            Table::readValue(L, key, order.layer);
        } else if (key == "depth") {
            Table::readValue(L, key, order.depth);
        } else if (key == "sortOffset") {
            Table::readValue(L, key, order.sortOffset);
        } else if (key == "visibility") {
            Table::readValue(L, key, order.visibility);
        } else if (key == "blend") {
            Table::readValue(L, key, order.blend);
        } else if (key == "material") {
            Table::readValue(L, key, order.material);
        } else if (key == "partMask") {
            Table::readValue(L, key, order.partMask);
        } else if (key == "normalMap") {
            Table::readValue(L, key, order.normalMap);
        } else if (key == "specular") {
            Table::readValue(L, key, order.specular);
        } else if (key == "shininess") {
            Table::readValue(L, key, order.shininess);
        } else if (key == "emission") {
            Table::readValue(L, key, order.emission);
        } else if (key == "lightMask") {
            Table::readValue(L, key, order.lightMask);
        } else if (key == "unshaded") {
            Table::readValue(L, key, order.unshaded);
        }
    });
    // clang-format on
    return order;
}

text::Style TypeConverter::readTextStyle(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields) {
    text::Style style;
    if (lua_isnoneornil(L, index)) {
        return style;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    // clang-format off
    Table::readFields(L, index, kTextStyleFields, extraFields, [L, &style](std::string_view key) {
        if (key == "size") {
            Table::readValue(L, key, style.size);
        } else if (key == "color") {
            Table::readValue(L, key, style.color);
        } else if (key == "outlineWidth") {
            Table::readValue(L, key, style.outlineWidth);
        } else if (key == "outlineColor") {
            Table::readValue(L, key, style.outlineColor);
        } else if (key == "shadowOffset") {
            Table::readValue(L, key, style.shadowOffset);
        } else if (key == "shadowColor") {
            Table::readValue(L, key, style.shadowColor);
        } else if (key == "shadowBlur") {
            Table::readValue(L, key, style.shadowBlur);
        } else if (key == "align") {
            Table::readValue(L, key, style.align);
        } else if (key == "maxWidth") {
            Table::readValue(L, key, style.maxWidth);
        } else if (key == "lineSpacing") {
            Table::readValue(L, key, style.lineSpacing);
        } else if (key == "anchor") {
            Table::readValue(L, key, style.anchor);
        } else if (key == "rotation") {
            Table::readValue(L, key, style.rotation);
        } else if (key == "scale") {
            Table::readValue(L, key, style.scale);
        } else if (key == "bold") {
            Table::readValue(L, key, style.bold);
        } else if (key == "italic") {
            Table::readValue(L, key, style.italic);
        } else if (key == "direction") {
            Table::readValue(L, key, style.direction);
        } else if (key == "language") {
            Table::readValue(L, key, style.language);
        } else if (key == "pixelSnap") {
            Table::readValue(L, key, style.pixelSnap);
        }
    });
    // clang-format on
    return style;
}

graphics::Texture::Options TypeConverter::readTextureOptions(lua_State* L, int index, std::initializer_list<Table::FieldNames> extraFields) {
    graphics::Texture::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    // clang-format off
    Table::readFields(L, index, kTextureOptionFields, extraFields, [L, &options](std::string_view key) {
        if (key == "filter") {
            Table::readValue(L, key, options.filter);
        } else if (key == "wrap") {
            Table::readValue(L, key, options.wrap);
        }
    });
    // clang-format on
    return options;
}

graphics2d::SpriteInstance TypeConverter::readSpriteInstance(lua_State* L, int index, const graphics::Texture& texture, graphics2d::SpriteInstance base, std::initializer_list<Table::FieldNames> extraFields) {
    luaL_checktype(L, index, LUA_TTABLE);
    // clang-format off
    Table::readFields(L, index, kSpriteInstanceFields, extraFields, [L, &base](std::string_view key) {
        if (key == "x") {
            Table::readValue(L, key, base.position.x);
        } else if (key == "y") {
            Table::readValue(L, key, base.position.y);
        } else if (key == "width") {
            Table::readValue(L, key, base.size.x);
        } else if (key == "height") {
            Table::readValue(L, key, base.size.y);
        } else if (key == "source") {
            Table::readValue(L, key, base.source);
        } else if (key == "pivotX") {
            Table::readValue(L, key, base.pivot.x);
        } else if (key == "pivotY") {
            Table::readValue(L, key, base.pivot.y);
        } else if (key == "rotation") {
            Table::readValue(L, key, base.rotation);
        } else if (key == "color") {
            Table::readValue(L, key, base.color);
        } else if (key == "flash") {
            Table::readValue(L, key, base.flash);
        } else if (key == "flipHorizontal") {
            Table::readValue(L, key, base.flip.horizontal);
        } else if (key == "flipVertical") {
            Table::readValue(L, key, base.flip.vertical);
        } else if (key == "flipDiagonal") {
            Table::readValue(L, key, base.flip.diagonal);
        }
    });
    // clang-format on

    if (base.size.isZero()) {
        base.size = base.source.isEmpty() ? texture.getSize() : base.source.getSize();
    }
    return base;
}

} // namespace haylen::lua
