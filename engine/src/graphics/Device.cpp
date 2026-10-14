#include "haylen/graphics/Device.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

#include "graphics/DeviceSetup.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/Gpu.hpp"
#include "graphics/GpuAdapter.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/JobSystem.hpp"

namespace haylen::graphics {

void Device::validateSize(int width, int height, int limit) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Texture dimensions must be positive.");
    }
    if (width > limit || height > limit) {
        throw std::invalid_argument("Texture dimensions exceed the device limit.");
    }
}

void Device::validateAlpha(int width, int height, std::span<const std::uint8_t> alpha, int limit) {
    validateSize(width, height, limit);
    if (alpha.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        throw std::invalid_argument("Alpha texture data does not match its dimensions.");
    }
}

Device::Device(const DeviceSetup& setup) : state(std::make_unique<DeviceState>()) {
    // Sokol keeps one global device, so a process can run only one engine at a time.
    if (sg_isvalid()) {
        throw std::logic_error("Only one graphics device can exist at a time.");
    }

    sg_desc desc{};
    desc.environment = setup.environment;
    desc.logger = setup.logger;
    desc.disable_validation = !setup.validation;
    desc.image_pool_size = Gpu::kImagePoolSize;
    desc.view_pool_size = Gpu::kViewPoolSize;
    desc.buffer_pool_size = Gpu::kBufferPoolSize;
    desc.shader_pool_size = Gpu::kShaderPoolSize;
    desc.pipeline_pool_size = Gpu::kPipelinePoolSize;
    sg_setup(&desc);
    if (!sg_isvalid()) {
        throw std::runtime_error("The graphics device could not be initialized.");
    }
    state->white = createTexture(Image(1, 1, math::Color::white()));
}

Device::~Device() {
    state->white = {};
    if (state->staging.id != SG_INVALID_ID) {
        state->graveyard->buryBuffer(state->staging);
    }
    state->graveyard->collect();
    state->graveyard.reset();
    for (const auto& [key, sampler] : state->samplers) {
        sg_destroy_sampler(sampler);
    }
    sg_shutdown();
}

Texture Device::createTexture(const Image& image, Texture::Options options) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());

    auto resource = std::make_shared<TextureResource>();
    std::size_t bytes = 0;
    const DeviceState::ImageViews created = DeviceState::createTextureImage(image, options.mipmaps, bytes);
    resource->image = created.image;
    resource->view = created.view;
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
    std::size_t bytes = DeviceState::getImageBytes(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8);
    const DeviceState::ImageViews created = resource->dynamic ? DeviceState::createDynamicImage(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8) : DeviceState::createTextureImage(image, resource->options.mipmaps, bytes);
    state->graveyard->bury(resource->image, resource->view);
    resource->image = created.image;
    resource->view = created.view;
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
                read.copy = std::make_unique<GpuReadback>(resource.image, resource.width, resource.height);
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
    return state->createDynamicTexture(image.getWidth(), image.getHeight(), SG_PIXELFORMAT_RGBA8, image.getPixels(), options);
}

Texture Device::createDynamicTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createDynamicTexture(Image(width, height, fill), options);
}

Texture Device::createDynamicAlphaTexture(int width, int height, std::span<const std::uint8_t> alpha, Texture::Options options) {
    validateAlpha(width, height, alpha, getMaxTextureSize());
    return state->createDynamicTexture(width, height, SG_PIXELFORMAT_R8, alpha, options);
}

void Device::updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels) {
    const std::shared_ptr<TextureResource>& resource = texture.getResource();
    if (!resource || !resource->dynamic) {
        throw std::logic_error("Only a dynamic texture changes its pixels in place.");
    }
    if (pixels.size() != DeviceState::getImageBytes(resource->width, resource->height, sg_query_image_desc(resource->image).pixel_format)) {
        throw std::invalid_argument("The pixels do not match the size of the dynamic texture.");
    }
    state->stagePixels(resource, pixels);
}

