#include "haylen/2d/tiled/MapRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/MapQuery.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::tiled {

const MapRenderer::DrawOptions MapRenderer::kDefaultOptions{};

MapRenderer::MapRenderer(Map data, std::shared_ptr<text::Font> textFont) : map(std::move(data)), font(std::move(textFont)) {}

graphics2d::SpriteFlip MapRenderer::flipsOf(std::uint32_t gid, bool diagonal) noexcept {
    return {.horizontal = (gid & Map::kFlipHorizontal) != 0, .vertical = (gid & Map::kFlipVertical) != 0, .diagonal = diagonal && (gid & Map::kFlipDiagonal) != 0};
}

math::Vec2 MapRenderer::drawSize(const Tileset& tileset, math::Rect source, math::Vec2 grid) noexcept {
    if (!tileset.renderGridSize) {
        return source.getSize();
    }
    if (!tileset.preserveAspect) {
        return grid;
    }
    const float scale = std::min(grid.x / source.width, grid.y / source.height);
    return source.getSize() * scale;
}

math::Rect MapRenderer::instanceBounds(const graphics2d::SpriteInstance& instance) noexcept {
    const float reach = std::max(instance.size.x, instance.size.y);
    return math::Rect::fromCenter(instance.position, {reach, reach});
}

math::Color MapRenderer::fade(math::Color color, float opacity) noexcept {
    return color.withAlpha(color.a * opacity);
}

math::Vec2 MapRenderer::flipInTile(math::Vec2 point, math::Vec2 tileSize, std::uint32_t gid) noexcept {
    math::Vec2 result = point;
    math::Vec2 size = tileSize;
    if ((gid & Map::kFlipDiagonal) != 0) {
        result = {result.y, result.x};
        size = {size.y, size.x};
    }
    if ((gid & Map::kFlipHorizontal) != 0) {
        result.x = size.x - result.x;
    }
    if ((gid & Map::kFlipVertical) != 0) {
        result.y = size.y - result.y;
    }
    return result;
}

void MapRenderer::addOutline(physics2d::Body& body, const Object& object, std::span<const math::Vec2> points, const physics2d::Shape::Options& options) {
    if (points.empty()) {
        return;
    }
    if (object.shape != Object::Shape::Polyline) {
        body.addPolygon(points, options);
        return;
    }
    for (std::size_t index = 1; index < points.size(); ++index) {
        body.addSegment(points[index - 1], points[index], options);
    }
}

physics2d::CollisionFilter MapRenderer::layerFilter(const Layer& layer) {
    physics2d::CollisionFilter filter;
    filter.category = readCollisionBits(layer, "category", filter.category);
    filter.mask = readCollisionBits(layer, "mask", filter.mask);
    return filter;
}

std::uint64_t MapRenderer::readCollisionBits(const Layer& layer, std::string_view name, std::uint64_t fallback) {
    const Property* property = layer.properties.find(name);
    if (property == nullptr) {
        return fallback;
    }
    if (!property->value.is_number_integer() || property->value < 0) {
        throw std::invalid_argument("The collision property '" + std::string(name) + "' of the Tiled layer '" + layer.name + "' needs an integer of at least 0.");
    }
    return property->value.get<std::uint64_t>();
}

bool MapRenderer::isFullCell(const Object& object, math::Vec2 tileSize) noexcept {
    return object.shape == Object::Shape::Rectangle && object.rotation == 0.0F && object.position == math::Vec2{} && object.size == tileSize && !object.properties.getBool("sensor", false);
}

graphics2d::DrawOrder MapRenderer::groundOrder(const graphics2d::DrawOrder& order, float ground, float standing) noexcept {
    graphics2d::DrawOrder placed = order;
    placed.depth = ground;
    placed.sortOffset = order.sortOffset + ground - standing;
    return placed;
}

MapRenderer::Inherited MapRenderer::inherit(const Inherited& parent, const Layer& layer) noexcept {
    const math::Color tint = parent.tint * layer.tint;
    return {
        .origin = parent.origin,
        .offset = parent.offset + layer.offset,
        .parallax = {parent.parallax.x * layer.parallax.x, parent.parallax.y * layer.parallax.y},
        .tint = tint.withAlpha(tint.a * layer.opacity),
        .visible = parent.visible && layer.visible,
    };
}

