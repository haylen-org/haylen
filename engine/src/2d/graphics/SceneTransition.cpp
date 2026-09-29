#include "haylen/2d/graphics/SceneTransition.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/math/Math.hpp"

namespace haylen::graphics2d {

const DrawOrder SceneTransition::kCapturedOrder{.blend = graphics::BlendMode::Type::Premultiplied};

math::Vec2 SceneTransition::getSteps(Direction direction) noexcept {
    switch (direction) {
    case Direction::Left:
        return {-1.0F, 0.0F};
    case Direction::Right:
        return {1.0F, 0.0F};
    case Direction::Up:
        return {0.0F, -1.0F};
    case Direction::Down:
        return {0.0F, 1.0F};
    case Direction::UpLeft:
        return {-1.0F, -1.0F};
    case Direction::UpRight:
        return {1.0F, -1.0F};
    case Direction::DownLeft:
        return {-1.0F, 1.0F};
    case Direction::DownRight:
        return {1.0F, 1.0F};
    }
    return {};
}

bool SceneTransition::isVertical(Direction direction) noexcept {
    return direction == Direction::Up || direction == Direction::Down;
}

math::Rect SceneTransition::getPlace(const math::Rect& area, const math::Rect& part) noexcept {
    return {area.x + part.x * area.width, area.y + part.y * area.height, part.width * area.width, part.height * area.height};
}

math::Rect SceneTransition::getSource(const graphics::Texture& texture, const math::Rect& part) noexcept {
    const math::Vec2 size = texture.getSize();
    return {part.x * size.x, part.y * size.y, part.width * size.x, part.height * size.y};
}

float SceneTransition::getHash(int index) noexcept {
    auto value = static_cast<std::uint32_t>(index) * 0x9E3779B1U + 0x7F4A7C15U;
    value ^= value >> 15U;
    value *= 0x2C1B3C6DU;
    value ^= value >> 12U;
    return static_cast<float>(value & 0xFFFFFFU) / static_cast<float>(0x1000000U);
}

std::vector<math::Vec2> SceneTransition::clip(std::span<const math::Vec2> polygon, math::Vec2 normal, float limit) {
    std::vector<math::Vec2> kept;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const math::Vec2 current = polygon[index];
        const math::Vec2 next = polygon[(index + 1) % polygon.size()];
        const float currentSide = math::Vec2::dot(current, normal) - limit;
        const float nextSide = math::Vec2::dot(next, normal) - limit;
        if (currentSide <= 0.0F) {
            kept.push_back(current);
        }
        if ((currentSide < 0.0F) != (nextSide < 0.0F) && currentSide != nextSide) {
            kept.push_back(math::Vec2::lerp(current, next, currentSide / (currentSide - nextSide)));
        }
    }
    return kept;
}

float SceneTransition::getSwitchProgress() const noexcept {
    switch (options.kind) {
    case Kind::Fade:
    case Kind::FlipX:
    case Kind::FlipY:
    case Kind::ZoomFlip:
    case Kind::RotoZoom:
    case Kind::JumpZoom:
    case Kind::SplitColumns:
    case Kind::SplitRows:
    case Kind::Iris:
    case Kind::Pixelate:
        return 0.5F;
    case Kind::CrossFade:
    case Kind::MoveIn:
    case Kind::SlideIn:
    case Kind::Push:
    case Kind::ShrinkGrow:
    case Kind::TurnOffTiles:
    case Kind::FadeTiles:
    case Kind::PageTurn:
    case Kind::RadialClockwise:
    case Kind::RadialCounterclockwise:
    case Kind::Wipe:
    case Kind::InOut:
    case Kind::OutIn:
    case Kind::Dissolve:
        break;
    }
    return 0.0F;
}

float SceneTransition::getExitProgress() const noexcept {
    const float switchProgress = getSwitchProgress();
    return switchProgress > 0.0F ? switchProgress : 1.0F;
}

