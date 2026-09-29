#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// An object of an object layer or of a tile collision group, with its template already merged in.
struct Object {
    enum class Shape : std::uint8_t {
        Rectangle,
        Ellipse,
        Capsule,
        Point,
        Polygon,
        Polyline,
        Text,
        Tile,
    };

    struct Text {
        std::string text;
        std::string fontFamily;
        float pixelSize = 16.0F;
        bool wrap = false;
        math::Color color = math::Color::black();
        bool bold = false;
        bool italic = false;
        std::string horizontalAlign = "left";
        std::string verticalAlign = "top";
    };

    std::uint32_t id = 0;
    std::string name;
    std::string type;
    math::Vec2 position{};
    math::Vec2 size{};
    float rotation = 0.0F;
    bool visible = true;
    float opacity = 1.0F;
    Shape shape = Shape::Rectangle;
    std::vector<math::Vec2> points;
    std::uint32_t gid = 0;
    Text text;
    Properties properties;
    std::string templatePath;

    [[nodiscard]] static std::string_view shapeName(Shape value) noexcept;
};

} // namespace haylen::tiled