math::Vec2 MapRenderer::cellAnchor(int column, int row) const noexcept {
    const math::Vec2 cell = map.cellToWorld(column, row);
    if (map.orientation == Map::Orientation::Isometric) {
        return {cell.x - map.tileSize.x * 0.5F, cell.y + map.tileSize.y};
    }
    if (map.orientation == Map::Orientation::Oblique) {
        return map.cellToWorld(column, row + 1);
    }
    return {cell.x, cell.y + map.tileSize.y};
}

graphics2d::SpriteInstance MapRenderer::tileInstance(const Tileset& tileset, std::uint32_t localId, std::uint32_t gid, math::Vec2 anchor) const {
    const math::Rect source = tileset.getSource(localId);
    const math::Vec2 size = drawSize(tileset, source, map.tileSize);
    const bool diagonal = (gid & Map::kFlipDiagonal) != 0;
    const math::Vec2 footprint = diagonal ? math::Vec2{size.y, size.x} : size;
    const math::Vec2 topLeft = anchor + math::Vec2{0.0F, -footprint.y} + tileset.tileOffset;
    return {
        .position = topLeft + footprint * 0.5F,
        .size = footprint,
        .source = source,
        .pivot = {0.5F, 0.5F},
        .rotation = (gid & Map::kRotateHexagonal) != 0 ? math::Math::kTau / 3.0F : 0.0F,
        .flip = flipsOf(gid, true),
    };
}

template <typename Visit> void MapRenderer::forEachCell(const Layer& layer, Visit&& visit) const {
    const bool reverseRows = map.renderOrder == Map::RenderOrder::RightUp || map.renderOrder == Map::RenderOrder::LeftUp;
    const bool reverseColumns = map.renderOrder == Map::RenderOrder::LeftDown || map.renderOrder == Map::RenderOrder::LeftUp;
    // clang-format off
    const auto visitGrid = [&](int left, int top, int columns, int rows, const std::vector<std::uint32_t>& gids) {
        for (int step = 0; step < rows; ++step) {
            const int row = reverseRows ? rows - 1 - step : step;
            for (int across = 0; across < columns; ++across) {
                const int column = reverseColumns ? columns - 1 - across : across;
                const std::uint32_t gid = gids[static_cast<std::size_t>(row * columns + column)];
                if (gid != 0) {
                    visit(left + column, top + row, gid);
                }
            }
        }
    };
    // clang-format on

    if (layer.chunks.empty()) {
        visitGrid(0, 0, layer.width, layer.height, layer.gids);
        return;
    }
    std::vector<const Layer::Chunk*> chunks;
    for (const Layer::Chunk& chunk : layer.chunks) {
        chunks.push_back(&chunk);
    }
    std::sort(chunks.begin(), chunks.end(), [](const Layer::Chunk* lhs, const Layer::Chunk* rhs) { return lhs->y != rhs->y ? lhs->y < rhs->y : lhs->x < rhs->x; });
    for (const Layer::Chunk* chunk : chunks) {
        visitGrid(chunk->x, chunk->y, chunk->width, chunk->height, chunk->gids);
    }
}

std::uint32_t MapRenderer::getTile(std::string_view layer, int column, int row) const {
    const Layer* found = map.findLayer(layer);
    if (found == nullptr || found->kind != Layer::Kind::Tile) {
        throw std::invalid_argument("Unknown tile layer: " + std::string(layer));
    }
    return found->getGid(column, row);
}

void MapRenderer::setTile(std::string_view layer, int column, int row, std::uint32_t gid) {
    Layer* found = map.findLayer(layer);
    if (found == nullptr || found->kind != Layer::Kind::Tile) {
        throw std::invalid_argument("Unknown tile layer: " + std::string(layer));
    }
    if (gid != 0 && map.findTileset(gid) == nullptr) {
        throw std::invalid_argument("No tileset holds the tile " + std::to_string(Map::tileId(gid)));
    }
    found->setGid(column, row, gid);

    // Only the region of the cell bakes again, in every cache of the layer that baked already.
    for (const bool rows : {false, true}) {
        if (const auto cache = caches.find({found->id, rows}); cache != caches.end() && cache->second.baked) {
            cache->second.stale.insert(regionOf(column, row, cellAnchor(column, row).y + cache->second.offset.y, rows));
        }
    }
}

