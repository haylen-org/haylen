#include "haylen/2d/tiled/MapWriter.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "2d/tiled/MapParser.hpp"
#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Tileset.hpp"
#include "haylen/io/Path.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::tiled {

std::vector<std::string_view> MapWriter::split(std::string_view path) {
    std::vector<std::string_view> parts;
    std::size_t begin = 0;
    while (begin < path.size()) {
        const std::size_t end = std::min(path.find('/', begin), path.size());
        parts.push_back(path.substr(begin, end - begin));
        begin = end + 1;
    }
    return parts;
}

core::Json MapWriter::writePoints(const std::vector<math::Vec2>& points) {
    core::Json list = core::Json::array();
    for (const math::Vec2 point : points) {
        list.push_back({{"x", point.x}, {"y", point.y}});
    }
    return list;
}

std::string MapWriter::relative(std::string_view directory, std::string_view path) {
    if (path.empty()) {
        return {};
    }
    const std::vector<std::string_view> from = split(directory);
    const std::vector<std::string_view> to = split(path);
    std::size_t shared = 0;
    while (shared < from.size() && shared + 1 < to.size() && from[shared] == to[shared]) {
        ++shared;
    }
    std::string result;
    for (std::size_t index = shared; index < from.size(); ++index) {
        result += "../";
    }
    for (std::size_t index = shared; index < to.size(); ++index) {
        result += std::string(to[index]) + (index + 1 < to.size() ? "/" : "");
    }
    return result;
}

// File values go back to paths relative to the map, as the loader resolved them, including the file items of lists.
// Makes file values relative to the folder of the map again, including the file items of lists and of lists nested in them.
core::Json MapWriter::writeValue(std::string_view type, core::Json value, std::string_view directory) {
    if (type == "file" && value.is_string()) {
        return relative(directory, value.get<std::string>());
    }
    if (type == "list" && value.is_array()) {
        for (core::Json& item : value) {
            item["value"] = writeValue(item.value("type", std::string("string")), item.value("value", core::Json()), directory);
        }
    }
    return value;
}

core::Json MapWriter::writeProperties(const Properties& properties, std::string_view directory) {
    core::Json list = core::Json::array();
    for (const Property& property : properties.getItems()) {
        core::Json entry{{"name", property.name}, {"type", property.type}, {"value", writeValue(property.type, property.value, directory)}};
        if (!property.propertyType.empty()) {
            entry["propertytype"] = property.propertyType;
        }
        list.push_back(std::move(entry));
    }
    return list;
}

core::Json MapWriter::writeObject(const Object& object, std::string_view directory) {
    core::Json entry{
        {"id", object.id}, {"name", object.name}, {"type", object.type}, {"x", object.position.x}, {"y", object.position.y}, {"width", object.size.x}, {"height", object.size.y}, {"rotation", object.rotation * 180.0F / math::Math::kPi}, {"visible", object.visible},
    };
    if (object.opacity != 1.0F) {
        entry["opacity"] = object.opacity;
    }
    if (object.gid != 0) {
        entry["gid"] = object.gid;
    }
    switch (object.shape) {
    case Object::Shape::Ellipse:
        entry["ellipse"] = true;
        break;
    case Object::Shape::Capsule:
        entry["capsule"] = true;
        break;
    case Object::Shape::Point:
        entry["point"] = true;
        break;
    case Object::Shape::Polygon:
        entry["polygon"] = writePoints(object.points);
        break;
    case Object::Shape::Polyline:
        entry["polyline"] = writePoints(object.points);
        break;
    case Object::Shape::Text:
        entry["text"] = {{"text", object.text.text}, {"fontfamily", object.text.fontFamily}, {"pixelsize", object.text.pixelSize}, {"wrap", object.text.wrap}, {"color", object.text.color.toHex()}, {"bold", object.text.bold}, {"italic", object.text.italic}, {"underline", object.text.underline}, {"strikeout", object.text.strikeout}, {"kerning", object.text.kerning}, {"halign", object.text.horizontalAlign}, {"valign", object.text.verticalAlign}};
        break;
    case Object::Shape::Rectangle:
    case Object::Shape::Tile:
        break;
    }
    if (!object.properties.getItems().empty()) {
        entry["properties"] = writeProperties(object.properties, directory);
    }
    return entry;
}

