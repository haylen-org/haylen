#include "haylen/2d/animation/AtlasBuilder.hpp"

#include <algorithm>
#include <climits>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace haylen::animation2d {

void AtlasBuilder::add(std::string name, std::shared_ptr<const graphics::Image> image, std::optional<graphics::Image::Area> area) {
    if (!image || image->isEmpty()) {
        throw std::invalid_argument("The frame \"" + name + "\" of an atlas needs an image.");
    }
    const graphics::Image::Area region = area.value_or(image->getArea());
    if (region.x < 0 || region.y < 0 || region.width < 1 || region.height < 1 || region.x + region.width > image->getWidth() || region.y + region.height > image->getHeight()) {
        throw std::invalid_argument("The area of the frame \"" + name + "\" of an atlas is not inside its image.");
    }
    if (!names.emplace(name, entries.size()).second) {
        throw std::invalid_argument("The atlas has the frame \"" + name + "\" twice.");
    }
    entries.push_back({.name = std::move(name), .image = std::move(image), .area = region, .frame = {.originalSize = {static_cast<float>(region.width), static_cast<float>(region.height)}}});
}

// The animations of the atlas show regions its frames hold, so each frame of an animation becomes the entry of the frame with the same region.
void AtlasBuilder::addAtlas(std::string_view prefix, const SpriteAtlas& atlas, std::shared_ptr<const graphics::Image> image) {
    if (!image || image->getWidth() != static_cast<int>(atlas.getTexture().getSize().x) || image->getHeight() != static_cast<int>(atlas.getTexture().getSize().y)) {
        throw std::invalid_argument("The image of an atlas added to an atlas builder has the size of the texture of the atlas.");
    }
    std::map<std::tuple<int, int, int, int>, std::size_t> bySource;
    for (const std::string& frameName : atlas.getFrameNames()) {
        const SpriteFrame& frame = atlas.getFrame(frameName);
        const graphics::Image::Area area{static_cast<int>(std::lround(frame.source.x)), static_cast<int>(std::lround(frame.source.y)), static_cast<int>(std::lround(frame.source.width)), static_cast<int>(std::lround(frame.source.height))};
        add(std::string(prefix) + frameName, image, area);
        SpriteFrame& kept = entries.back().frame;
        kept = frame;
        kept.originalSize = frame.originalSize.isZero() ? frame.source.getSize() : frame.originalSize;
        bySource.emplace(std::tuple{area.x, area.y, area.width, area.height}, entries.size() - 1);
    }

    for (const std::string& animationName : atlas.getAnimationNames()) {
        const Animation& animation = atlas.getAnimation(animationName);
        std::vector<std::size_t> frameEntries;
        for (const SpriteFrame& frame : animation.frames) {
            const auto found = bySource.find({static_cast<int>(std::lround(frame.source.x)), static_cast<int>(std::lround(frame.source.y)), static_cast<int>(std::lround(frame.source.width)), static_cast<int>(std::lround(frame.source.height))});
            if (found == bySource.end()) {
                throw std::invalid_argument("The animation \"" + animationName + "\" of an atlas shows a region that no frame of the atlas holds.");
            }
            frameEntries.push_back(found->second);
        }
        animations.emplace_back(std::string(prefix) + animationName, animation);
        animationEntries.push_back(std::move(frameEntries));
    }
}

graphics::Image::Area AtlasBuilder::trimmed(const graphics::Image& image, const graphics::Image::Area& area) {
    int left = area.x + area.width;
    int top = area.y + area.height;
    int right = area.x - 1;
    int bottom = area.y - 1;
    for (int row = area.y; row < area.y + area.height; ++row) {
        for (int column = area.x; column < area.x + area.width; ++column) {
            if (image.getPixels()[static_cast<std::size_t>(row * image.getWidth() + column) * 4U + 3U] > 0) {
                left = std::min(left, column);
                right = std::max(right, column);
                top = std::min(top, row);
                bottom = std::max(bottom, row);
            }
        }
    }
    return right < left ? graphics::Image::Area{area.x, area.y, 1, 1} : graphics::Image::Area{left, top, right - left + 1, bottom - top + 1};
}