void MapRenderer::setLayerVisible(std::string_view layer, bool visible) {
    Layer* found = map.findLayer(layer);
    if (found == nullptr) {
        throw std::invalid_argument("Unknown layer: " + std::string(layer));
    }
    found->visible = visible;
}

void MapRenderer::update(float deltaSeconds) noexcept {
    time += deltaSeconds;
}

math::Vec2 MapRenderer::parallaxOffset(const Inherited& state, const View& view) const noexcept {
    const math::Vec2 shift = view.center - (state.origin + map.parallaxOrigin);
    return {shift.x * (1.0F - state.parallax.x), shift.y * (1.0F - state.parallax.y)};
}

std::uint32_t MapRenderer::animatedTileId(const Tile& tile) const noexcept {
    float total = 0.0F;
    for (const Tile::AnimationFrame& frame : tile.animation) {
        total += frame.duration;
    }
    if (total <= 0.0F) {
        return tile.animation.front().tileId;
    }
    float elapsed = std::fmod(time, total);
    for (const Tile::AnimationFrame& frame : tile.animation) {
        if (elapsed < frame.duration) {
            return frame.tileId;
        }
        elapsed -= frame.duration;
    }
    return tile.animation.back().tileId;
}

void MapRenderer::draw(graphics2d::Renderer& renderer, const View& view, const DrawOptions& options) {
    for (const Layer& layer : map.layers) {
        drawTree(renderer, layer, {.origin = options.offset}, view, options);
    }
}

void MapRenderer::drawLayer(graphics2d::Renderer& renderer, std::string_view name, const View& view, const DrawOptions& options) {
    // The inherited state of a nested layer comes from the groups above it, so the walk starts at the top.
    // clang-format off
    const auto walk = [&](const auto& self, const std::vector<Layer>& layers, const Inherited& parent) -> bool {
        for (const Layer& layer : layers) {
            if (layer.name == name) {
                drawTree(renderer, layer, parent, view, options);
                return true;
            }
            if (self(self, layer.layers, inherit(parent, layer))) {
                return true;
            }
        }
        return false;
    };
    // clang-format on
    if (!walk(walk, map.layers, {.origin = options.offset})) {
        throw std::invalid_argument("Unknown layer: " + std::string(name));
    }
}

void MapRenderer::drawTree(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& parent, const View& view, const DrawOptions& options) {
    const Inherited state = inherit(parent, layer);
    if (!state.visible) {
        return;
    }

    DrawOptions blended = options;
    blended.order.blend = layer.blend;
    switch (layer.kind) {
    case Layer::Kind::Tile:
        drawTiles(renderer, layer, state, view, blended);
        return;
    case Layer::Kind::Object:
        drawObjects(renderer, layer, state, view, blended);
        return;
    case Layer::Kind::Image:
        drawImage(renderer, layer, state, view, blended.order);
        return;
    case Layer::Kind::Group:
        for (const Layer& child : layer.layers) {
            drawTree(renderer, child, state, view, options);
        }
        return;
    }
}

MapRenderer::RegionKey MapRenderer::regionOf(int column, int row, float ground, bool rows) noexcept {
    // Rows group the cells that stand on the same y, which covers the diagonals of isometric maps too.
    const int block = static_cast<int>(std::floor(static_cast<float>(column) / kRegionCells));
    const long band = rows ? std::lround(ground) : static_cast<long>(std::floor(static_cast<float>(row) / kRegionCells));
    return {band, block};
}