void SceneTransition::render(Renderer& renderer, const Frames& frames, float progress) {
    renderer.beginScreen();
    const math::Rect area = renderer.getCanvasBounds();
    const float first = std::clamp(progress * 2.0F, 0.0F, 1.0F);
    const float second = std::clamp(progress * 2.0F - 1.0F, 0.0F, 1.0F);
    const bool before = progress < 0.5F;
    const graphics::Texture& shown = before ? frames.outgoing : frames.incoming;

    switch (options.kind) {
    case Kind::Fade:
        drawImage(renderer, shown, area);
        renderer.drawRect(area, options.color.withAlpha(options.color.a * (before ? first : 1.0F - second)));
        return;
    case Kind::CrossFade:
        drawImage(renderer, frames.outgoing, area);
        drawImage(renderer, frames.incoming, area, math::Color{progress, progress, progress, progress});
        return;
    case Kind::MoveIn:
    case Kind::SlideIn:
    case Kind::Push:
        drawMotion(renderer, area, frames, progress);
        return;
    case Kind::ShrinkGrow:
        renderer.drawRect(area, options.color);
        drawTransformed(renderer, area, frames.outgoing, {2.0F / 3.0F, 0.5F}, 1.0F - progress, 0.0F);
        drawTransformed(renderer, area, frames.incoming, {1.0F / 3.0F, 0.5F}, progress, 0.0F);
        return;
    case Kind::FlipX:
    case Kind::FlipY:
    case Kind::ZoomFlip: {
        // The outgoing scene turns until it is edge on, and the incoming one turns the rest of the way from edge on.
        const bool vertical = options.kind == Kind::FlipY || (options.kind == Kind::ZoomFlip && isVertical(options.direction));
        const math::Vec2 steps = getSteps(options.direction);
        const float sign = (vertical ? steps.y : steps.x) < 0.0F ? -1.0F : 1.0F;
        const float angle = sign * math::Math::kHalfPi * (before ? first : second - 1.0F);
        const float scale = options.kind == Kind::ZoomFlip ? 1.0F - 0.5F * std::sin(math::Math::kPi * progress) : 1.0F;
        renderer.drawRect(area, options.color);
        drawTurn(renderer, area, shown, angle, vertical, scale);
        return;
    }
    case Kind::RotoZoom: {
        const float scale = before ? 1.0F - first : second;
        const float turns = before ? first * 2.0F : (second - 1.0F) * 2.0F;
        renderer.drawRect(area, options.color);
        drawTransformed(renderer, area, shown, {0.5F, 0.5F}, scale, turns * math::Math::kTau);
        return;
    }
    case Kind::JumpZoom:
        drawJump(renderer, area, frames, progress);
        return;
    case Kind::SplitColumns:
    case Kind::SplitRows:
        drawSplit(renderer, area, frames, progress, options.kind == Kind::SplitColumns);
        return;
    case Kind::TurnOffTiles:
    case Kind::FadeTiles:
        drawImage(renderer, frames.incoming, area);
        drawGrid(renderer, area, frames.outgoing, progress, options.kind == Kind::FadeTiles);
        return;
    case Kind::Wipe: {
        // The uncovered part is the unit square behind an edge that crosses it toward the direction.
        const math::Vec2 normal = getSteps(options.direction).getNormalized();
        const std::array<math::Vec2, 4> square{math::Vec2{0.0F, 0.0F}, math::Vec2{1.0F, 0.0F}, math::Vec2{1.0F, 1.0F}, math::Vec2{0.0F, 1.0F}};
        float low = math::Vec2::dot(square[0], normal);
        float high = low;
        for (const math::Vec2 corner : square) {
            low = std::min(low, math::Vec2::dot(corner, normal));
            high = std::max(high, math::Vec2::dot(corner, normal));
        }
        drawImage(renderer, frames.outgoing, area);
        drawRegion(renderer, area, frames.incoming, clip(square, normal, math::Math::lerp(low, high, progress)));
        return;
    }
    case Kind::InOut: {
        const math::Rect part = math::Rect::fromCenter({0.5F, 0.5F}, {progress, progress});
        drawImage(renderer, frames.outgoing, area);
        drawImage(renderer, frames.incoming, getPlace(area, part), math::Color::white(), part);
        return;
    }
    case Kind::OutIn: {
        const math::Rect part = math::Rect::fromCenter({0.5F, 0.5F}, {1.0F - progress, 1.0F - progress});
        drawImage(renderer, frames.incoming, area);
        drawImage(renderer, frames.outgoing, getPlace(area, part), math::Color::white(), part);
        return;
    }
    case Kind::PageTurn:
        drawBlend(renderer, area, frames, progress, ImageBlend::Pattern::PageTurn);
        return;
    case Kind::RadialClockwise:
    case Kind::RadialCounterclockwise:
        drawBlend(renderer, area, frames, progress, ImageBlend::Pattern::Radial);
        return;
    case Kind::Iris:
        drawBlend(renderer, area, frames, progress, ImageBlend::Pattern::Iris);
        return;
    case Kind::Dissolve:
        drawBlend(renderer, area, frames, progress, ImageBlend::Pattern::Dissolve);
        return;
    case Kind::Pixelate:
        drawBlend(renderer, area, frames, progress, ImageBlend::Pattern::Pixelate);
        return;
    }
}

