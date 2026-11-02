#pragma once

#include <lua.hpp>

#include <array>
#include <optional>
#include <string_view>

#include "haylen/2d/procedural/Placement.hpp"
#include "haylen/lua/EnumNames.hpp"

namespace haylen::lua {

template <> struct EnumNames<procedural2d::Placement::Shape::Kind> {
    static std::optional<procedural2d::Placement::Shape::Kind> fromName(std::string_view name) {
        return procedural2d::Placement::shapeKindFromName(name);
    }
    static std::string_view name(procedural2d::Placement::Shape::Kind value) {
        return procedural2d::Placement::shapeKindName(value);
    }
};

template <> struct EnumNames<procedural2d::Placement::Order> {
    static std::optional<procedural2d::Placement::Order> fromName(std::string_view name) {
        return procedural2d::Placement::orderFromName(name);
    }
    static std::string_view name(procedural2d::Placement::Order value) {
        return procedural2d::Placement::orderName(value);
    }
};

} // namespace haylen::lua

namespace haylen::procedural2d {

// Adds `place` and `placeAsync` to `haylen.procedural2d`, which place counts of entities of several kinds by the shapes they occupy, with the kinds counted from 1.
class PlacementLua final {
  public:
    // Sets the functions on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 7> kFields{"area", "kinds", "forbidden", "order", "attempts", "seed", "random"};
    static constexpr std::array<std::string_view, 5> kKindFields{"count", "shape", "spacing", "margin", "rotate"};
    static constexpr std::array<std::string_view, 7> kShapeFields{"kind", "center", "radius", "width", "height", "points", "rotation"};

    [[nodiscard]] static Placement::Options readOptions(lua_State* L, int index);
    [[nodiscard]] static Placement::Shape readShape(lua_State* L, int index);

    // Pushes `{entities = {{kind, x, y, rotation}, ...}, missing = {count of each kind}}`.
    static void pushResult(lua_State* L, const Placement::Result& result);

    static int place(lua_State* L);
    static int placeAsync(lua_State* L);
};

} // namespace haylen::procedural2d
