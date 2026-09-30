#include "2d/tiled/MapParser.hpp"

#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <utility>

#include "haylen/io/Path.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::tiled {

Map MapParser::parse(const core::Json& document, std::string_view file, const Map::JsonReader& reader) {
    Map result;
    result.path = io::Path::normalize(file);
    const std::string directory{io::Path::directory(result.path)};

    result.type = document.value("class", std::string{});
    result.orientation = readOrientation(document.value("orientation", std::string("orthogonal")));
    result.renderOrder = readRenderOrder(document.value("renderorder", std::string("right-down")));
    result.width = document.at("width").get<int>();
    result.height = document.at("height").get<int>();
    result.tileSize = {document.at("tilewidth").get<float>(), document.at("tileheight").get<float>()};
    if (result.width < 0 || result.height < 0) {
        throw std::invalid_argument("The Tiled map \"" + result.path + "\" has a negative size.");
    }
    if (!(result.tileSize.x > 0.0F && result.tileSize.y > 0.0F && std::isfinite(result.tileSize.x) && std::isfinite(result.tileSize.y))) {
        throw std::invalid_argument("The Tiled map \"" + result.path + "\" needs a positive tile size.");
    }
    result.infinite = document.value("infinite", false);
    result.hexSideLength = document.value("hexsidelength", 0);
    result.staggerX = document.value("staggeraxis", std::string("y")) == "x";
    result.staggerEven = document.value("staggerindex", std::string("odd")) == "even";
    result.skew = {document.value("skewx", 0.0F), document.value("skewy", 0.0F)};
    if (result.orientation == Map::Orientation::Oblique && result.skew.x * result.skew.y == result.tileSize.x * result.tileSize.y) {
        throw std::invalid_argument("The skew of the oblique map \"" + result.path + "\" folds its grid onto a line.");
    }
    result.parallaxOrigin = {document.value("parallaxoriginx", 0.0F), document.value("parallaxoriginy", 0.0F)};
    result.backgroundColor = readOptionalColor(document, "backgroundcolor");
    result.properties = readProperties(document, directory);

    const MapParser parser(reader, result);
    for (const core::Json& entry : document.value("tilesets", core::Json::array())) {
        const auto firstGid = entry.at("firstgid").get<std::uint32_t>();
        if (!entry.contains("source")) {
            result.tilesets.push_back({firstGid, parser.readTileset(entry, directory, {})});
            continue;
        }
        const std::string source = resolve(directory, entry.at("source").get<std::string>());
        result.tilesets.push_back({firstGid, parser.readTileset(reader(source), io::Path::directory(source), source)});
    }
    std::sort(result.tilesets.begin(), result.tilesets.end(), [](const Map::TilesetReference& lhs, const Map::TilesetReference& rhs) { return lhs.firstGid < rhs.firstGid; });

    for (const core::Json& entry : document.value("layers", core::Json::array())) {
        result.layers.push_back(parser.readLayer(entry, directory));
    }
    return result;
}

std::string MapParser::resolve(std::string_view directory, std::string_view relative) {
    return relative.empty() ? std::string{} : io::Path::join(directory, relative);
}

std::vector<std::uint8_t> MapParser::decodeBase64(std::string_view text) {
    std::array<int, 256> table{};
    table.fill(-1);
    const std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (std::size_t index = 0; index < alphabet.size(); ++index) {
        table[static_cast<unsigned char>(alphabet[index])] = static_cast<int>(index);
    }

    std::vector<std::uint8_t> bytes;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (const char character : text) {
        if (character == '=' || std::isspace(static_cast<unsigned char>(character)) != 0) {
            continue;
        }
        const int value = table[static_cast<unsigned char>(character)];
        if (value < 0) {
            throw std::invalid_argument("Tile layer data is not valid base64.");
        }
        buffer = (buffer << 6U) | static_cast<std::uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((buffer >> static_cast<unsigned>(bits)) & 0xFFU));
        }
    }
    return bytes;
}

