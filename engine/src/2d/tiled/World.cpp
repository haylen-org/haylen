#include "haylen/2d/tiled/World.hpp"

#include <regex>

#include "2d/tiled/MapParser.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

World World::parse(const core::Json& document, std::string_view file, const std::vector<std::string>& files) {
    const std::string directory{io::Path::directory(io::Path::normalize(file))};
    World world;
    for (const core::Json& entry : document.value("maps", core::Json::array())) {
        world.maps.push_back({.path = MapParser::resolve(directory, entry.at("fileName").get<std::string>()), .bounds = {core::JsonNumber::toFloat(entry, "x", 0.0F), core::JsonNumber::toFloat(entry, "y", 0.0F), core::JsonNumber::toFloat(entry, "width", 0.0F), core::JsonNumber::toFloat(entry, "height", 0.0F)}});
    }

    for (const core::Json& pattern : document.value("patterns", core::Json::array())) {
        const std::regex expression(pattern.at("regexp").get<std::string>());
        const math::Vec2 multiplier{core::JsonNumber::toFloat(pattern, "multiplierX", 1.0F), core::JsonNumber::toFloat(pattern, "multiplierY", 1.0F)};
        const math::Vec2 offset{core::JsonNumber::toFloat(pattern, "offsetX", 0.0F), core::JsonNumber::toFloat(pattern, "offsetY", 0.0F)};
        const math::Vec2 size{core::JsonNumber::toFloat(pattern, "mapWidth", 0.0F), core::JsonNumber::toFloat(pattern, "mapHeight", 0.0F)};
        for (const std::string& candidate : files) {
            if (!directory.empty() && !candidate.starts_with(directory + "/")) {
                continue;
            }
            const std::string relative = directory.empty() ? candidate : candidate.substr(directory.size() + 1);
            std::smatch match;
            if (!std::regex_match(relative, match, expression) || match.size() < 3) {
                continue;
            }
            const math::Vec2 cell{std::stof(match[1].str()), std::stof(match[2].str())};
            world.maps.push_back({.path = candidate, .bounds = {cell.x * multiplier.x + offset.x, cell.y * multiplier.y + offset.y, size.x, size.y}});
        }
    }
    return world;
}

} // namespace haylen::tiled