void MapRenderer::bake(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, LayerCache& cache, bool rows) const {
    std::map<RegionKey, Region> regions;
    const math::Color color = state.tint;
    const bool partial = cache.baked;

    // clang-format off
    forEachCell(layer, [&](int column, int row, std::uint32_t gid) {
        const math::Vec2 anchor = cellAnchor(column, row) + state.offset;
        const RegionKey key = regionOf(column, row, anchor.y, rows);
        if (partial && !cache.stale.contains(key)) {
            return;
        }

        const Map::TilesetReference* reference = map.findTileset(gid);
        if (reference == nullptr) {
            throw std::invalid_argument("A tile layer uses a tile that no tileset holds: " + std::to_string(Map::tileId(gid)));
        }
        const Tileset& tileset = *reference->tileset;
        const std::uint32_t localId = Map::tileId(gid) - reference->firstGid;
        graphics2d::SpriteInstance instance = tileInstance(tileset, localId, gid, anchor);
        instance.color = color;

        Region& region = regions[key];
        region.ground = anchor.y;
        region.bounds = region.runs.empty() ? instanceBounds(instance) : region.bounds.merged(instanceBounds(instance));

        const Tile* tile = tileset.findTile(localId);
        const bool animated = tile != nullptr && !tile->animation.empty();
        const graphics::Texture& texture = tileset.getTexture(localId);
        const bool continues = !region.runs.empty() && region.runs.back().texture == texture && region.runs.back().animated.empty() != animated;
        if (!continues) {
            region.runs.push_back({.texture = texture});
        }
        if (animated) {
            region.runs.back().animated.push_back({.tileset = &tileset, .tile = tile, .instance = instance});
        } else {
            region.runs.back().instances.push_back(instance);
        }
    });
    // clang-format on

    // Stale regions whose cells all emptied disappear, and the others replace their old batches.
    if (partial) {
        for (const RegionKey& key : cache.stale) {
            cache.regions.erase(key);
        }
    }
    for (auto& [key, region] : regions) {
        for (Run& run : region.runs) {
            if (!run.instances.empty()) {
                run.batch = renderer.createStaticBatch(run.texture, run.instances);
                run.instances = {};
            }
        }
        cache.regions.insert_or_assign(key, std::move(region));
    }
    cache.stale.clear();
    cache.offset = state.offset;
    cache.baked = true;
}

void MapRenderer::drawTiles(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const DrawOptions& options) {
    LayerCache& cache = caches[{layer.id, options.ysort}];
    if (!cache.baked || !cache.stale.empty()) {
        bake(renderer, layer, state, cache, options.ysort);
    }

    // Baked batches hold the layer offsets, so only the map origin and the parallax move them at draw time.
    const math::Vec2 shift = state.origin + parallaxOffset(state, view);
    const bool culling = !view.visible.isEmpty();
    for (const auto& [key, region] : cache.regions) {
        if (culling && !region.bounds.translated(shift).intersects(view.visible)) {
            continue;
        }
        const float ground = region.ground + shift.y;
        for (const Run& run : region.runs) {
            if (run.batch.isValid()) {
                const float standing = run.batch.getBounds().getBottom() + shift.y;
                renderer.drawStatic(run.batch, options.ysort ? groundOrder(options.order, ground, standing) : options.order, shift);
                continue;
            }

            // Animated tiles may change texture from frame to frame when they come from an image collection.
            for (const AnimatedTile& animated : run.animated) {
                const std::uint32_t frame = animatedTileId(*animated.tile);
                graphics2d::SpriteInstance instance = animated.instance;
                instance.source = animated.tileset->getSource(frame);
                instance.position += shift;
                renderer.drawBatch(animated.tileset->getTexture(frame), std::span(&instance, 1), options.ysort ? groundOrder(options.order, ground, instance.position.y) : options.order);
            }
        }
    }
}

std::vector<std::size_t> MapRenderer::drawnObjects(const Layer& layer) const {
    std::vector<std::size_t> drawn;
    for (std::size_t index = 0; index < layer.objects.size(); ++index) {
        const Object& object = layer.objects[index];
        if (object.visible && (object.shape == Object::Shape::Tile || object.shape == Object::Shape::Text)) {
            drawn.push_back(index);
        }
    }
    if (!layer.indexDrawOrder) {
        std::stable_sort(drawn.begin(), drawn.end(), [&layer](std::size_t lhs, std::size_t rhs) { return layer.objects[lhs].position.y < layer.objects[rhs].position.y; });
    }
    return drawn;
}