std::vector<std::uint8_t> MapParser::inflate(const std::vector<std::uint8_t>& compressed, bool gzip, std::size_t expected) {
    std::vector<std::uint8_t> output(expected);
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(compressed.data());
    stream.avail_in = static_cast<uInt>(compressed.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit2(&stream, gzip ? 16 + MAX_WBITS : MAX_WBITS) != Z_OK) {
        throw std::runtime_error("Tile layer data could not be decompressed.");
    }
    const int status = ::inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (status != Z_STREAM_END || stream.total_out != expected) {
        throw std::runtime_error("Tile layer data could not be decompressed.");
    }
    return output;
}

std::size_t MapParser::countCells(int width, int height, std::string_view layer) {
    if (width < 0 || height < 0) {
        throw std::invalid_argument("The Tiled tile layer \"" + std::string(layer) + "\" has a negative size.");
    }
    const auto columns = static_cast<std::size_t>(width);
    const auto rows = static_cast<std::size_t>(height);
    if (columns != 0 && rows > kMaxCells / columns) {
        throw std::invalid_argument("The Tiled tile layer \"" + std::string(layer) + "\" has more cells than a layer can hold.");
    }
    return columns * rows;
}

std::vector<std::uint32_t> MapParser::readTileData(const core::Json& layer, const core::Json& data, std::size_t cells) {
    if (data.is_array()) {
        std::vector<std::uint32_t> gids = data.get<std::vector<std::uint32_t>>();
        if (gids.size() != cells) {
            throw std::invalid_argument("Tile layer data does not match the layer size.");
        }
        return gids;
    }

    std::vector<std::uint8_t> bytes = decodeBase64(data.get<std::string>());
    const std::string compression = layer.value("compression", std::string{});
    if (compression == "zlib" || compression == "gzip") {
        bytes = inflate(bytes, compression == "gzip", cells * 4);
    } else if (compression == "zstd") {
        std::vector<std::uint8_t> output(cells * 4);
        const std::size_t size = ZSTD_decompress(output.data(), output.size(), bytes.data(), bytes.size());
        if (ZSTD_isError(size) != 0U || size != output.size()) {
            throw std::runtime_error("Tile layer data could not be decompressed.");
        }
        bytes = std::move(output);
    } else if (!compression.empty()) {
        throw std::invalid_argument("The compression of a tile layer must be \"zlib\", \"gzip\" or \"zstd\", not \"" + compression + "\".");
    }

    if (bytes.size() != cells * 4) {
        throw std::invalid_argument("Tile layer data does not match the layer size.");
    }
    std::vector<std::uint32_t> gids(cells);
    for (std::size_t index = 0; index < cells; ++index) {
        const std::size_t offset = index * 4;
        gids[index] = static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) | (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) | (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    }
    return gids;
}

std::optional<math::Color> MapParser::readOptionalColor(const core::Json& object, const char* key) {
    if (!object.contains(key)) {
        return std::nullopt;
    }
    const std::string text = object.at(key).get<std::string>();
    const std::optional<math::Color> color = math::Color::parse(text);
    if (!color) {
        throw std::invalid_argument("The Tiled color \"" + text + "\" is not a \"#RRGGBB\" or \"#AARRGGBB\" color.");
    }
    return color;
}

// Tiled offers every Qt composition mode, and the ones fixed-function GPU blending cannot reproduce are rejected.
graphics::BlendMode::Type MapParser::readBlendMode(const std::string& mode, std::string_view layer) {
    if (mode == "normal") {
        return graphics::BlendMode::Type::Alpha;
    }
    if (mode == "add") {
        return graphics::BlendMode::Type::Additive;
    }
    if (mode == "multiply") {
        return graphics::BlendMode::Type::Multiply;
    }
    if (mode == "screen") {
        return graphics::BlendMode::Type::Screen;
    }
    throw std::invalid_argument("The Tiled layer \"" + std::string(layer) + "\" uses the blend mode \"" + mode + "\", which Haylen cannot draw. Layers can use \"normal\", \"add\", \"multiply\" or \"screen\".");
}