core::Json MapWriter::writeTileset(const Tileset& tileset, std::uint32_t firstGid, std::string_view directory) {
    core::Json entry{
        {"firstgid", firstGid}, {"name", tileset.name}, {"class", tileset.type}, {"tilewidth", tileset.tileSize.x}, {"tileheight", tileset.tileSize.y}, {"tilecount", tileset.tileCount}, {"columns", tileset.columns}, {"margin", tileset.margin}, {"spacing", tileset.spacing}, {"objectalignment", tileset.objectAlignment}, {"tilerendersize", tileset.renderGridSize ? "grid" : "tile"}, {"fillmode", tileset.preserveAspect ? "preserve-aspect-fit" : "stretch"},
    };
    if (!tileset.image.empty()) {
        entry["image"] = relative(directory, tileset.image);
        entry["imagewidth"] = tileset.imageSize.x;
        entry["imageheight"] = tileset.imageSize.y;
    }
    if (tileset.transparentColor) {
        entry["transparentcolor"] = tileset.transparentColor->toHex();
    }
    if (!tileset.tileOffset.isZero()) {
        entry["tileoffset"] = {{"x", tileset.tileOffset.x}, {"y", tileset.tileOffset.y}};
    }
    if (tileset.grid.isometric || tileset.grid.size != tileset.tileSize) {
        entry["grid"] = {{"orientation", tileset.grid.isometric ? "isometric" : "orthogonal"}, {"width", tileset.grid.size.x}, {"height", tileset.grid.size.y}};
    }
    const Tileset::Transformations& turns = tileset.transformations;
    if (turns.flipHorizontal || turns.flipVertical || turns.rotate || turns.preferUntransformed) {
        entry["transformations"] = {{"hflip", turns.flipHorizontal}, {"vflip", turns.flipVertical}, {"rotate", turns.rotate}, {"preferuntransformed", turns.preferUntransformed}};
    }
    if (!tileset.properties.getItems().empty()) {
        entry["properties"] = writeProperties(tileset.properties, directory);
    }

    core::Json tiles = core::Json::array();
    for (const auto& [id, tile] : tileset.tiles) {
        core::Json described{{"id", id}, {"type", tile.type}, {"probability", tile.probability}};
        if (!tile.properties.getItems().empty()) {
            described["properties"] = writeProperties(tile.properties, directory);
        }
        if (!tile.animation.empty()) {
            core::Json frames = core::Json::array();
            for (const Tile::AnimationFrame& frame : tile.animation) {
                frames.push_back({{"tileid", frame.tileId}, {"duration", std::lround(frame.duration * 1000.0F)}});
            }
            described["animation"] = std::move(frames);
        }
        if (!tile.collision.empty()) {
            core::Json objects = core::Json::array();
            for (const Object& object : tile.collision) {
                objects.push_back(writeObject(object, directory));
            }
            described["objectgroup"] = {{"type", "objectgroup"}, {"objects", std::move(objects)}};
        }
        if (!tile.image.empty()) {
            described["image"] = relative(directory, tile.image);
            described["x"] = tile.source.x;
            described["y"] = tile.source.y;
            described["width"] = tile.source.width;
            described["height"] = tile.source.height;
            described["imagewidth"] = tile.texture.isValid() ? tile.texture.getSize().x : tile.source.getRight();
            described["imageheight"] = tile.texture.isValid() ? tile.texture.getSize().y : tile.source.getBottom();
        }
        tiles.push_back(std::move(described));
    }
    if (!tiles.empty()) {
        entry["tiles"] = std::move(tiles);
    }

    core::Json wangSets = core::Json::array();
    for (const WangSet& set : tileset.wangSets) {
        core::Json colors = core::Json::array();
        for (const WangSet::Color& color : set.colors) {
            colors.push_back({{"name", color.name}, {"class", color.type}, {"color", color.color.toHex()}, {"tile", color.tile}, {"probability", color.probability}, {"properties", writeProperties(color.properties, directory)}});
        }
        core::Json wangTiles = core::Json::array();
        for (const WangSet::Tile& tile : set.tiles) {
            wangTiles.push_back({{"tileid", tile.tileId}, {"wangid", tile.wangId}});
        }
        wangSets.push_back({{"name", set.name}, {"class", set.type}, {"type", set.kind}, {"tile", set.tile}, {"colors", std::move(colors)}, {"wangtiles", std::move(wangTiles)}, {"properties", writeProperties(set.properties, directory)}});
    }
    if (!wangSets.empty()) {
        entry["wangsets"] = std::move(wangSets);
    }
    return entry;
}