// Places each cell, tallest first, at the lowest place along the skyline where it fits, and returns the height the cells reach, or nothing when a cell is wider than the width.
std::optional<int> AtlasBuilder::skyline(const std::vector<graphics::Image::Area>& cells, const std::vector<std::size_t>& order, int width, std::vector<Placement>& placements) {
    std::vector<Span> spans{{0, 0, width}};
    int reach = 0;
    for (const std::size_t cell : order) {
        const int cellWidth = cells[cell].width;
        const int cellHeight = cells[cell].height;
        if (cellWidth > width) {
            return std::nullopt;
        }

        std::size_t chosen = spans.size();
        int chosenX = 0;
        int chosenY = INT_MAX;
        for (std::size_t start = 0; start < spans.size() && spans[start].x + cellWidth <= width; ++start) {
            int y = 0;
            for (std::size_t covered = start; covered < spans.size() && spans[covered].x < spans[start].x + cellWidth; ++covered) {
                y = std::max(y, spans[covered].y);
            }
            if (y < chosenY) {
                chosen = start;
                chosenX = spans[start].x;
                chosenY = y;
            }
        }
        placements[cell].x = chosenX;
        placements[cell].y = chosenY;
        reach = std::max(reach, chosenY + cellHeight);

        // The cell becomes a span of its own, and the spans it covers shrink or go.
        const int end = chosenX + cellWidth;
        spans.insert(spans.begin() + static_cast<std::ptrdiff_t>(chosen), {chosenX, chosenY + cellHeight, cellWidth});
        std::size_t next = chosen + 1;
        while (next < spans.size() && spans[next].x < end) {
            const int overlap = end - spans[next].x;
            if (overlap >= spans[next].width) {
                spans.erase(spans.begin() + static_cast<std::ptrdiff_t>(next));
                continue;
            }
            spans[next].x += overlap;
            spans[next].width -= overlap;
            break;
        }
        for (std::size_t index = 1; index < spans.size();) {
            if (spans[index].y == spans[index - 1].y) {
                spans[index - 1].width += spans[index].width;
                spans.erase(spans.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
        }
    }
    return reach;
}

AtlasBuilder::Packed AtlasBuilder::pack(const Options& options) const {
    if (options.padding < 0 || options.extrude < 0 || options.extrude > options.padding / 2 || options.maxSize < 1) {
        throw std::invalid_argument("An atlas needs a padding of zero or more, an extrusion of at most half the padding and a positive largest size.");
    }
    if (entries.empty()) {
        throw std::invalid_argument("An atlas needs at least one image.");
    }

    // Entries that show the same pixels share one cell.
    std::vector<graphics::Image::Area> areas;
    std::vector<const graphics::Image*> sources;
    std::vector<std::size_t> cellOf(entries.size());
    std::map<Key, std::size_t> cellsByKey;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const Entry& entry = entries[index];
        const graphics::Image::Area area = options.trim ? trimmed(*entry.image, entry.area) : entry.area;
        const auto [found, added] = cellsByKey.emplace(Key{entry.image.get(), area.x, area.y, area.width, area.height}, areas.size());
        if (added) {
            areas.push_back(area);
            sources.push_back(entry.image.get());
        }
        cellOf[index] = found->second;
    }

    std::vector<graphics::Image::Area> cells;
    int widest = 0;
    for (const graphics::Image::Area& area : areas) {
        cells.push_back({0, 0, area.width + options.padding, area.height + options.padding});
        widest = std::max(widest, cells.back().width);
    }
    std::vector<std::size_t> order(cells.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::ranges::sort(order, [&cells](std::size_t lhs, std::size_t rhs) { return cells[lhs].height != cells[rhs].height ? cells[lhs].height > cells[rhs].height : cells[lhs].width > cells[rhs].width; });

    // The width that gives the smallest square around the cells wins, then the smallest area.
    std::vector<Placement> placements(cells.size());
    std::vector<Placement> best;
    int bestWidth = 0;
    int bestHeight = 0;
    for (int width = std::min(64, options.maxSize);; width = std::min(width * 2, options.maxSize)) {
        const std::optional<int> height = width >= widest ? skyline(cells, order, width, placements) : std::nullopt;
        if (height && *height <= options.maxSize) {
            const int side = std::max(width, *height);
            const int bestSide = std::max(bestWidth, bestHeight);
            if (best.empty() || side < bestSide || (side == bestSide && width * *height < bestWidth * bestHeight)) {
                best = placements;
                bestWidth = width;
                bestHeight = *height;
            }
        }
        if (width == options.maxSize) {
            break;
        }
    }
    if (best.empty()) {
        throw std::runtime_error("The images of the atlas need more room than a texture of " + std::to_string(options.maxSize) + " pixels.");
    }

    Packed packed{.image = graphics::Image(bestWidth, bestHeight)};
    const int margin = options.padding / 2;
    for (std::size_t cell = 0; cell < areas.size(); ++cell) {
        const graphics::Image::Area& area = areas[cell];
        const graphics::Image::Area placed{best[cell].x + margin, best[cell].y + margin, area.width, area.height};
        packed.image.blit(*sources[cell], area, placed.x, placed.y);
        packed.image.extrude(placed, options.extrude);
        best[cell].area = placed;
    }
    if (options.texture.filter == graphics::Texture::Filter::Linear) {
        packed.image.bleed();
    }
    if (options.texture.mipmaps) {
        packed.image.buildMipmaps();
    }

    // A trimmed frame keeps where its pixels sat in its untrimmed frame.
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const Entry& entry = entries[index];
        const std::size_t cell = cellOf[index];
        const graphics::Image::Area& placed = best[cell].area;
        SpriteFrame frame = entry.frame;
        frame.source = {static_cast<float>(placed.x), static_cast<float>(placed.y), static_cast<float>(placed.width), static_cast<float>(placed.height)};
        frame.offset += math::Vec2{static_cast<float>(areas[cell].x - entry.area.x), static_cast<float>(areas[cell].y - entry.area.y)};
        packed.frames.emplace_back(entry.name, std::move(frame));
    }
    for (std::size_t index = 0; index < animations.size(); ++index) {
        Animation animation = animations[index].second;
        for (std::size_t frame = 0; frame < animation.frames.size(); ++frame) {
            const SpriteFrame& placed = packed.frames[animationEntries[index][frame]].second;
            animation.frames[frame].source = placed.source;
            animation.frames[frame].offset = placed.offset;
            animation.frames[frame].originalSize = placed.originalSize;
        }
        animation.texture = {};
        packed.animations.emplace_back(animations[index].first, std::move(animation));
    }
    return packed;
}

SpriteAtlas AtlasBuilder::finish(Packed packed, graphics::Texture texture) {
    return SpriteAtlas::assemble(std::move(texture), std::move(packed.frames), std::move(packed.animations));
}

} // namespace haylen::animation2d