// Resolves file values against the content folder, including the file items of lists and of lists nested in them.
core::Json MapParser::resolveValue(std::string_view type, core::Json value, std::string_view directory) {
    if (type == "file" && value.is_string()) {
        return resolve(directory, value.get<std::string>());
    }
    if (type == "list" && value.is_array()) {
        for (core::Json& item : value) {
            item["value"] = resolveValue(item.value("type", std::string("string")), item.value("value", core::Json()), directory);
        }
    }
    return value;
}

Properties MapParser::readProperties(const core::Json& owner, std::string_view directory) {
    std::vector<Property> items;
    for (const core::Json& entry : owner.value("properties", core::Json::array())) {
        Property property{.name = entry.at("name").get<std::string>(), .type = entry.value("type", std::string("string")), .propertyType = entry.value("propertytype", std::string{})};
        property.value = resolveValue(property.type, entry.value("value", core::Json()), directory);
        items.push_back(std::move(property));
    }
    return Properties(std::move(items));
}

Map::Orientation MapParser::readOrientation(const std::string& name) {
    const std::optional<Map::Orientation> orientation = Map::orientationFromName(name);
    if (!orientation) {
        throw std::invalid_argument("The orientation of a Tiled map must be \"orthogonal\", \"isometric\", \"staggered\", \"hexagonal\" or \"oblique\", not \"" + name + "\".");
    }
    return *orientation;
}

Map::RenderOrder MapParser::readRenderOrder(const std::string& name) {
    const std::optional<Map::RenderOrder> order = Map::renderOrderFromName(name);
    if (!order) {
        throw std::invalid_argument("The render order of a Tiled map must be \"right-down\", \"right-up\", \"left-down\" or \"left-up\", not \"" + name + "\".");
    }
    return *order;
}

std::vector<math::Vec2> MapParser::readPoints(const core::Json& points) {
    std::vector<math::Vec2> result;
    for (const core::Json& point : points) {
        result.push_back({point.at("x").get<float>(), point.at("y").get<float>()});
    }
    return result;
}

Object MapParser::readObject(const core::Json& source, std::string_view directory) const {
    core::Json object = source;
    Properties properties = readProperties(source, directory);
    std::string templatePath;

    // Template objects give defaults that the instance overrides field by field and property by property.
    if (source.contains("template")) {
        templatePath = resolve(directory, source.at("template").get<std::string>());
        const core::Json document = read(templatePath);
        const std::string templateDirectory{io::Path::directory(templatePath)};
        const core::Json& base = document.at("object");
        object = base;
        for (const auto& [key, value] : source.items()) {
            object[key] = value;
        }
        properties = readProperties(base, templateDirectory).merged(properties);
        if (!source.contains("gid") && base.contains("gid")) {
            object["gid"] = readTemplateGid(document, base.at("gid").get<std::uint32_t>(), templateDirectory);
        }
    }

    Object result{
        .id = object.value("id", 0U),
        .name = object.value("name", std::string{}),
        .type = object.value("type", std::string{}),
        .position = {object.value("x", 0.0F), object.value("y", 0.0F)},
        .size = {object.value("width", 0.0F), object.value("height", 0.0F)},
        .rotation = math::Math::radians(object.value("rotation", 0.0F)),
        .visible = object.value("visible", true),
        .opacity = object.value("opacity", 1.0F),
        .gid = object.value("gid", 0U),
        .properties = std::move(properties),
        .templatePath = std::move(templatePath),
    };

    if (result.gid != 0) {
        result.shape = Object::Shape::Tile;
    } else if (object.contains("text")) {
        const core::Json& text = object.at("text");
        result.shape = Object::Shape::Text;
        result.text = {
            .text = text.value("text", std::string{}),
            .fontFamily = text.value("fontfamily", std::string("sans-serif")),
            .pixelSize = text.value("pixelsize", 16.0F),
            .wrap = text.value("wrap", false),
            .color = readOptionalColor(text, "color").value_or(math::Color::black()),
            .bold = text.value("bold", false),
            .italic = text.value("italic", false),
            .horizontalAlign = text.value("halign", std::string("left")),
            .verticalAlign = text.value("valign", std::string("top")),
        };
    } else if (object.value("ellipse", false)) {
        result.shape = Object::Shape::Ellipse;
    } else if (object.value("capsule", false)) {
        result.shape = Object::Shape::Capsule;
    } else if (object.value("point", false)) {
        result.shape = Object::Shape::Point;
    } else if (object.contains("polygon")) {
        result.shape = Object::Shape::Polygon;
        result.points = readPoints(object.at("polygon"));
    } else if (object.contains("polyline")) {
        result.shape = Object::Shape::Polyline;
        result.points = readPoints(object.at("polyline"));
    }
    return result;
}