void MapRenderer::drawObjects(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const DrawOptions& options) {
    // Objects never change once the map loads, so each layer sorts them once.
    auto drawn = objectOrders.find(layer.id);
    if (drawn == objectOrders.end()) {
        drawn = objectOrders.emplace(layer.id, drawnObjects(layer)).first;
    }

    const math::Vec2 shift = state.origin + state.offset + parallaxOffset(state, view);
    for (const std::size_t index : drawn->second) {
        const Object* object = &layer.objects[index];
        const math::Vec2 position = map.objectToWorld(object->position) + shift;
        if (object->shape == Object::Shape::Text) {
            if (font == nullptr) {
                throw std::logic_error("Drawing Tiled text objects needs a font.");
            }
            const Object::Text& text = object->text;
            const text::Alignment align = text.horizontalAlign == "center" ? text::Alignment::Center : (text.horizontalAlign == "right" ? text::Alignment::Right : (text.horizontalAlign == "justify" ? text::Alignment::Fill : text::Alignment::Left));
            const float height = font->measure(text.text, {.size = text.pixelSize, .maxWidth = text.wrap ? object->size.x : 0.0F}).y;
            const float slack = object->size.y - height;
            const float down = text.verticalAlign == "center" ? slack * 0.5F : (text.verticalAlign == "bottom" ? slack : 0.0F);
            const float across = align == text::Alignment::Center ? object->size.x * 0.5F : (align == text::Alignment::Right ? object->size.x : 0.0F);
            const math::Vec2 origin = MapQuery::rotateAround(position + math::Vec2{across, down}, position, object->rotation);
            const graphics2d::DrawOrder order = options.ysort ? groundOrder(options.order, position.y + object->size.y, origin.y) : options.order;
            renderer.drawText(*font, text.text, origin, {.size = text.pixelSize, .color = fade(state.tint * text.color, object->opacity), .align = align, .maxWidth = text.wrap ? object->size.x : 0.0F, .anchor = {align == text::Alignment::Center ? 0.5F : (align == text::Alignment::Right ? 1.0F : 0.0F), 0.0F}, .rotation = object->rotation}, order);
            continue;
        }

        const Map::TilesetReference* reference = map.findTileset(object->gid);
        if (reference == nullptr) {
            throw std::invalid_argument("A tile object uses a tile that no tileset holds: " + std::to_string(Map::tileId(object->gid)));
        }
        const Tileset& tileset = *reference->tileset;
        std::uint32_t localId = Map::tileId(object->gid) - reference->firstGid;
        if (const Tile* tile = tileset.findTile(localId); tile != nullptr && !tile->animation.empty()) {
            localId = animatedTileId(*tile);
        }
        const math::Rect source = tileset.getSource(localId);
        const graphics2d::SpriteInstance instance{
            .position = position,
            .size = object->size.isZero() ? source.getSize() : object->size,
            .source = source,
            .pivot = MapQuery::alignmentPivot(tileset.objectAlignment, map.orientation),
            .rotation = object->rotation,
            .color = fade(state.tint, object->opacity),
            .flip = flipsOf(object->gid, false),
        };
        if (view.visible.isEmpty() || instanceBounds(instance).intersects(view.visible)) {
            const float ground = instance.position.y + instance.size.y * (1.0F - instance.pivot.y);
            renderer.drawBatch(tileset.getTexture(localId), std::span(&instance, 1), options.ysort ? groundOrder(options.order, ground, instance.position.y) : options.order);
        }
    }
}

void MapRenderer::drawImage(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const graphics2d::DrawOrder& order) const {
    if (!layer.texture.isValid()) {
        return;
    }

    const math::Vec2 size = layer.texture.getSize();
    const math::Vec2 origin = state.origin + state.offset + parallaxOffset(state, view);
    if (!layer.repeatX && !layer.repeatY) {
        renderer.draw({.texture = layer.texture, .position = origin, .pivot = {0.0F, 0.0F}, .color = state.tint, .order = order});
        return;
    }
    if (view.visible.isEmpty()) {
        throw std::invalid_argument("Repeated image layers need the visible area of the view.");
    }

    // Repeated images cover the visible area with copies aligned to the layer origin.
    const auto firstCopy = [](float start, float from, float step) { return from + std::floor((start - from) / step) * step; };
    const float left = layer.repeatX ? firstCopy(view.visible.getLeft(), origin.x, size.x) : origin.x;
    const float top = layer.repeatY ? firstCopy(view.visible.getTop(), origin.y, size.y) : origin.y;
    const float right = layer.repeatX ? view.visible.getRight() : origin.x + 1.0F;
    const float bottom = layer.repeatY ? view.visible.getBottom() : origin.y + 1.0F;
    std::vector<graphics2d::SpriteInstance> copies;
    for (float y = top; y < bottom; y += size.y) {
        for (float x = left; x < right; x += size.x) {
            copies.push_back({.position = {x, y}, .size = size, .pivot = {0.0F, 0.0F}, .color = state.tint});
        }
    }
    renderer.drawBatch(layer.texture, copies, order);
}

