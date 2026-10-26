#include "haylen/graphics/Device.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include "graphics/DeviceState.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::graphics {

void Device::validateSize(int width, int height, int limit) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Texture dimensions must be positive.");
    }
    if (width > limit || height > limit) {
        throw std::invalid_argument("Texture dimensions exceed the device limit.");
    }
}

void Device::validateDistances(int width, int height, std::span<const std::uint16_t> distances, int limit) {
    validateSize(width, height, limit);
    if (distances.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        throw std::invalid_argument("The distances of a distance texture do not match its dimensions.");
    }
}

Device::Device(std::unique_ptr<gpu::Device> device) : state(std::make_unique<DeviceState>(std::move(device))) {
    state->white = createTexture(Image(1, 1, math::Color::white()));
}

Device::~Device() {
    state->white = {};
    state->graveyard->collect(*state->gpu);
    state->graveyard.reset();
    for (const auto& [key, sampler] : state->samplers) {
        state->gpu->destroy(sampler);
    }
}

gpu::Device& Device::getGpu() noexcept {
    return *state->gpu;
}

Texture Device::createTexture(const Image& image, Texture::Options options) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());

    auto resource = std::make_shared<TextureResource>();
    std::size_t bytes = 0;
    resource->image = state->createTextureImage(image, options.mipmaps, bytes);
    resource->tracked.setBytes(bytes);
    resource->sampler = state->getSampler(options);
    resource->width = image.getWidth();
    resource->height = image.getHeight();
    resource->id = state->nextTextureId++;
    resource->options = options;
    resource->graveyard = state->graveyard;
    return Texture(std::move(resource));
}

Texture Device::createTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createTexture(Image(width, height, fill), options);
}

const Texture& Device::getWhiteTexture() const noexcept {
    return state->white;
}

RenderTarget Device::createRenderTarget(int width, int height, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    if (options.mipmaps) {
        throw std::invalid_argument("A render target has no mipmaps.");
    }
    return state->createRenderTarget(width, height, state->offscreenFormat, options);
}

void Device::replaceTexture(const Texture& texture, const Image& image) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());
    const std::shared_ptr<TextureResource>& resource = texture.getResource();

    // The old image is released only once the new one exists, so a failed creation leaves the texture intact. A dynamic texture stays dynamic, and its new pixels reach the GPU with the next upload.
    std::size_t bytes = DeviceState::getImageBytes(image.getWidth(), image.getHeight(), gpu::Device::PixelFormat::Rgba8);
    const gpu::Device::Image created = resource->dynamic ? state->createDynamicImage(image.getWidth(), image.getHeight(), gpu::Device::PixelFormat::Rgba8) : state->createTextureImage(image, resource->options.mipmaps, bytes);
    state->graveyard->bury(resource->image);
    resource->image = created;
    resource->format = gpu::Device::PixelFormat::Rgba8;
    resource->width = image.getWidth();
    resource->height = image.getHeight();
    resource->tracked.setBytes(bytes);
    if (resource->dynamic) {
        state->stagePixels(resource, image.getPixels());
    }
}

void Device::readTarget(const RenderTarget& target, ReadCallback done) {
    if (!target.isValid()) {
        throw std::invalid_argument("A read needs a valid render target.");
    }
    state->reads.push_back({.target = target, .done = std::move(done)});
}

// A read starts its copy at the first call after the frame that asked for it, and hands its pixels to the task pool once the copy finished. Failed reads reach their callbacks after the walk, since a callback may ask for another read.
void Device::processReads(core::JobSystem& jobs) {
    std::vector<std::pair<ReadCallback, std::string>> failed;
    for (std::size_t index = 0; index < state->reads.size();) {
        DeviceState::PendingRead& read = state->reads[index];
        const TextureResource& resource = *read.target.getTexture().getResource();
        if (!read.copy) {
            try {
                read.copy = state->gpu->readImage(resource.image, resource.width, resource.height);
            } catch (const std::exception& error) {
                failed.emplace_back(std::move(read.done), error.what());
                state->reads.erase(state->reads.begin() + static_cast<std::ptrdiff_t>(index));
                continue;
            }
            ++index;
            continue;
        }

        std::optional<std::vector<std::uint8_t>> pixels;
        try {
            pixels = read.copy->poll();
        } catch (const std::exception& error) {
            failed.emplace_back(std::move(read.done), error.what());
            state->reads.erase(state->reads.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }
        if (!pixels) {
            ++index;
            continue;
        }
        const int width = resource.width;
        const int height = resource.height;
        const bool flipped = resource.flipped;
        ReadCallback done = std::move(read.done);
        state->reads.erase(state->reads.begin() + static_cast<std::ptrdiff_t>(index));
        // clang-format off
        jobs.run([copied = std::move(*pixels), width, height, flipped]() mutable {
            return Image::fromRendered(std::move(copied), width, height, flipped);
        }, [done = std::move(done)](core::JobSystem::Result<Image> result) {
            done(std::move(result.value), result.error);
        });
        // clang-format on
    }
    for (auto& [done, error] : failed) {
        done(std::nullopt, error);
    }
}

void Device::cancelReads() noexcept {
    state->reads.clear();
}

std::size_t Device::getPendingReadCount() const noexcept {
    return state->reads.size();
}

Texture Device::createDynamicTexture(const Image& image, Texture::Options options) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());
    if (options.mipmaps) {
        throw std::invalid_argument("A texture that changes in place has no mipmaps.");
    }
    return state->createDynamicTexture(image.getWidth(), image.getHeight(), gpu::Device::PixelFormat::Rgba8, image.getPixels(), options);
}