// Tile templates store a gid of their own tileset, which the map also lists under a different first gid.
std::uint32_t MapParser::readTemplateGid(const core::Json& document, std::uint32_t gid, std::string_view directory) const {
    const core::Json& reference = document.at("tileset");
    const std::string source = resolve(directory, reference.at("source").get<std::string>());
    const std::uint32_t local = Map::tileId(gid) - reference.at("firstgid").get<std::uint32_t>();
    for (const Map::TilesetReference& candidate : map.tilesets) {
        if (candidate.tileset->path == source) {
            return (gid & Map::kFlagMask) | (candidate.firstGid + local);
        }
    }
    throw std::invalid_argument("A tile template uses the tileset \"" + source + "\", which the map does not list.");
}

std::shared_ptr<Tileset> MapParser::readTileset(const core::Json& document, std::string_view directory, std::string path) const {
    auto tileset = std::make_shared<Tileset>();
    tileset->name = document.value("name", std::string{});
    tileset->type = document.value("class", std::string{});
    tileset->path = std::move(path);
    tileset->image = resolve(directory, document.value("image", std::string{}));
    tileset->transparentColor = readOptionalColor(document, "transparentcolor");
    tileset->imageSize = {document.value("imagewidth", 0.0F), document.value("imageheight", 0.0F)};
    tileset->tileSize = {document.at("tilewidth").get<float>(), document.at("tileheight").get<float>()};
    tileset->columns = document.value("columns", 0);
    tileset->tileCount = document.value("tilecount", 0);
    tileset->margin = document.value("margin", 0);
    tileset->spacing = document.value("spacing", 0);
    if (document.contains("tileoffset")) {
        tileset->tileOffset = {document.at("tileoffset").value("x", 0.0F), document.at("tileoffset").value("y", 0.0F)};
    }
    tileset->objectAlignment = document.value("objectalignment", std::string("unspecified"));
    tileset->renderGridSize = document.value("tilerendersize", std::string("tile")) == "grid";
    tileset->preserveAspect = document.value("fillmode", std::string("stretch")) == "preserve-aspect-fit";
    tileset->properties = readProperties(document, directory);

    for (const core::Json& entry : document.value("tiles", core::Json::array())) {
        Tile tile{.id = entry.at("id").get<std::uint32_t>(), .type = entry.value("type", std::string{}), .properties = readProperties(entry, directory), .probability = entry.value("probability", 1.0F)};
        for (const core::Json& frame : entry.value("animation", core::Json::array())) {
            tile.animation.push_back({.tileId = frame.at("tileid").get<std::uint32_t>(), .duration = frame.at("duration").get<float>() / 1000.0F});
        }
        if (entry.contains("objectgroup")) {
            for (const core::Json& object : entry.at("objectgroup").value("objects", core::Json::array())) {
                tile.collision.push_back(readObject(object, directory));
            }
        }
        if (entry.contains("image")) {
            tile.image = resolve(directory, entry.at("image").get<std::string>());
            const math::Vec2 imageSize{entry.value("imagewidth", 0.0F), entry.value("imageheight", 0.0F)};
            tile.source = {entry.value("x", 0.0F), entry.value("y", 0.0F), entry.value("width", imageSize.x), entry.value("height", imageSize.y)};
        }
        tileset->tiles.emplace(tile.id, std::move(tile));
    }

    for (const core::Json& entry : document.value("wangsets", core::Json::array())) {
        WangSet set{.name = entry.value("name", std::string{}), .type = entry.value("class", std::string{}), .kind = entry.value("type", std::string("corner")), .tile = entry.value("tile", -1), .properties = readProperties(entry, directory)};
        for (const core::Json& color : entry.value("colors", core::Json::array())) {
            set.colors.push_back({.name = color.value("name", std::string{}), .type = color.value("class", std::string{}), .color = readOptionalColor(color, "color").value_or(math::Color::white()), .tile = color.value("tile", -1), .probability = color.value("probability", 1.0F), .properties = readProperties(color, directory)});
        }
        for (const core::Json& tile : entry.value("wangtiles", core::Json::array())) {
            WangSet::Tile wang{.tileId = tile.at("tileid").get<std::uint32_t>()};
            const std::vector<int> ids = tile.at("wangid").get<std::vector<int>>();
            std::ranges::transform(ids | std::views::take(wang.wangId.size()), wang.wangId.begin(), [](int id) { return static_cast<std::uint8_t>(id); });
            set.tiles.push_back(wang);
        }
        tileset->wangSets.push_back(std::move(set));
    }
    return tileset;
}