// Every texture uploads the last pixels it received. Sokol takes every write into the staging buffer before the first copy out of it in a frame, so the pixels of all the textures go in before any of them is copied into its image.
std::size_t Device::uploadTextures() {
    ++state->uploads;
    std::swap(state->changedTextures, state->uploadingTextures);
    std::size_t staged = 0;
    for (const std::weak_ptr<TextureResource>& changed : state->uploadingTextures) {
        if (const std::shared_ptr<TextureResource> resource = changed.lock()) {
            staged += DeviceState::getStagingBytes(*resource);
        }
    }
    state->reserveStaging(staged);

    std::size_t offset = 0;
    for (const std::weak_ptr<TextureResource>& changed : state->uploadingTextures) {
        if (const std::shared_ptr<TextureResource> resource = changed.lock()) {
            sg_write_buffer_desc write{};
            write.src.data = {.ptr = resource->staged.data(), .size = resource->staged.size()};
            write.dst = {.buffer = state->staging, .offset = offset};
            sg_write_buffer_transient(&write);
            offset += DeviceState::getStagingBytes(*resource);
        }
    }

    std::size_t bytes = 0;
    offset = 0;
    for (const std::weak_ptr<TextureResource>& changed : state->uploadingTextures) {
        const std::shared_ptr<TextureResource> resource = changed.lock();
        if (!resource) {
            continue;
        }
        sg_copy_buffer_to_image_desc copy{};
        copy.src.buffer = state->staging;
        copy.src.offset = offset;
        copy.src.bytes_per_row = static_cast<int>(resource->staged.size()) / resource->height;
        copy.src.bytes_per_slice = static_cast<int>(resource->staged.size());
        copy.dst.image = resource->image;
        copy.size = {.width = resource->width, .height = resource->height, .num_slices = 1};
        sg_copy_buffer_to_image(&copy);
        offset += DeviceState::getStagingBytes(*resource);
        bytes += resource->staged.size();
        resource->pending = false;
        resource->uploadedAt = state->uploads;
    }

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

std::string_view Device::getBackendName() const noexcept {
    switch (sg_query_backend()) {
    case SG_BACKEND_GLCORE:
        return "glcore";
    case SG_BACKEND_GLES3:
        return "gles3";
    case SG_BACKEND_D3D11:
        return "d3d11";
    case SG_BACKEND_METAL_IOS:
    case SG_BACKEND_METAL_MACOS:
    case SG_BACKEND_METAL_SIMULATOR:
        return "metal";
    case SG_BACKEND_WGPU:
        return "webgpu";
    case SG_BACKEND_VULKAN:
        return "vulkan";
    case SG_BACKEND_DUMMY:
        return "dummy";
    }
    return "unknown";
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    const sg_backend backend = sg_query_backend();
    if (backend != SG_BACKEND_METAL_MACOS && backend != SG_BACKEND_METAL_IOS && backend != SG_BACKEND_METAL_SIMULATOR) {
        return std::nullopt;
    }
    const std::uint64_t nanoseconds = sg_mtl_gpu_frame_nanoseconds();
    return nanoseconds == 0 ? std::nullopt : std::optional<double>(static_cast<double>(nanoseconds) * 1e-9);
}

std::string Device::getAdapterName() const {
    return GpuAdapter::getName();
}

int Device::getMaxTextureSize() const noexcept {
    return sg_query_limits().max_image_size_2d;
}

std::vector<Device::Pool> Device::getPools() const {
    const sg_stats stats = sg_query_stats();
    const sg_desc desc = sg_query_desc();
    return {
        {.name = "images", .used = static_cast<int>(stats.total.images.alive), .size = desc.image_pool_size}, {.name = "views", .used = static_cast<int>(stats.total.views.alive), .size = desc.view_pool_size}, {.name = "buffers", .used = static_cast<int>(stats.total.buffers.alive), .size = desc.buffer_pool_size}, {.name = "samplers", .used = static_cast<int>(stats.total.samplers.alive), .size = desc.sampler_pool_size}, {.name = "shaders", .used = static_cast<int>(stats.total.shaders.alive), .size = desc.shader_pool_size}, {.name = "pipelines", .used = static_cast<int>(stats.total.pipelines.alive), .size = desc.pipeline_pool_size},
    };
}

void Device::collectGarbage() {
    state->graveyard->collect();
}

} // namespace haylen::graphics