void MapRenderer::forEachObject(std::string_view layer, const ObjectVisitor& visit) const {
    MapQuery(map).forEachObject(layer, [&](const Object& object, math::Vec2 offset) { visit(object, map.objectToWorld(object.position) + offset); });
}

std::vector<physics2d::Body> MapRenderer::buildCollision(physics2d::World& world) const {
    std::vector<physics2d::Body> bodies;
    const MapQuery query(map);
    std::vector<math::Vec2> points;

    // clang-format off
    const auto finish = [&bodies](physics2d::Body body) {
        if (body.getShapes().empty()) {
            body.destroy();
            return;
        }
        bodies.push_back(body);
    };
    const auto visit = [&](const auto& self, const Layer& layer, math::Vec2 offset) -> void {
        const math::Vec2 origin = offset + layer.offset;
        if (layer.kind == Layer::Kind::Group) {
            for (const Layer& child : layer.layers) {
                self(self, child, origin);
            }
            return;
        }

        if (layer.kind == Layer::Kind::Object) {
            const physics2d::CollisionFilter filter = layerFilter(layer);
            physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static});
            const bool wholeLayer = layer.type == "collision";
            for (const Object& object : layer.objects) {
                if (wholeLayer || object.type == "collision") {
                    query.getOutline(object, origin, points);
                    addOutline(body, object, points, {.filter = filter, .sensor = object.properties.getBool("sensor", false)});
                }
            }
            finish(body);
            return;
        }
        if (layer.kind != Layer::Kind::Tile || !layer.properties.getBool("collision", true)) {
            return;
        }

        // Tiles whose whole cell is solid merge into row-wide boxes, which keeps large blocked areas cheap.
        const physics2d::CollisionFilter filter = layerFilter(layer);
        physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static});
        std::map<int, std::vector<int>> fullCells;
        forEachCell(layer, [&](int column, int row, std::uint32_t gid) {
            const Map::TilesetReference* reference = map.findTileset(gid);
            if (reference == nullptr) {
                return;
            }
            const Tileset& tileset = *reference->tileset;
            const std::uint32_t localId = Map::tileId(gid) - reference->firstGid;
            const Tile* tile = tileset.findTile(localId);
            if (tile == nullptr || tile->collision.empty()) {
                return;
            }

            const graphics2d::SpriteInstance placed = tileInstance(tileset, localId, gid, cellAnchor(column, row) + origin);
            const math::Vec2 topLeft = placed.position - placed.size * 0.5F;
            const math::Vec2 imageSize = drawSize(tileset, tileset.getSource(localId), map.tileSize);
            for (const Object& object : tile->collision) {
                if (map.orientation == Map::Orientation::Orthogonal && placed.size == map.tileSize && isFullCell(object, map.tileSize)) {
                    fullCells[row].push_back(column);
                    continue;
                }
                query.traceOutline(object, points);
                for (math::Vec2& point : points) {
                    point = topLeft + flipInTile(point, imageSize, gid);
                }
                addOutline(body, object, points, {.filter = filter, .sensor = object.properties.getBool("sensor", false)});
            }
        });

        for (auto& [row, columns] : fullCells) {
            std::sort(columns.begin(), columns.end());
            std::size_t start = 0;
            for (std::size_t index = 1; index <= columns.size(); ++index) {
                if (index < columns.size() && columns[index] == columns[index - 1] + 1) {
                    continue;
                }
                const math::Vec2 corner = map.cellToWorld(columns[start], row) + origin;
                const float cells = static_cast<float>(columns[index - 1] - columns[start] + 1);
                const math::Vec2 size{cells * map.tileSize.x, map.tileSize.y};
                body.addBox(size, {.filter = filter, .offset = corner + size * 0.5F});
                start = index;
            }
        }
        finish(body);
    };
    // clang-format on

    for (const Layer& layer : map.layers) {
        visit(visit, layer, {});
    }
    return bodies;
}

} // namespace haylen::tiled
