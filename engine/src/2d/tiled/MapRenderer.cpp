#include "haylen/2d/tiled/MapRenderer.hpp"

#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::tiled {

const MapRenderer::DrawOptions MapRenderer::kDefaultOptions{};

MapRenderer::MapRenderer(Map data, std::shared_ptr<text::Font> textFont) : map(std::move(data)), font(std::move(textFont)) {}

graphics2d::SpriteFlip MapRenderer::flipsOf(std::uint32_t gid, bool diagonal) noexcept {
    return {.horizontal = (gid & Map::kFlipHorizontal) != 0, .vertical = (gid & Map::kFlipVertical) != 0, .diagonal = diagonal && (gid & Map::kFlipDiagonal) != 0};
}

math::Vec2 MapRenderer::alignmentPivot(std::string_view alignment, Map::Orientation orientation) noexcept {
    if (alignment == "topleft") {
        return {0.0F, 0.0F};
    }
    if (alignment == "top") {
        return {0.5F, 0.0F};
    }
    if (alignment == "topright") {
        return {1.0F, 0.0F};
    }
    if (alignment == "left") {
        return {0.0F, 0.5F};
    }
    if (alignment == "center") {
        return {0.5F, 0.5F};
    }
    if (alignment == "right") {
        return {1.0F, 0.5F};
    }
    if (alignment == "bottom") {
        return {0.5F, 1.0F};
    }
    if (alignment == "bottomright") {
        return {1.0F, 1.0F};
    }
    if (alignment == "bottomleft") {
        return {0.0F, 1.0F};
    }
    // Unspecified alignment is bottom left on most maps and bottom center on isometric ones.
    return orientation == Map::Orientation::Isometric ? math::Vec2{0.5F, 1.0F} : math::Vec2{0.0F, 1.0F};
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

std::vector<math::Vec2> MapRenderer::ellipseOutline(math::Vec2 center, math::Vec2 radii) {
    std::vector<math::Vec2> points;
    for (int index = 0; index < kEllipseSegments; ++index) {
        const float angle = math::Math::kTau * static_cast<float>(index) / static_cast<float>(kEllipseSegments);
        points.push_back(center + math::Vec2{std::cos(angle) * radii.x, std::sin(angle) * radii.y});
    }
    return points;
}

std::vector<math::Vec2> MapRenderer::capsuleOutline(math::Vec2 topLeft, math::Vec2 size) {
    const float radius = std::min(size.x, size.y) * 0.5F;
    if (size.x == size.y) {
        return ellipseOutline(topLeft + size * 0.5F, {radius, radius});
    }

    const bool wide = size.x > size.y;
    const math::Vec2 first = topLeft + math::Vec2{radius, radius};
    const math::Vec2 second = topLeft + (wide ? math::Vec2{size.x - radius, radius} : math::Vec2{radius, size.y - radius});
    const float start = wide ? -math::Math::kTau * 0.25F : 0.0F;
    constexpr int steps = kEllipseSegments / 2;
    std::vector<math::Vec2> points;
    for (const auto& [center, from] : {std::pair{second, start}, std::pair{first, start + math::Math::kTau * 0.5F}}) {
        for (int step = 0; step <= steps; ++step) {
            const float angle = from + math::Math::kTau * 0.5F * static_cast<float>(step) / static_cast<float>(steps);
            points.push_back(center + math::Vec2{std::cos(angle), std::sin(angle)} * radius);
        }
    }
    return points;
}

math::Vec2 MapRenderer::rotateAround(math::Vec2 point, math::Vec2 origin, float angle) noexcept {
    const math::Vec2 local = point - origin;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return origin + math::Vec2{local.x * cosine - local.y * sine, local.x * sine + local.y * cosine};
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

template <typename Transform> void MapRenderer::addObjectShape(physics2d::Body& body, const Object& object, const physics2d::Shape::Options& options, Transform&& transform) {
    const math::Vec2 origin = object.position;
    switch (object.shape) {
    case Object::Shape::Point:
    case Object::Shape::Text:
        return;
    case Object::Shape::Rectangle:
    case Object::Shape::Tile: {
        // Tile objects sit on their bottom-left corner while rectangles hang from their top-left corner.
        const math::Vec2 top = object.shape == Object::Shape::Tile ? origin - math::Vec2{0.0F, object.size.y} : origin;
        std::vector<math::Vec2> corners{top, top + math::Vec2{object.size.x, 0.0F}, top + object.size, top + math::Vec2{0.0F, object.size.y}};
        for (math::Vec2& corner : corners) {
            corner = transform(rotateAround(corner, origin, object.rotation));
        }
        body.addPolygon(corners, options);
        return;
    }
    case Object::Shape::Ellipse:
    case Object::Shape::Capsule: {
        std::vector<math::Vec2> outline = object.shape == Object::Shape::Ellipse ? ellipseOutline(origin + object.size * 0.5F, object.size * 0.5F) : capsuleOutline(origin, object.size);
        for (math::Vec2& point : outline) {
            point = transform(rotateAround(point, origin, object.rotation));
        }
        body.addPolygon(outline, options);
        return;
    }
    case Object::Shape::Polygon: {
        std::vector<math::Vec2> points;
        for (const math::Vec2 point : object.points) {
            points.push_back(transform(rotateAround(origin + point, origin, object.rotation)));
        }
        body.addPolygon(points, options);
        return;
    }
    case Object::Shape::Polyline:
        for (std::size_t index = 1; index < object.points.size(); ++index) {
            const math::Vec2 first = transform(rotateAround(origin + object.points[index - 1], origin, object.rotation));
            const math::Vec2 second = transform(rotateAround(origin + object.points[index], origin, object.rotation));
            body.addSegment(first, second, options);
        }
        return;
    }
}

physics2d::CollisionFilter MapRenderer::layerFilter(const Properties& properties) {
    physics2d::CollisionFilter filter;
    filter.category = static_cast<std::uint64_t>(properties.getNumber("category", 1.0));
    if (properties.has("mask")) {
        filter.mask = static_cast<std::uint64_t>(properties.getNumber("mask", 0.0));
    }
    return filter;
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
    caches.erase({found->id, false});
    caches.erase({found->id, true});
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

void MapRenderer::bake(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, LayerCache& cache, bool rows) const {
    std::map<std::pair<long, int>, Region> regions;
    const math::Color color = state.tint;

    // clang-format off
    forEachCell(layer, [&](int column, int row, std::uint32_t gid) {
        const Map::TilesetReference* reference = map.findTileset(gid);
        if (reference == nullptr) {
            throw std::invalid_argument("A tile layer uses a tile that no tileset holds: " + std::to_string(Map::tileId(gid)));
        }
        const Tileset& tileset = *reference->tileset;
        const std::uint32_t localId = Map::tileId(gid) - reference->firstGid;
        const math::Vec2 anchor = cellAnchor(column, row) + state.offset;
        graphics2d::SpriteInstance instance = tileInstance(tileset, localId, gid, anchor);
        instance.color = color;

        // Rows group the cells that stand on the same y, which covers the diagonals of isometric maps too.
        const int block = static_cast<int>(std::floor(static_cast<float>(column) / kRegionCells));
        const long band = rows ? std::lround(anchor.y) : static_cast<long>(std::floor(static_cast<float>(row) / kRegionCells));
        Region& region = regions[{band, block}];
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

    cache.regions.clear();
    for (auto& [key, region] : regions) {
        for (Run& run : region.runs) {
            if (!run.instances.empty()) {
                run.batch = renderer.createStaticBatch(run.texture, run.instances);
                run.instances = {};
            }
        }
        cache.regions.push_back(std::move(region));
    }
    cache.baked = true;
}

void MapRenderer::drawTiles(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const DrawOptions& options) {
    LayerCache& cache = caches[{layer.id, options.ysort}];
    if (!cache.baked) {
        bake(renderer, layer, state, cache, options.ysort);
    }

    // Baked batches hold the layer offsets, so only the map origin and the parallax move them at draw time.
    const math::Vec2 shift = state.origin + parallaxOffset(state, view);
    const bool culling = !view.visible.isEmpty();
    for (const Region& region : cache.regions) {
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

void MapRenderer::drawObjects(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const DrawOptions& options) const {
    std::vector<const Object*> objects;
    for (const Object& object : layer.objects) {
        if (object.visible && (object.shape == Object::Shape::Tile || object.shape == Object::Shape::Text)) {
            objects.push_back(&object);
        }
    }
    if (!layer.indexDrawOrder) {
        std::stable_sort(objects.begin(), objects.end(), [](const Object* lhs, const Object* rhs) { return lhs->position.y < rhs->position.y; });
    }

    const math::Vec2 shift = state.origin + state.offset + parallaxOffset(state, view);
    for (const Object* object : objects) {
        const math::Vec2 position = map.objectToWorld(object->position) + shift;
        if (object->shape == Object::Shape::Text) {
            if (font == nullptr) {
                throw std::logic_error("Drawing Tiled text objects needs a font.");
            }
            const Object::Text& text = object->text;
            const text::TextAlign align = text.horizontalAlign == "center" ? text::TextAlign::Center : (text.horizontalAlign == "right" ? text::TextAlign::Right : (text.horizontalAlign == "justify" ? text::TextAlign::Fill : text::TextAlign::Left));
            const float height = font->measure(text.text, {.size = text.pixelSize, .maxWidth = text.wrap ? object->size.x : 0.0F}).y;
            const float slack = object->size.y - height;
            const float down = text.verticalAlign == "center" ? slack * 0.5F : (text.verticalAlign == "bottom" ? slack : 0.0F);
            const float across = align == text::TextAlign::Center ? object->size.x * 0.5F : (align == text::TextAlign::Right ? object->size.x : 0.0F);
            const math::Vec2 origin = rotateAround(position + math::Vec2{across, down}, position, object->rotation);
            const graphics2d::DrawOrder order = options.ysort ? groundOrder(options.order, position.y + object->size.y, origin.y) : options.order;
            renderer.drawText(*font, text.text, origin, {.size = text.pixelSize, .color = fade(state.tint * text.color, object->opacity), .align = align, .maxWidth = text.wrap ? object->size.x : 0.0F, .anchor = {align == text::TextAlign::Center ? 0.5F : (align == text::TextAlign::Right ? 1.0F : 0.0F), 0.0F}, .rotation = object->rotation}, order);
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
            .pivot = alignmentPivot(tileset.objectAlignment, map.orientation),
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
    // clang-format off
    const auto walk = [&](const auto& self, const std::vector<Layer>& layers, math::Vec2 offset) -> bool {
        bool found = false;
        for (const Layer& child : layers) {
            const math::Vec2 origin = offset + child.offset;
            const bool matches = layer.empty() || child.name == layer;
            if (matches && child.kind == Layer::Kind::Object) {
                for (const Object& object : child.objects) {
                    visit(object, map.objectToWorld(object.position) + origin);
                }
                found = true;
            }
            found = self(self, child.layers, origin) || found;
        }
        return found;
    };
    // clang-format on
    if (!walk(walk, map.layers, {}) && !layer.empty()) {
        throw std::invalid_argument("Unknown object layer: " + std::string(layer));
    }
}

std::vector<physics2d::Body> MapRenderer::buildCollision(physics2d::World& world) const {
    std::vector<physics2d::Body> bodies;

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

        const physics2d::CollisionFilter filter = layerFilter(layer.properties);
        if (layer.kind == Layer::Kind::Object) {
            physics2d::Body body = world.createBody({.type = physics2d::Body::Type::Static});
            const bool wholeLayer = layer.type == "collision";
            for (const Object& object : layer.objects) {
                if (wholeLayer || object.type == "collision") {
                    const physics2d::Shape::Options options{.filter = filter, .sensor = object.properties.getBool("sensor", false)};
                    addObjectShape(body, object, options, [&](math::Vec2 point) { return map.objectToWorld(point) + origin; });
                }
            }
            finish(body);
            return;
        }
        if (layer.kind != Layer::Kind::Tile || !layer.properties.getBool("collision", true)) {
            return;
        }

        // Tiles whose whole cell is solid merge into row-wide boxes, which keeps large blocked areas cheap.
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
                const physics2d::Shape::Options options{.filter = filter, .sensor = object.properties.getBool("sensor", false)};
                addObjectShape(body, object, options, [&](math::Vec2 point) { return topLeft + flipInTile(point, imageSize, gid); });
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