void SceneTransition::drawImage(Renderer& renderer, const graphics::Texture& texture, const math::Rect& target, math::Color tint, const math::Rect& part) {
    renderer.draw({.texture = texture, .source = getSource(texture, part), .position = target.getMin(), .size = target.getSize(), .pivot = {}, .color = tint, .order = kCapturedOrder});
}

void SceneTransition::drawTransformed(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, math::Vec2 pivot, float scale, float rotation) {
    if (scale <= 0.0F) {
        return;
    }
    const math::Vec2 center = area.getMin() + area.getSize() * pivot;
    renderer.draw({.texture = texture, .source = getSource(texture, {0.0F, 0.0F, 1.0F, 1.0F}), .position = center, .size = area.getSize() * scale, .pivot = pivot, .rotation = rotation, .order = kCapturedOrder});
}

// Rotates the image in depth around its middle and projects it with a camera placed well in front of it, as a grid fine enough that each cell stays straight.
void SceneTransition::drawTurn(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, float angle, bool vertical, float scale) {
    const math::Vec2 half = area.getSize() * 0.5F;
    const math::Vec2 center = area.getCenter();
    const float distance = std::max(area.width, area.height) * 1.5F;
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);

    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(static_cast<std::size_t>((kTurnColumns + 1) * (kTurnRows + 1)));
    for (int column = 0; column <= kTurnColumns; ++column) {
        const float along = static_cast<float>(column) / static_cast<float>(kTurnColumns);
        const float reach = (along * 2.0F - 1.0F) * (vertical ? half.y : half.x);
        const float perspective = distance / (distance + reach * sine) * scale;
        for (int row = 0; row <= kTurnRows; ++row) {
            const float across = static_cast<float>(row) / static_cast<float>(kTurnRows);
            const float side = (across * 2.0F - 1.0F) * (vertical ? half.x : half.y);
            const math::Vec2 local = vertical ? math::Vec2{side, reach * cosine} : math::Vec2{reach * cosine, side};
            vertices.push_back({.position = center + local * perspective, .uv = vertical ? math::Vec2{across, along} : math::Vec2{along, across}});
            if (column > 0 && row > 0) {
                const auto corner = static_cast<std::uint32_t>(column * (kTurnRows + 1) + row);
                const auto before = corner - static_cast<std::uint32_t>(kTurnRows + 1);
                indices.insert(indices.end(), {before - 1U, before, corner, before - 1U, corner, corner - 1U});
            }
        }
    }
    renderer.drawMesh(texture, vertices, indices, kCapturedOrder);
}

void SceneTransition::drawRegion(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, std::span<const math::Vec2> polygon) {
    if (polygon.size() < 3) {
        return;
    }
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    for (const math::Vec2 point : polygon) {
        vertices.push_back({.position = area.getMin() + point * area.getSize(), .uv = point});
    }
    for (std::uint32_t corner = 2; corner < polygon.size(); ++corner) {
        indices.insert(indices.end(), {0U, corner - 1U, corner});
    }
    renderer.drawMesh(texture, vertices, indices, kCapturedOrder);
}

// Tiles turn off in random order, or shrink in a wave that crosses the screen toward the direction.
void SceneTransition::drawGrid(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, float progress, bool wave) const {
    const int columns = kTileColumns;
    const int rows = std::max(1, static_cast<int>(std::lround(static_cast<float>(columns) * area.height / area.width)));
    const math::Vec2 tile{1.0F / static_cast<float>(columns), 1.0F / static_cast<float>(rows)};
    const math::Vec2 normal = getSteps(options.direction).getNormalized();
    const float low = std::min(0.0F, normal.x) + std::min(0.0F, normal.y);
    const float span = std::abs(normal.x) + std::abs(normal.y);

    std::vector<SpriteInstance> tiles;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const math::Rect part{static_cast<float>(column) * tile.x, static_cast<float>(row) * tile.y, tile.x, tile.y};
            float scale = 1.0F;
            if (wave) {
                const float place = (math::Vec2::dot(part.getCenter(), normal) - low) / span;
                scale = 1.0F - math::Math::saturate((progress - place * 0.65F) / 0.35F);
            } else if (getHash(row * columns + column) < progress) {
                scale = 0.0F;
            }
            if (scale <= 0.0F) {
                continue;
            }
            const math::Vec2 center = area.getMin() + part.getCenter() * area.getSize();
            tiles.push_back({.position = center, .size = part.getSize() * area.getSize() * scale, .source = getSource(texture, part)});
        }
    }
    renderer.drawBatch(texture, tiles, kCapturedOrder);
}

