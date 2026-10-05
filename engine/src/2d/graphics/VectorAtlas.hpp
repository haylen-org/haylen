#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {
class Device;
}

namespace haylen::graphics2d {

// Keeps the rasters of vector images in pages of one dynamic texture each, so the draws of many images at many sizes share a few textures and batch together. Scales come in steps a quarter of an octave apart, and every draw takes the raster of the step that covers its scale. The first raster of an image is made at once on the frame thread, so the image shows from its first draw, and every other step is made on the task pool while the nearest step the atlas holds draws in its place. Pages pack their rasters in shelves, a page that fills up starts another one, and once the atlas holds as many pages as it may, the page that drew longest ago starts over with a new texture.
class VectorAtlas final {
  public:
    struct Raster {
        graphics::Texture texture;
        math::Rect source;
    };

    static constexpr int kStepsPerOctave = 4;
    static constexpr int kPageSize = 1024;
    static constexpr std::size_t kMaxPages = 8;

    VectorAtlas(graphics::Device& graphicsDevice, core::JobSystem& jobSystem);
    ~VectorAtlas();

    VectorAtlas(const VectorAtlas&) = delete;
    VectorAtlas& operator=(const VectorAtlas&) = delete;

    // Returns the raster of an image for a scale in pixels per unit of the image, marking its page as drawn in the frame.
    [[nodiscard]] Raster find(const graphics::VectorImage& image, float scale, std::uint64_t frame);

    // Hands the pages whose pixels changed to the device, which sends them with the next upload.
    void stage();

    // Returns the step whose scale covers a scale, and the scale of a step.
    [[nodiscard]] static int stepOf(float scale) noexcept;
    [[nodiscard]] static float scaleOf(int step) noexcept;

    [[nodiscard]] std::size_t getPageCount() const noexcept {
        return pages.size();
    }

  private:
    struct Entry {
        std::size_t page = 0;
        math::Rect source;
    };

    struct Page {
        graphics::Texture texture;
        std::vector<std::uint8_t> pixels;
        int width = 0;
        int height = 0;
        int cursorX = 1;
        int cursorY = 1;
        int rowHeight = 0;
        std::uint64_t frame = 0;
        bool dirty = false;
        std::vector<std::pair<std::uint32_t, int>> rasters;
    };

    // Lets rasters finished on the task pool find the atlas only while it lives.
    struct Link {
        VectorAtlas* atlas = nullptr;
    };

    [[nodiscard]] int clampStep(const graphics::VectorImage& image, int step) const noexcept;
    void request(const graphics::VectorImage& image, int step);
    const Entry& place(std::uint32_t image, int step, const graphics::Image& raster, std::uint64_t frame);
    [[nodiscard]] std::size_t findRoom(int width, int height, std::uint64_t frame);
    void clearPage(std::size_t index, int side);

    graphics::Device& device;
    core::JobSystem& jobs;
    std::vector<Page> pages;
    std::unordered_map<std::uint32_t, std::map<int, Entry>> images;
    std::set<std::pair<std::uint32_t, int>> pending;
    std::shared_ptr<Link> link = std::make_shared<Link>(Link{this});
    std::uint64_t currentFrame = 0;
};

} // namespace haylen::graphics2d