Layer MapParser::readLayer(const core::Json& entry, std::string_view directory) const {
    const std::string name = entry.value("name", std::string{});
    const std::string kind = entry.at("type").get<std::string>();
    const std::string mode = entry.value("mode", std::string("normal"));
    // Tiled draws the layers of a group with their own blend modes and never blends a group as a whole, so a group mode would change nothing on screen.
    if (kind == "group" && mode != "normal") {
        throw std::invalid_argument("The Tiled group layer \"" + name + "\" uses the blend mode \"" + mode + "\", which Tiled does not apply to the layers inside it. Set the blend mode on those layers.");
    }

    Layer layer{
        .id = entry.at("id").get<std::uint32_t>(),
        .name = name,
        .type = entry.value("class", std::string{}),
        .visible = entry.value("visible", true),
        .opacity = entry.value("opacity", 1.0F),
        .blend = readBlendMode(mode, name),
        .offset = {entry.value("offsetx", 0.0F), entry.value("offsety", 0.0F)},
        .parallax = {entry.value("parallaxx", 1.0F), entry.value("parallaxy", 1.0F)},
        .tint = readOptionalColor(entry, "tintcolor").value_or(math::Color::white()),
        .properties = readProperties(entry, directory),
    };

    if (kind == "tilelayer") {
        layer.kind = Layer::Kind::Tile;
        layer.width = entry.value("width", 0);
        layer.height = entry.value("height", 0);
        if (entry.contains("chunks")) {
            for (const core::Json& chunk : entry.at("chunks")) {
                Layer::Chunk parsed{.x = chunk.at("x").get<int>(), .y = chunk.at("y").get<int>(), .width = chunk.at("width").get<int>(), .height = chunk.at("height").get<int>()};
                parsed.gids = readTileData(entry, chunk.at("data"), countCells(parsed.width, parsed.height, name));
                layer.chunks.push_back(std::move(parsed));
            }
        } else {
            layer.gids = readTileData(entry, entry.at("data"), countCells(layer.width, layer.height, name));
        }
    } else if (kind == "objectgroup") {
        layer.kind = Layer::Kind::Object;
        layer.indexDrawOrder = entry.value("draworder", std::string("topdown")) == "index";
        for (const core::Json& object : entry.value("objects", core::Json::array())) {
            layer.objects.push_back(readObject(object, directory));
        }
    } else if (kind == "imagelayer") {
        layer.kind = Layer::Kind::Image;
        layer.image = resolve(directory, entry.value("image", std::string{}));
        layer.transparentColor = readOptionalColor(entry, "transparentcolor");
        layer.imageSize = {entry.value("imagewidth", 0.0F), entry.value("imageheight", 0.0F)};
        layer.repeatX = entry.value("repeatx", false);
        layer.repeatY = entry.value("repeaty", false);
    } else if (kind == "group") {
        layer.kind = Layer::Kind::Group;
        for (const core::Json& child : entry.value("layers", core::Json::array())) {
            layer.layers.push_back(readLayer(child, directory));
        }
    } else {
        throw std::invalid_argument("The type of a Tiled layer must be \"tilelayer\", \"objectgroup\", \"imagelayer\" or \"group\", not \"" + kind + "\".");
    }
    return layer;
}

} // namespace haylen::tiled
