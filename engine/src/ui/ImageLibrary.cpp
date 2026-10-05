#include "ui/ImageLibrary.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::ui {

ImageLibrary::ImageLibrary(assets::Manager& assetManager, graphics::Device& graphicsDevice, core::JobSystem& jobSystem, std::size_t budget) : assets(assetManager), device(graphicsDevice), jobs(jobSystem), rasterBudget(budget) {}

bool ImageLibrary::isVector(std::string_view path) noexcept {
    constexpr std::string_view svg = ".svg";
    const std::string_view extension = io::Path::extension(path);
    return std::ranges::equal(extension, svg, [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == b; });
}

int ImageLibrary::getStep(float scale) noexcept {
    return static_cast<int>(std::ceil(std::log2(scale) * static_cast<float>(kStepsPerOctave) - 0.001F));
}

float ImageLibrary::getStepScale(int step) noexcept {
    return std::exp2(static_cast<float>(step) / static_cast<float>(kStepsPerOctave));
}

graphics::Texture ImageLibrary::getTexture(std::string_view path, graphics::Texture::Filter filter, float scale) {
    if (!isVector(path)) {
        return requestFile(path, filter).texture;
    }
    Vector& vector = requestVector(path);
    if (!vector.image.isValid()) {
        return {};
    }

    const int step = clampStep(vector.image, getStep(std::max(scale, 1.0F / 64.0F)));
    const auto exact = vector.rasters.find(step);
    if (exact != vector.rasters.end()) {
        exact->second.frame = frame;
        return exact->second.texture;
    }
    requestRaster(std::string(path), vector, step);
    if (vector.rasters.empty()) {
        return {};
    }

    // Until the raster of the step arrives, the nearest step the picture has draws in its place.
    const auto above = vector.rasters.lower_bound(step);
    const auto below = above == vector.rasters.begin() ? above : std::prev(above);
    Raster& nearest = above == vector.rasters.end() || step - below->first <= above->first - step ? below->second : above->second;
    nearest.frame = frame;
    return nearest.texture;
}

math::Vec2 ImageLibrary::getSize(std::string_view path, graphics::Texture::Filter filter) {
    if (!isVector(path)) {
        const graphics::Texture& texture = requestFile(path, filter).texture;
        return texture.isValid() ? texture.getSize() : math::Vec2{};
    }
    const Vector& vector = requestVector(path);
    return vector.image.isValid() ? vector.image.getSize() : math::Vec2{};
}

void ImageLibrary::beginFrame() {
    ++frame;
    if (rasterBytes > rasterBudget) {
        release();
    }
}

// Image files load in the background with a filter, and each filter keeps its own textures, so a theme with another image filter loads the pictures again with it.
ImageLibrary::File& ImageLibrary::requestFile(std::string_view path, graphics::Texture::Filter filter) {
    std::map<std::string, File, std::less<>>& loaded = files[static_cast<std::size_t>(filter)];
    if (const auto found = loaded.find(path); found != loaded.end()) {
        if (!found->second.error.empty()) {
            throw std::runtime_error("The UI image \"" + std::string(path) + "\" could not be loaded. " + found->second.error);
        }
        return found->second;
    }
    File& file = loaded.emplace(std::string(path), File{}).first->second;
    // clang-format off
    assets.textureAsync(path, [this, owner = std::weak_ptr<bool>(alive), key = std::string(path), filter](graphics::Texture texture, std::string error) {
        if (owner.expired()) {
            return;
        }
        File& entry = files[static_cast<std::size_t>(filter)][key];
        entry.texture = std::move(texture);
        entry.error = std::move(error);
    }, {.filter = filter, .wrap = graphics::Texture::Wrap::Clamp});
    // clang-format on
    return file;
}

ImageLibrary::Vector& ImageLibrary::requestVector(std::string_view path) {
    if (const auto found = vectors.find(path); found != vectors.end()) {
        if (!found->second.error.empty()) {
            throw std::runtime_error("The UI image \"" + std::string(path) + "\" could not be loaded. " + found->second.error);
        }
        return found->second;
    }
    Vector& vector = vectors.emplace(std::string(path), Vector{}).first->second;
    // clang-format off
    assets.loadAsync("vectorImage", path, [this, owner = std::weak_ptr<bool>(alive), key = std::string(path)](std::shared_ptr<void> asset, std::string error) {
        if (owner.expired()) {
            return;
        }
        Vector& entry = vectors[key];
        entry.image = asset ? graphics::VectorImage(std::static_pointer_cast<graphics::VectorImageResource>(asset)) : graphics::VectorImage{};
        entry.error = std::move(error);
    });
    // clang-format on
    return vector;
}

void ImageLibrary::requestRaster(const std::string& path, Vector& vector, int step) {
    if (!vector.pending.insert(step).second) {
        return;
    }
    // clang-format off
    jobs.run([image = vector.image, step] { return image.rasterize(getStepScale(step)); }, [this, owner = std::weak_ptr<bool>(alive), path, step](core::JobSystem::Result<graphics::Image> result) {
        const auto found = owner.expired() ? vectors.end() : vectors.find(path);
        if (found == vectors.end()) {
            return;
        }
        Vector& entry = found->second;
        entry.pending.erase(step);
        if (!result.isOk()) {
            entry.error = std::move(result.error);
            return;
        }
        const graphics::Image& pixels = *result.value;
        Raster raster{.texture = device.createTexture(pixels, {.filter = graphics::Texture::Filter::Linear, .wrap = graphics::Texture::Wrap::Clamp}), .bytes = pixels.getPixels().size(), .frame = frame};
        rasterBytes += raster.bytes;
        entry.rasters.insert_or_assign(step, std::move(raster));
    });
    // clang-format on
}

// A raster never outgrows the largest texture of the device.
int ImageLibrary::clampStep(const graphics::VectorImage& image, int step) const noexcept {
    const math::Vec2 size = image.getSize();
    const float largest = static_cast<float>(device.getMaxTextureSize());
    return std::min(step, static_cast<int>(std::floor(std::log2(largest / std::max(size.x, size.y)) * static_cast<float>(kStepsPerOctave))));
}

// The rasters drawn longest ago go first.
void ImageLibrary::release() {
    struct Candidate {
        std::uint64_t frame;
        Vector* vector;
        int step;
    };
    std::vector<Candidate> candidates;
    for (auto& [path, vector] : vectors) {
        for (const auto& [step, raster] : vector.rasters) {
            if (raster.frame + 1 < frame) {
                candidates.push_back({raster.frame, &vector, step});
            }
        }
    }
    std::ranges::sort(candidates, {}, &Candidate::frame);
    for (const Candidate& candidate : candidates) {
        if (rasterBytes <= rasterBudget) {
            return;
        }
        const auto raster = candidate.vector->rasters.find(candidate.step);
        rasterBytes -= raster->second.bytes;
        candidate.vector->rasters.erase(raster);
    }
}

} // namespace haylen::ui
