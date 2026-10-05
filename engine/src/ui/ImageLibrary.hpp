#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::assets {
class Manager;
}

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {
class Device;
}

namespace haylen::ui {

// Loads the pictures of the UI in the background. Image files become textures with the filter a theme asks for, and SVG documents become rasters of the size they draw at, made on the job system in steps a quarter of an octave apart, so a picture stays sharp at any size and density. Rasters that no frame drew lately go once the rasters pass their memory budget.
class ImageLibrary final {
  public:
    static constexpr int kStepsPerOctave = 4;
    static constexpr std::size_t kRasterBudget = std::size_t{64} * 1024U * 1024U;

    ImageLibrary(assets::Manager& assetManager, graphics::Device& graphicsDevice, core::JobSystem& jobSystem, std::size_t budget = kRasterBudget);

    ImageLibrary(const ImageLibrary&) = delete;
    ImageLibrary& operator=(const ImageLibrary&) = delete;

    // Returns the texture of a picture drawn at `scale` pixels per unit of its natural size, which image files ignore, or an empty texture while it loads. An SVG document draws with the nearest raster it has until the raster of the scale arrives. Throws once the picture failed to load.
    [[nodiscard]] graphics::Texture getTexture(std::string_view path, graphics::Texture::Filter filter, float scale);

    // The natural size of a picture, the pixels of an image file or the size an SVG document gives itself, which is zero while it loads. Throws once the picture failed to load.
    [[nodiscard]] math::Vec2 getSize(std::string_view path, graphics::Texture::Filter filter);

    // Counts the frames that tell which rasters were drawn lately, and releases the rasters drawn longest ago while the rasters pass the budget, keeping those of this frame and the one before.
    void beginFrame();

    [[nodiscard]] static bool isVector(std::string_view path) noexcept;
    [[nodiscard]] static int getStep(float scale) noexcept;
    [[nodiscard]] static float getStepScale(int step) noexcept;

    [[nodiscard]] std::size_t getRasterBytes() const noexcept {
        return rasterBytes;
    }

  private:
    struct File {
        graphics::Texture texture;
        std::string error;
    };

    struct Raster {
        graphics::Texture texture;
        std::size_t bytes = 0;
        std::uint64_t frame = 0;
    };

    struct Vector {
        graphics::VectorImage image;
        std::string error;
        std::map<int, Raster> rasters;
        std::set<int> pending;
    };

    [[nodiscard]] File& requestFile(std::string_view path, graphics::Texture::Filter filter);
    [[nodiscard]] Vector& requestVector(std::string_view path);
    void requestRaster(const std::string& path, Vector& vector, int step);
    [[nodiscard]] int clampStep(const graphics::VectorImage& image, int step) const noexcept;
    void release();

    assets::Manager& assets;
    graphics::Device& device;
    core::JobSystem& jobs;
    std::array<std::map<std::string, File, std::less<>>, 2> files;
    std::map<std::string, Vector, std::less<>> vectors;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    std::size_t rasterBudget;
    std::size_t rasterBytes = 0;
    std::uint64_t frame = 1;
};

} // namespace haylen::ui