Texture Device::createDynamicTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createDynamicTexture(Image(width, height, fill), options);
}

// The texture takes the bytes of the halves as they lie in memory, which is how the GPU reads them.
Texture Device::createDynamicDistanceTexture(int width, int height, std::span<const std::uint16_t> distances, Texture::Options options) {
    validateDistances(width, height, distances, getMaxTextureSize());
    const std::span bytes(reinterpret_cast<const std::uint8_t*>(distances.data()), distances.size_bytes());
    return state->createDynamicTexture(width, height, gpu::Device::PixelFormat::R16Float, bytes, options);
}

void Device::updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels) {
    const std::shared_ptr<TextureResource>& resource = texture.getResource();
    if (!resource || !resource->dynamic) {
        throw std::logic_error("Only a dynamic texture changes its pixels in place.");
    }
    if (pixels.size() != DeviceState::getImageBytes(resource->width, resource->height, resource->format)) {
        throw std::invalid_argument("The pixels do not match the size of the dynamic texture.");
    }
    state->stagePixels(resource, pixels);
}

// Every texture uploads the last pixels it received, all of them in one hand-over to the GPU device before the passes of the frame.
std::size_t Device::uploadTextures(std::span<const gpu::Device::ImageWrite> frameImages) {
    ++state->uploads;
    std::swap(state->changedTextures, state->uploadingTextures);
    state->uploading.clear();
    state->writes.clear();
    std::size_t bytes = 0;
    for (const std::weak_ptr<TextureResource>& changed : state->uploadingTextures) {
        if (std::shared_ptr<TextureResource> resource = changed.lock()) {
            state->writes.push_back({.image = resource->image, .pixels = std::as_bytes(std::span(resource->staged))});
            bytes += resource->staged.size();
            resource->pending = false;
            resource->uploadedAt = state->uploads;
            state->uploading.push_back(std::move(resource));
        }
    }
    state->writes.insert(state->writes.end(), frameImages.begin(), frameImages.end());
    if (!state->writes.empty()) {
        state->gpu->writeImages(state->writes);
    }
    state->writes.clear();
    state->uploading.clear();

    // A texture that changed for the upload before but not for this one gives its staging memory back.
    for (const std::weak_ptr<TextureResource>& uploaded : state->uploadedTextures) {
        const std::shared_ptr<TextureResource> resource = uploaded.lock();
        if (resource && !resource->pending && resource->uploadedAt != state->uploads) {
            resource->staged = std::vector<std::uint8_t>();
        }
    }
    std::swap(state->uploadedTextures, state->uploadingTextures);
    state->uploadingTextures.clear();
    return bytes;
}

std::string_view Device::getApiName() const noexcept {
    const gpu::Device::Api api = state->gpu->getApi();
    for (const auto& [name, value] : gpu::Device::kApiNames) {
        if (value == api) {
            return name;
        }
    }
    return gpu::Device::kApiNames.front().first;
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    return state->gpu->getGpuFrameSeconds();
}

std::string Device::getAdapterName() const {
    return state->gpu->getAdapterName();
}

int Device::getMaxTextureSize() const noexcept {
    return state->gpu->getCapabilities().maxImageSize;
}

std::vector<Device::Pool> Device::getPools() const {
    std::vector<Pool> pools;
    for (const gpu::Device::Pool& pool : state->gpu->getPools()) {
        pools.push_back({.name = pool.name, .used = pool.used, .size = pool.size});
    }
    return pools;
}

void Device::collectGarbage() {
    state->graveyard->collect(*state->gpu);
}

} // namespace haylen::graphics