// The outgoing strips leave in alternating directions and the incoming strips come back the same way.
void SceneTransition::drawSplit(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress, bool columns) const {
    const bool before = progress < 0.5F;
    const graphics::Texture& texture = before ? frames.outgoing : frames.incoming;
    const float travel = before ? progress * 2.0F : 2.0F - progress * 2.0F;
    const math::Vec2 steps = getSteps(options.direction);
    const float first = (columns ? steps.y : steps.x) < 0.0F ? -1.0F : 1.0F;

    std::vector<SpriteInstance> strips;
    for (int index = 0; index < kSplits; ++index) {
        const float start = static_cast<float>(index) / static_cast<float>(kSplits);
        const float length = 1.0F / static_cast<float>(kSplits);
        const math::Rect part = columns ? math::Rect{start, 0.0F, length, 1.0F} : math::Rect{0.0F, start, 1.0F, length};
        const float sign = index % 2 == 0 ? first : -first;
        const math::Vec2 shift = columns ? math::Vec2{0.0F, sign * travel * area.height} : math::Vec2{sign * travel * area.width, 0.0F};
        strips.push_back({.position = area.getMin() + part.getMin() * area.getSize() + shift, .size = part.getSize() * area.getSize(), .source = getSource(texture, part), .pivot = {}});
    }
    renderer.drawRect(area, options.color);
    renderer.drawBatch(texture, strips, kCapturedOrder);
}

// A quarter to shrink the outgoing scene, a quarter for it to jump out, a quarter for the incoming scene to jump in and a quarter to grow it.
void SceneTransition::drawJump(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress) const {
    const bool before = progress < 0.5F;
    const graphics::Texture& texture = before ? frames.outgoing : frames.incoming;
    const float phase = std::clamp(progress * 4.0F, 0.0F, 4.0F);
    const math::Vec2 steps = getSteps(options.direction);

    float scale = 0.5F;
    float travel = 0.0F;
    if (phase < 1.0F) {
        scale = 1.0F - phase * 0.5F;
    } else if (phase < 2.0F) {
        travel = phase - 1.0F;
    } else if (phase < 3.0F) {
        travel = phase - 3.0F;
    } else {
        scale = 0.5F + (phase - 3.0F) * 0.5F;
    }

    const float hop = std::abs(std::sin(travel * math::Math::kTau)) * area.height * 0.25F;
    const math::Vec2 center = area.getCenter() + steps * area.getSize() * travel - math::Vec2{0.0F, hop};
    renderer.drawRect(area, options.color);
    renderer.draw({.texture = texture, .source = getSource(texture, {0.0F, 0.0F, 1.0F, 1.0F}), .position = center, .size = area.getSize() * scale, .order = kCapturedOrder});
}

void SceneTransition::drawMotion(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress) const {
    const math::Vec2 travel = getSteps(options.direction) * area.getSize();
    const math::Rect incoming = area.translated(-travel * (1.0F - progress));
    if (options.kind == Kind::MoveIn) {
        drawImage(renderer, frames.outgoing, area);
    } else if (options.kind == Kind::SlideIn) {
        const float shade = 1.0F - 0.5F * progress;
        drawImage(renderer, frames.outgoing, area.translated(travel * progress / 3.0F), math::Color{shade, shade, shade, 1.0F});
    } else {
        drawImage(renderer, frames.outgoing, area.translated(travel * progress));
    }
    drawImage(renderer, frames.incoming, incoming);
}

// Every pattern reads only its own settings, so the blend carries all of them sized to the image.
void SceneTransition::drawBlend(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress, ImageBlend::Pattern pattern) const {
    const auto height = static_cast<float>(frames.outgoing.getHeight());
    renderer.drawImageBlend(
        {
            .pattern = pattern,
            .from = frames.outgoing,
            .to = frames.incoming,
            .area = area,
            .progress = progress,
            .cellSize = std::max(1.0F, height / 360.0F),
            .blockSize = std::max(4.0F, height / 12.0F),
            .color = options.color,
            .reversed = options.kind == Kind::RadialCounterclockwise,
            .angle = getSteps(options.direction).getAngle(),
        },
        kCapturedOrder);
}

} // namespace haylen::graphics2d
