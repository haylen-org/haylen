#include "2d/graphics/VectorAtlas.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <span>

#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"

namespace haylen::graphics2d {

VectorAtlas::VectorAtlas(graphics::Device& graphicsDevice, core::JobSystem& jobSystem) : device(graphicsDevice), jobs(jobSystem) {}

VectorAtlas::~VectorAtlas() {
    link->atlas = nullptr;
}

int VectorAtlas::stepOf(float scale) noexcept {
    return static_cast<int>(std::ceil(std::log2(scale) * static_cast<float>(kStepsPerOctave) - 0.001F));
}

float VectorAtlas::scaleOf(int step) noexcept {
    return std::exp2(static_cast<float>(step) / static_cast<float>(kStepsPerOctave));
}

// A raster never outgrows the largest texture of the device, with a pixel to spare on each side.
int VectorAtlas::clampStep(const graphics::VectorImage& image, int step) const noexcept {
    const math::Vec2 size = image.getSize();
    const float largest = static_cast<float>(device.getMaxTextureSize() - 2);
    return std::min(step, static_cast<int>(std::floor(std::log2(largest / std::max(size.x, size.y)) * static_cast<float>(kStepsPerOctave))));
}

VectorAtlas::Raster VectorAtlas::find(const graphics::VectorImage& image, float scale, std::uint64_t frame) {
    currentFrame = frame;
    const int step = clampStep(image, stepOf(scale));
    const auto known = images.find(image.getId());
    const Entry* entry = nullptr;
    if (known == images.end()) {
        entry = &place(image.getId(), step, image.rasterize(scaleOf(step)), frame);
    } else if (const auto exact = known->second.find(step); exact != known->second.end()) {
        entry = &exact->second;
    } else {
        // Until the raster of the step arrives, the nearest step the atlas holds draws in its place.
        request(image, step);
        const std::map<int, Entry>& steps = known->second;
        const auto above = steps.lower_bound(step);
        const auto below = above == steps.begin() ? above : std::prev(above);
        entry = above == steps.end() || step - below->first <= above->first - step ? &below->second : &above->second;
    }

    Page& page = pages[entry->page];
    page.frame = frame;
    return {page.texture, entry->source};
}

void VectorAtlas::request(const graphics::VectorImage& image, int step) {
    if (!pending.emplace(image.getId(), step).second) {
        return;
    }
    // clang-format off
    jobs.post([owner = std::weak_ptr<Link>(link), image, step, queue = &jobs] {
        graphics::Image raster = image.rasterize(scaleOf(step));
        queue->postToFrame([owner, id = image.getId(), step, raster = std::move(raster)] {
            const std::shared_ptr<Link> alive = owner.lock();
            if (alive && alive->atlas != nullptr) {
                VectorAtlas& atlas = *alive->atlas;
                atlas.pending.erase({id, step});
                atlas.place(id, step, raster, atlas.currentFrame);
            }
        });
    });
    // clang-format on
}

const VectorAtlas::Entry& VectorAtlas::place(std::uint32_t image, int step, const graphics::Image& raster, std::uint64_t frame) {
    const int width = raster.getWidth();
    const int height = raster.getHeight();
    const std::size_t index = findRoom(width, height, frame);
    Page& page = pages[index];

    const std::span<const std::uint8_t> source = raster.getPixels();
    const auto rowBytes = static_cast<std::size_t>(width) * 4U;
    for (int row = 0; row < height; ++row) {
        const std::size_t from = static_cast<std::size_t>(row) * rowBytes;
        const std::size_t to = (static_cast<std::size_t>(page.cursorY + row) * static_cast<std::size_t>(page.width) + static_cast<std::size_t>(page.cursorX)) * 4U;
        std::copy_n(source.begin() + static_cast<std::ptrdiff_t>(from), rowBytes, page.pixels.begin() + static_cast<std::ptrdiff_t>(to));
    }

    const Entry entry{.page = index, .source = {static_cast<float>(page.cursorX), static_cast<float>(page.cursorY), static_cast<float>(width), static_cast<float>(height)}};
    page.cursorX += width + 1;
    page.rowHeight = std::max(page.rowHeight, height);
    page.dirty = true;
    page.rasters.emplace_back(image, step);
    return images[image].insert_or_assign(step, entry).first->second;
}

// A raster goes on the current shelf of the first page with room, or starts a new shelf there. Otherwise a new page starts, or, once the atlas holds as many pages as it may, the page that drew longest ago starts over.
std::size_t VectorAtlas::findRoom(int width, int height, std::uint64_t frame) {
    for (std::size_t index = 0; index < pages.size(); ++index) {
        Page& page = pages[index];
        if (page.cursorX + width + 1 <= page.width && page.cursorY + height + 1 <= page.height) {
            return index;
        }
        if (width + 2 <= page.width && page.cursorY + page.rowHeight + height + 2 <= page.height) {
            page.cursorX = 1;
            page.cursorY += page.rowHeight + 1;
            page.rowHeight = 0;
            return index;
        }
    }

    std::size_t index = pages.size();
    if (pages.size() >= kMaxPages) {
        const auto oldest = std::min_element(pages.begin(), pages.end(), [](const Page& lhs, const Page& rhs) { return lhs.frame < rhs.frame; });
        if (oldest->frame != frame) {
            index = static_cast<std::size_t>(oldest - pages.begin());
        }
    }
    if (index == pages.size()) {
        pages.emplace_back();
    }
    clearPage(index, std::min(device.getMaxTextureSize(), std::max({kPageSize, width + 2, height + 2})));
    return index;
}

// A page that starts over takes a new texture, so the draws of the frame keep the image their rasters were placed on.
void VectorAtlas::clearPage(std::size_t index, int side) {
    Page& page = pages[index];
    for (const auto& [image, step] : page.rasters) {
        const auto found = images.find(image);
        found->second.erase(step);
        if (found->second.empty()) {
            images.erase(found);
        }
    }
    page = Page{.pixels = std::vector<std::uint8_t>(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * 4U, 0), .width = side, .height = side};
    page.texture = device.createDynamicTexture(graphics::Image(side, side, page.pixels), {.filter = graphics::Texture::Filter::Linear});
}

void VectorAtlas::stage() {
    for (Page& page : pages) {
        if (page.dirty) {
            device.updateTexture(page.texture, page.pixels);
            page.dirty = false;
        }
    }
}

} // namespace haylen::graphics2d