core::Json MapWriter::writeLayer(const Layer& layer, std::string_view directory) {
    static constexpr std::array<std::string_view, 4> kKinds{"tilelayer", "objectgroup", "imagelayer", "group"};
    core::Json entry{
        {"id", layer.id}, {"name", layer.name}, {"type", kKinds[static_cast<std::size_t>(layer.kind)]}, {"class", layer.type}, {"visible", layer.visible}, {"opacity", layer.opacity}, {"offsetx", layer.offset.x}, {"offsety", layer.offset.y}, {"parallaxx", layer.parallax.x}, {"parallaxy", layer.parallax.y},
    };
    if (layer.kind != Layer::Kind::Group) {
        entry["mode"] = MapParser::blendModeName(layer.blend);
    }
    if (layer.tint != math::Color::white()) {
        entry["tintcolor"] = layer.tint.toHex();
    }
    if (!layer.properties.getItems().empty()) {
        entry["properties"] = writeProperties(layer.properties, directory);
    }

    switch (layer.kind) {
    case Layer::Kind::Tile:
        entry["width"] = layer.width;
        entry["height"] = layer.height;
        if (!layer.infinite) {
            entry["data"] = layer.gids;
        } else {
            core::Json chunks = core::Json::array();
            for (const Layer::Chunk& chunk : layer.chunks) {
                chunks.push_back({{"x", chunk.x}, {"y", chunk.y}, {"width", chunk.width}, {"height", chunk.height}, {"data", chunk.gids}});
            }
            entry["chunks"] = std::move(chunks);
        }
        break;
    case Layer::Kind::Object: {
        core::Json objects = core::Json::array();
        for (const Object& object : layer.objects) {
            objects.push_back(writeObject(object, directory));
        }
        entry["draworder"] = layer.indexDrawOrder ? "index" : "topdown";
        if (layer.color) {
            entry["color"] = layer.color->toHex();
        }
        entry["objects"] = std::move(objects);
        break;
    }
    case Layer::Kind::Image:
        entry["image"] = relative(directory, layer.image);
        entry["imagewidth"] = layer.imageSize.x;
        entry["imageheight"] = layer.imageSize.y;
        entry["repeatx"] = layer.repeatX;
        entry["repeaty"] = layer.repeatY;
        if (layer.transparentColor) {
            entry["transparentcolor"] = layer.transparentColor->toHex();
        }
        break;
    case Layer::Kind::Group: {
        core::Json children = core::Json::array();
        for (const Layer& child : layer.layers) {
            children.push_back(writeLayer(child, directory));
        }
        entry["layers"] = std::move(children);
        break;
    }
    }
    return entry;
}

void MapWriter::collectIds(const Layer& layer, std::uint32_t& layerId, std::uint32_t& objectId) noexcept {
    layerId = std::max(layerId, layer.id);
    for (const Object& object : layer.objects) {
        objectId = std::max(objectId, object.id);
    }
    for (const Layer& child : layer.layers) {
        collectIds(child, layerId, objectId);
    }
}

core::Json MapWriter::write(const Map& map, std::string_view file) {
    const std::string directory(io::Path::directory(file));
    core::Json document{
        {"type", "map"}, {"version", "1.10"}, {"class", map.type}, {"orientation", Map::orientationName(map.orientation)}, {"renderorder", Map::renderOrderName(map.renderOrder)}, {"width", map.width}, {"height", map.height}, {"tilewidth", map.tileSize.x}, {"tileheight", map.tileSize.y}, {"infinite", map.infinite}, {"parallaxoriginx", map.parallaxOrigin.x}, {"parallaxoriginy", map.parallaxOrigin.y},
    };
    if (map.orientation == Map::Orientation::Staggered || map.orientation == Map::Orientation::Hexagonal) {
        document["staggeraxis"] = map.staggerX ? "x" : "y";
        document["staggerindex"] = map.staggerEven ? "even" : "odd";
    }
    if (map.orientation == Map::Orientation::Hexagonal) {
        document["hexsidelength"] = map.hexSideLength;
    }
    if (!map.skew.isZero()) {
        document["skewx"] = map.skew.x;
        document["skewy"] = map.skew.y;
    }
    if (map.backgroundColor) {
        document["backgroundcolor"] = map.backgroundColor->toHex();
    }
    if (!map.properties.getItems().empty()) {
        document["properties"] = writeProperties(map.properties, directory);
    }

    core::Json tilesets = core::Json::array();
    for (const Map::TilesetReference& reference : map.tilesets) {
        tilesets.push_back(writeTileset(*reference.tileset, reference.firstGid, directory));
    }
    document["tilesets"] = std::move(tilesets);

    core::Json layers = core::Json::array();
    std::uint32_t layerId = 0;
    std::uint32_t objectId = 0;
    for (const Layer& layer : map.layers) {
        layers.push_back(writeLayer(layer, directory));
        collectIds(layer, layerId, objectId);
    }
    document["layers"] = std::move(layers);
    document["nextlayerid"] = layerId + 1;
    document["nextobjectid"] = objectId + 1;
    return document;
}

} // namespace haylen::tiled
