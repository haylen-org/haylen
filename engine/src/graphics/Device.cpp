#include "haylen/graphics/Device.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <format>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "graphics/BufferResource.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"
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

// The checks that the GPU device makes of a buffer as breaks of its contract, said here as what the app asked for.
void Device::validateBuffer(std::size_t size, bool data, const Buffer::Options& options) const {
    const Buffer::Usage& usage = options.usage;
    if (size == 0) {
        throw std::invalid_argument("A buffer needs a size of at least 1 byte.");
    }

    if (usage == Buffer::Usage{}) {
        throw std::invalid_argument("A buffer needs at least one usage: \"vertex\", \"index\", \"uniform\", \"storage\", \"indirect\", \"copySource\" or \"copyDestination\".");
    }

    const gpu::Device::Capabilities& capabilities = state->gpu->getCapabilities();
    if ((usage.storage && !capabilities.storageBuffers) || (usage.indirect && !capabilities.indirectBuffers)) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" has no {} buffers.", getApiName(), usage.storage && !capabilities.storageBuffers ? "storage" : "indirect"));
    }

    if (usage.uniform && (usage.vertex || usage.index || usage.storage || usage.indirect)) {
        throw std::invalid_argument("A uniform buffer serves no other draw. Make a buffer of its own for the vertices, indices, storage or indirect arguments.");
    }

    if (usage.index && usage.vertex) {
        throw std::invalid_argument("An index buffer holds no vertices. Make a buffer of its own for the vertices.");
    }

    if (options.update == Buffer::Update::Immutable && (!data || usage.copyDestination)) {
        throw std::invalid_argument("An immutable buffer keeps the data it is made with, so it needs data and takes no \"copyDestination\" usage.");
    }

    if (options.update == Buffer::Update::Stream && (usage.copySource || usage.copyDestination)) {
        throw std::invalid_argument("A stream buffer holds new contents every frame, so it takes no \"copySource\" or \"copyDestination\" usage.");
    }
}

void Device::validateRange(const Buffer& buffer, std::size_t offset, std::size_t size, std::string_view action, bool wholeWords) {
    if (!buffer.isValid()) {
        throw std::invalid_argument(std::format("To {} a buffer, it must be a valid buffer.", action));
    }

    if (size == 0 || offset > buffer.getSize() || size > buffer.getSize() - offset) {
        throw std::invalid_argument(std::format("To {} {} bytes at the offset {} of a buffer of {} bytes, the range must lie inside the buffer.", action, size, offset, buffer.getSize()));
    }

    if (offset % 4 != 0 || (wholeWords && size % 4 != 0)) {
        throw std::invalid_argument(std::format("To {} a buffer, the offset must be a multiple of 4 bytes, and the size too unless the buffer is a stream buffer, not {} and {}.", action, offset, size));
    }
}

Buffer Device::createBuffer(std::size_t size, const Buffer::Options& options) {
    validateBuffer(size, false, options);
    const std::vector<std::byte> zeros(size);
    return createBuffer(zeros, options);
}

Buffer Device::createBuffer(std::span<const std::byte> data, const Buffer::Options& options) {
    validateBuffer(data.size(), true, options);
    auto resource = std::make_shared<BufferResource>();
    resource->buffer = state->gpu->makeBuffer({.data = data, .usage = options.usage, .update = options.update, .label = options.label});
    resource->size = data.size();
    resource->usage = options.usage;
    resource->update = options.update;
    resource->label = std::string(options.label);
    resource->graveyard = state->graveyard;
    resource->tracked.setBytes(data.size());
    return Buffer(std::move(resource));
}

void Device::writeBuffer(const Buffer& buffer, std::size_t offset, std::span<const std::byte> data) {
    if (buffer.isValid() && buffer.getUpdate() == Buffer::Update::Immutable) {
        throw std::invalid_argument("An immutable buffer keeps the data it was made with. Make it dynamic or stream to write into it.");
    }

    // A stream buffer takes the bytes of a frame whatever their count, and a dynamic one whole words.
    const bool stream = buffer.isValid() && buffer.getUpdate() == Buffer::Update::Stream;
    validateRange(buffer, offset, data.size(), "write", !stream);
    state->gpu->writeBuffer(buffer.getGpuBuffer(), offset, data);
}

void Device::copyBuffer(const Buffer& source, std::size_t sourceOffset, const Buffer& destination, std::size_t destinationOffset, std::size_t size) {
    validateRange(source, sourceOffset, size, "copy from", true);
    validateRange(destination, destinationOffset, size, "copy into", true);
    if (!source.getUsage().copySource || !destination.getUsage().copyDestination) {
        throw std::invalid_argument("A copy reads a buffer with the \"copySource\" usage and writes one with the \"copyDestination\" usage.");
    }

    if (source == destination) {
        throw std::invalid_argument("A copy moves bytes between two buffers. Copy through a buffer of its own to move bytes inside one buffer.");
    }

    state->gpu->copyBuffer(source.getGpuBuffer(), sourceOffset, destination.getGpuBuffer(), destinationOffset, size);
}

void Device::readBuffer(const Buffer& buffer, std::size_t offset, std::size_t size, BufferReadCallback done) {
    validateRange(buffer, offset, size, "read", true);
    if (!buffer.getUsage().copySource) {
        throw std::invalid_argument("A read copies the bytes of a buffer, so the buffer needs the \"copySource\" usage.");
    }

    state->bufferReads.push_back({.buffer = buffer, .offset = offset, .size = size, .done = std::move(done)});
}

// A read starts its copy at the first call after the frame that asked for it, and hands over its bytes once the copy finished. Callbacks run after the walk, since a callback may ask for another read.
void Device::processBufferReads() {
    std::vector<std::pair<BufferReadCallback, std::optional<std::vector<std::uint8_t>>>> finished;
    std::vector<std::pair<BufferReadCallback, std::string>> failed;
    for (std::size_t index = 0; index < state->bufferReads.size();) {
        DeviceState::PendingBufferRead& read = state->bufferReads[index];
        std::optional<std::vector<std::uint8_t>> bytes;
        try {
            if (!read.copy) {
                read.copy = state->gpu->readBuffer(read.buffer.getGpuBuffer(), read.offset, read.size);
            } else {
                bytes = read.copy->poll();
            }
        } catch (const std::exception& error) {
            failed.emplace_back(std::move(read.done), error.what());
            state->bufferReads.erase(state->bufferReads.begin() + static_cast<std::ptrdiff_t>(index));
            continue;
        }

        if (!bytes) {
            ++index;
            continue;
        }

        finished.emplace_back(std::move(read.done), std::move(bytes));
        state->bufferReads.erase(state->bufferReads.begin() + static_cast<std::ptrdiff_t>(index));
    }

    for (auto& [done, bytes] : finished) {
        done(std::move(bytes), {});
    }

    for (auto& [done, error] : failed) {
        done(std::nullopt, error);
    }
}

Device::Device(std::unique_ptr<gpu::Device> device) : state(std::make_unique<DeviceState>(std::move(device))) {
    state->white = createTexture(Image(1, 1, math::Color::white()));
}

Device::~Device() {
    cancelCapture();
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
    options.mipmaps = options.hasMipmaps();
    options = state->settle(options);

    auto resource = std::make_shared<TextureResource>();
    std::size_t bytes = 0;
    resource->image = state->createTextureImage(image, *options.mipmaps, bytes, resource->mipLevels);
    resource->tracked.setBytes(bytes);
    resource->sampler = state->getSampler(options);
    resource->width = image.getWidth();
    resource->height = image.getHeight();
    resource->id = state->nextTextureId++;
    resource->options = options;
    resource->graveyard = state->graveyard;
    return Texture(std::move(resource));
}

// A texture takes the levels of its data from the largest down, and keeps only the first one when it keeps no mipmaps. A texture that brings its first level alone and keeps mipmaps takes them from the GPU, which generates them from that level once it reached the image.
Texture Device::createTexture(const TextureData& data, Texture::Options options) {
    validateData(data, options);
    if (options.mipmaps.value_or(false) && data.levels.size() == 1) {
        return state->createStagedTexture(data, options, false);
    }

    options.mipmaps = options.mipmaps.value_or(data.levels.size() > 1);
    options = state->settle(options);
    const std::span<const std::span<const std::byte>> levels = *options.mipmaps ? data.levels : data.levels.first(1);

    std::size_t bytes = 0;
    for (const std::span<const std::byte> level : levels) {
        bytes += level.size();
    }

    auto resource = std::make_shared<TextureResource>();
    const int layers = DeviceState::getLayers(data);
    resource->image = state->gpu->makeImage({.type = data.type, .width = data.width, .height = data.height, .layers = layers, .mipLevels = static_cast<int>(levels.size()), .format = data.format, .kind = gpu::Device::ImageKind::Texture, .levels = levels, .label = "haylen-texture"});
    resource->tracked.setBytes(bytes);
    resource->sampler = state->getSampler(options);
    resource->width = data.width;
    resource->height = data.height;
    resource->id = state->nextTextureId++;
    resource->options = options;
    resource->format = data.format;
    resource->type = data.type;
    resource->layers = layers;
    resource->mipLevels = static_cast<int>(levels.size());
    resource->graveyard = state->graveyard;
    return Texture(std::move(resource));
}

Texture Device::createTexture(const TextureData& shape, math::Color fill, Texture::Options options) {
    validateShape(shape);
    const std::vector<std::uint8_t> pixels = fillLayers(shape, fill);
    const std::array<std::span<const std::byte>, 1> levels{std::as_bytes(std::span(pixels))};
    TextureData data = shape;
    data.levels = levels;
    options.mipmaps = options.hasMipmaps();
    return createTexture(data, options);
}

Texture Device::createDynamicTexture(const TextureData& shape, math::Color fill, Texture::Options options) {
    validateShape(shape);
    const std::vector<std::uint8_t> pixels = fillLayers(shape, fill);
    const std::array<std::span<const std::byte>, 1> levels{std::as_bytes(std::span(pixels))};
    TextureData data = shape;
    data.levels = levels;
    return createDynamicTexture(data, options);
}

// Every pixel of every layer of the first level takes the color in RGBA8.
std::vector<std::uint8_t> Device::fillLayers(const TextureData& shape, math::Color fill) {
    const Image layer(shape.width, shape.height, fill);
    const std::span<const std::uint8_t> one = layer.getPixels();
    const auto layers = static_cast<std::size_t>(DeviceState::getLayers(shape));
    std::vector<std::uint8_t> pixels;
    pixels.reserve(one.size() * layers);
    for (std::size_t index = 0; index < layers; ++index) {
        pixels.insert(pixels.end(), one.begin(), one.end());
    }

    return pixels;
}

Texture Device::createDynamicTexture(const TextureData& data, Texture::Options options) {
    validateData(data, options);
    if (data.levels.size() != 1) {
        throw std::invalid_argument("A dynamic texture takes the pixels of its first level alone, and the GPU generates the others when it keeps mipmaps.");
    }

    if (gpu::ImageInfo::isCompressed(data.format)) {
        throw std::invalid_argument(std::format("A dynamic texture changes its pixels, which the blocks of the format \"{}\" do not.", getFormatName(data.format)));
    }

    return state->createStagedTexture(data, options, true);
}

// A view shows its range of the image of its texture with the sampler of that texture, at the size of its first mip level.
Texture Device::createView(const Texture& texture, const TextureView& view) {
    validateView(texture, view);
    const std::shared_ptr<TextureResource>& viewed = texture.getResource();
    const bool volume = view.type == gpu::Device::TextureType::Volume;
    auto resource = std::make_shared<TextureResource>();
    resource->image = state->gpu->makeView({.image = viewed->image, .type = view.type, .baseMipLevel = view.mipLevel, .mipLevels = view.mipLevels, .baseLayer = view.layer, .layers = volume ? viewed->layers : view.layers, .label = "haylen-texture-view"});
    resource->sampler = viewed->sampler;
    resource->width = std::max(1, viewed->width >> view.mipLevel);
    resource->height = std::max(1, viewed->height >> view.mipLevel);
    resource->id = state->nextTextureId++;
    resource->options = viewed->options;
    resource->format = viewed->format;
    resource->flipped = viewed->flipped;
    resource->type = view.type;
    resource->layers = volume ? std::max(1, viewed->layers >> view.mipLevel) : view.layers;
    resource->mipLevels = view.mipLevels;
    resource->label = viewed->label;
    resource->viewed = viewed;
    resource->graveyard = state->graveyard;
    return Texture(std::move(resource));
}

// A view keeps the type of its texture, or shows faces of a cube or a cube array as an array, within the mip levels and layers that the texture holds.
void Device::validateView(const Texture& texture, const TextureView& view) const {
    if (!state->gpu->getCapabilities().imageViews) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device makes no views of textures. Check \"imageViews\" of \"graphics.capabilities()\" before making one.", getApiName()));
    }

    const std::shared_ptr<TextureResource>& viewed = texture.getResource();
    if (viewed == nullptr || viewed->viewed != nullptr) {
        throw std::invalid_argument("A view shows a range of a texture, not of another view.");
    }

    const gpu::Device::TextureType type = viewed->type;
    const bool faces = view.type == gpu::Device::TextureType::Array && (type == gpu::Device::TextureType::Cube || type == gpu::Device::TextureType::CubeArray);
    if (view.type != type && !faces) {
        throw std::invalid_argument(std::format("A texture of the kind \"{}\" has no view of the kind \"{}\".", Texture::typeName(type), Texture::typeName(view.type)));
    }

    if (view.mipLevel < 0 || view.mipLevels < 1 || view.mipLevel + view.mipLevels > viewed->mipLevels) {
        throw std::invalid_argument(std::format("The mip levels of a view lie within the {} mip levels of its texture, which {} levels from the level {} do not.", viewed->mipLevels, view.mipLevels, view.mipLevel));
    }

    switch (view.type) {
    case gpu::Device::TextureType::Flat:
    case gpu::Device::TextureType::Volume:
        if (view.layer != 0 || view.layers != 1) {
            throw std::invalid_argument(std::format("A view of the kind \"{}\" shows the whole texture, so its layer is 0 and it has 1 layer.", Texture::typeName(view.type)));
        }

        break;
    case gpu::Device::TextureType::Cube:
        if (view.layer != 0 || view.layers != 6) {
            throw std::invalid_argument("A view of the kind \"cube\" shows the 6 faces of its cube, from the layer 0.");
        }

        break;
    case gpu::Device::TextureType::Array:
    case gpu::Device::TextureType::CubeArray:
        if (view.layer < 0 || view.layers < 1 || view.layer + view.layers > viewed->layers) {
            throw std::invalid_argument(std::format("The layers of a view lie within the {} layers of its texture, which {} layers from the layer {} do not.", viewed->layers, view.layers, view.layer));
        }

        if (view.type == gpu::Device::TextureType::CubeArray && (view.layer % 6 != 0 || view.layers % 6 != 0)) {
            throw std::invalid_argument("A view of the kind \"cubeArray\" shows whole cubes, 6 layers for each, from a layer that is a multiple of 6.");
        }

        break;
    }
}

// The data of a texture fits the device before any of it reaches the GPU: a type and a size that the device takes, a color format that the device samples and filters, sides of whole blocks, no compressed volume, a level of the right size for each mip level it brings, and a format whose mipmaps the GPU generates when it brings its first level alone and keeps mipmaps.
void Device::validateData(const TextureData& data, Texture::Options options) const {
    validateShape(data);
    const std::string_view format = getFormatName(data.format);
    if (gpu::ImageInfo::isDepth(data.format)) {
        throw std::invalid_argument(std::format("The format \"{}\" belongs to depth attachments, so no texture is made in it.", format));
    }

    const gpu::Device::FormatSupport& support = state->gpu->getCapabilities().getFormat(data.format);
    if (!support.sample) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device samples no texture of the format \"{}\". Make the texture in a format that the device samples, as its format support tells.", getApiName(), format));
    }

    if (!support.filter) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device samples the format \"{}\" only without filtering, and textures filter. Make the texture in a format that the device filters, as its format support tells.", getApiName(), format));
    }

    const gpu::ImageInfo::Block block = gpu::ImageInfo::getBlock(data.format);
    if (data.width % block.width != 0 || data.height % block.height != 0) {
        throw std::invalid_argument(std::format("A texture of the format \"{}\" has sides of whole blocks of {} by {} pixels, which {} by {} pixels are not.", format, block.width, block.height, data.width, data.height));
    }

    if (data.type == gpu::Device::TextureType::Volume && gpu::ImageInfo::isCompressed(data.format)) {
        throw std::invalid_argument(std::format("A texture of the kind \"volume\" holds no blocks of the format \"{}\", since compressed formats hold flat blocks.", format));
    }

    const gpu::Device::ImageDesc shape{.type = data.type, .width = data.width, .height = data.height, .layers = DeviceState::getLayers(data), .format = data.format};
    const int most = gpu::ImageInfo::getMaxMipLevels(shape);
    if (data.levels.empty() || static_cast<int>(data.levels.size()) > most) {
        throw std::invalid_argument(std::format("A texture of {} by {} pixels brings from 1 to {} levels, not {}.", data.width, data.height, most, data.levels.size()));
    }

    if (options.mipmaps.value_or(false) && data.levels.size() == 1 && !support.mipmaps) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device generates no mipmaps of the format \"{}\", so a texture of it brings each of its levels.", getApiName(), format));
    }

    for (std::size_t level = 0; level < data.levels.size(); ++level) {
        const std::size_t expected = gpu::ImageInfo::getLevelBytes(shape, static_cast<int>(level));
        if (data.levels[level].size() != expected) {
            throw std::invalid_argument(std::format("The level {} of a texture of {} by {} pixels of the format \"{}\" holds {} bytes, and it needs {}.", level + 1, data.width, data.height, format, data.levels[level].size(), expected));
        }
    }
}

// A texture has the sides, the layers and the slices that its type takes within the limits of the device.
void Device::validateShape(const TextureData& data) const {
    const gpu::Device::Capabilities& capabilities = state->gpu->getCapabilities();
    const std::string_view kind = Texture::typeName(data.type);
    if (data.type != gpu::Device::TextureType::Volume && data.depth != 1) {
        throw std::invalid_argument(std::format("Only a texture of the kind \"volume\" has a depth, and one of the kind \"{}\" has none.", kind));
    }

    const bool single = data.type == gpu::Device::TextureType::Flat || data.type == gpu::Device::TextureType::Volume;
    if (single && data.layers != 1) {
        throw std::invalid_argument(std::format("A texture of the kind \"{}\" has 1 layer, not {}.", kind, data.layers));
    }

    const bool cube = data.type == gpu::Device::TextureType::Cube || data.type == gpu::Device::TextureType::CubeArray;
    if (cube && data.width != data.height) {
        throw std::invalid_argument(std::format("A texture of the kind \"{}\" has square faces, which {} by {} pixels are not.", kind, data.width, data.height));
    }

    switch (data.type) {
    case gpu::Device::TextureType::Flat:
        validateSize(data.width, data.height, capabilities.maxImageSize);
        break;
    case gpu::Device::TextureType::Array:
        validateSize(data.width, data.height, capabilities.maxImageSize);
        if (data.layers < 1 || data.layers > capabilities.maxArrayLayers) {
            throw std::invalid_argument(std::format("A texture of the kind \"array\" has from 1 to {} layers on this device, not {}.", capabilities.maxArrayLayers, data.layers));
        }

        break;
    case gpu::Device::TextureType::Volume:
        validateSize(data.width, data.height, capabilities.maxVolumeSize);
        if (data.depth < 1 || data.depth > capabilities.maxVolumeSize) {
            throw std::invalid_argument(std::format("A texture of the kind \"volume\" has from 1 to {} slices on this device, not {}.", capabilities.maxVolumeSize, data.depth));
        }

        break;
    case gpu::Device::TextureType::Cube:
        validateSize(data.width, data.height, capabilities.maxCubeSize);
        if (data.layers != 6) {
            throw std::invalid_argument(std::format("A texture of the kind \"cube\" has 6 layers, one for each face, not {}.", data.layers));
        }

        break;
    case gpu::Device::TextureType::CubeArray:
        if (!capabilities.cubeArrays) {
            throw std::invalid_argument(std::format("The graphics API \"{}\" of this device has no textures of the kind \"cubeArray\". Check \"cubeArrays\" of \"graphics.capabilities()\" before making one.", getApiName()));
        }

        validateSize(data.width, data.height, capabilities.maxCubeSize);
        if (data.layers < 6 || data.layers % 6 != 0 || data.layers > capabilities.maxArrayLayers) {
            throw std::invalid_argument(std::format("A texture of the kind \"cubeArray\" has 6 layers for each of its cubes, up to {} on this device, not {}.", capabilities.maxArrayLayers, data.layers));
        }

        break;
    }
}

std::string_view Device::getFormatName(gpu::Device::PixelFormat format) noexcept {
    const auto found = std::ranges::find(gpu::Device::kPixelFormatNames, format, &std::pair<std::string_view, gpu::Device::PixelFormat>::second);
    return found->first;
}

Texture Device::createTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createTexture(Image(width, height, fill), options);
}

const Texture& Device::getWhiteTexture() const noexcept {
    return state->white;
}

const Texture& Device::getWhiteTexture(gpu::Device::TextureType type) {
    if (type == gpu::Device::TextureType::Flat) {
        return state->white;
    }

    Texture& white = state->whites[static_cast<std::size_t>(type)];
    if (!white.isValid()) {
        const bool cube = type == gpu::Device::TextureType::Cube || type == gpu::Device::TextureType::CubeArray;
        white = createTexture(TextureData{.type = type, .width = 1, .height = 1, .layers = cube ? 6 : 1}, math::Color::white());
    }

    return white;
}

RenderTarget Device::createRenderTarget(int width, int height, Texture::Options options, const RenderTarget::Options& attachments) {
    validateSize(width, height, getMaxTextureSize());

    if (options.mipmaps.value_or(false)) {
        throw std::invalid_argument("A render target has no mipmaps.");
    }

    validateAttachments(attachments);
    return state->createRenderTarget(width, height, options, attachments);
}

// Every color is a format that the device draws into and filters, the depth a depth format that it draws into, each of them takes the samples, and a kept depth is one the device samples, which a target of several samples resolves by a mode the device has from a format without stencil.
void Device::validateAttachments(const RenderTarget::Options& attachments) const {
    const gpu::Device::Capabilities& capabilities = state->gpu->getCapabilities();
    if (attachments.colors.empty() || attachments.colors.size() > gpu::Device::kMaxColorTargets) {
        throw std::invalid_argument(std::format("A render target has from 1 to {} colors, not {}.", gpu::Device::kMaxColorTargets, attachments.colors.size()));
    }

    if (attachments.samples < 1 || !std::has_single_bit(static_cast<unsigned>(attachments.samples))) {
        throw std::invalid_argument(std::format("The samples of a render target are a power of two from 1, not {}.", attachments.samples));
    }

    for (const gpu::Device::PixelFormat format : attachments.colors) {
        const gpu::Device::FormatSupport& support = capabilities.getFormat(format);
        if (gpu::ImageInfo::isDepth(format) || !support.render) {
            throw std::invalid_argument(std::format("The graphics API \"{}\" of this device draws into no color of the format \"{}\". Make the render target in a format whose \"render\" its format support holds.", getApiName(), getFormatName(format)));
        }

        if (!support.filter) {
            throw std::invalid_argument(std::format("The graphics API \"{}\" of this device samples the format \"{}\" only without filtering, and the textures of a render target filter. Make the render target in a format that the device filters, as its format support tells.", getApiName(), getFormatName(format)));
        }

        validateSamples(format, attachments.samples);
    }

    if (!attachments.depthFormat) {
        if (attachments.depthTexture) {
            throw std::invalid_argument("A render target keeps a depth texture only with a \"depthFormat\".");
        }

        return;
    }

    const gpu::Device::PixelFormat depth = *attachments.depthFormat;
    if (!gpu::ImageInfo::isDepth(depth) || !capabilities.getFormat(depth).render) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device draws into no depth of the format \"{}\". Name a depth format whose \"render\" its format support holds.", getApiName(), getFormatName(depth)));
    }

    validateSamples(depth, attachments.samples);
    if (!attachments.depthTexture) {
        return;
    }

    if (!capabilities.getFormat(depth).sample) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device samples no depth of the format \"{}\", so a render target keeps no depth texture of it.", getApiName(), getFormatName(depth)));
    }

    if (attachments.samples == 1) {
        return;
    }

    if (gpu::ImageInfo::hasStencil(depth)) {
        throw std::invalid_argument(std::format("A render target of several samples keeps a depth texture only of a depth format without stencil, which \"{}\" has.", getFormatName(depth)));
    }

    if (!capabilities.hasDepthResolve(attachments.depthResolve)) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device resolves no depth by \"{}\". Name a \"depthResolve\" of the \"depthResolves\" of \"graphics.capabilities()\", or keep no depth texture.", getApiName(), getDepthResolveName(attachments.depthResolve)));
    }
}

void Device::validateSamples(gpu::Device::PixelFormat format, int samples) const {
    if (!gpu::ImageRules::takesSamples(state->gpu->getCapabilities(), format, samples)) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of this device takes no {} samples of the format \"{}\". Use one of the \"sampleCounts\" of its format support.", getApiName(), samples, getFormatName(format)));
    }
}

std::string_view Device::getDepthResolveName(gpu::Device::DepthResolve mode) noexcept {
    return std::ranges::find(gpu::Device::kDepthResolveNames, mode, &std::pair<std::string_view, gpu::Device::DepthResolve>::second)->first;
}

void Device::replaceTexture(const Texture& texture, const Image& image) {
    validateSize(image.getWidth(), image.getHeight(), getMaxTextureSize());
    const std::shared_ptr<TextureResource>& resource = texture.getResource();
    if (resource->type != gpu::Device::TextureType::Flat || resource->viewed != nullptr) {
        throw std::invalid_argument("Only a flat texture that is no view takes the pixels of an image in place of its own.");
    }

    // The old image is released only once the new one exists, so a failed creation leaves the texture intact. A dynamic texture stays dynamic, and its new pixels reach the GPU with the next upload.
    std::size_t bytes = DeviceState::getImageBytes(image.getWidth(), image.getHeight(), gpu::Device::PixelFormat::Rgba8);
    int mipLevels = 1;
    const TextureData shape{.width = image.getWidth(), .height = image.getHeight()};
    const gpu::Device::Image created = resource->dynamic ? state->createStagedImage(shape, resource->generatesMipmaps, mipLevels) : state->createTextureImage(image, resource->options.hasMipmaps(), bytes, mipLevels);
    state->graveyard->bury(resource->image);
    resource->image = created;
    resource->format = gpu::Device::PixelFormat::Rgba8;
    resource->width = image.getWidth();
    resource->height = image.getHeight();
    resource->mipLevels = mipLevels;
    resource->tracked.setBytes(bytes);

    // The region that waited for an upload belongs to the old image, so the new pixels go whole.
    if (resource->dynamic) {
        resource->staged.front().pending = false;
        state->stagePixels(resource, 0, image.getPixels(), {0, 0, image.getWidth(), image.getHeight()});
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

    processBufferReads();
}

void Device::cancelReads() noexcept {
    state->reads.clear();
    state->bufferReads.clear();
}

std::size_t Device::getPendingReadCount() const noexcept {
    return state->reads.size() + state->bufferReads.size();
}

gpu::Device::CaptureSupport Device::getCaptureSupport() const {
    return state->gpu->getCaptureSupport();
}

void Device::captureFrames(int frames, const std::filesystem::path& path, CaptureCallback done) {
    if (frames < 1) {
        throw std::invalid_argument("A capture records at least 1 frame.");
    }

    if (state->capture) {
        throw std::logic_error("A GPU capture is already recording. Wait for it to end before capturing again.");
    }

    gpu::Device::CaptureSupport support = state->gpu->getCaptureSupport();
    if (support.tool == gpu::Device::CaptureTool::None) {
        throw std::runtime_error(std::move(support.missing));
    }

    state->gpu->beginCapture(path);
    state->capture = DeviceState::PendingCapture{.framesLeft = frames, .done = std::move(done)};
}

bool Device::isCapturing() const noexcept {
    return state->capture.has_value();
}

// The callback runs once the capture is over, so it may start another one.
void Device::processCapture() {
    if (!state->capture || --state->capture->framesLeft > 0) {
        return;
    }

    CaptureCallback done = std::move(state->capture->done);
    state->capture.reset();

    std::filesystem::path path;
    try {
        path = state->gpu->endCapture();
    } catch (const std::exception& error) {
        done(std::nullopt, error.what());
        return;
    }

    done(std::move(path), {});
}

void Device::cancelCapture() noexcept {
    if (!state->capture) {
        return;
    }

    state->capture.reset();

    try {
        static_cast<void>(state->gpu->endCapture());
    } catch (const std::exception& error) {
        core::Log::warning("The GPU capture that the app stopped during failed to end: {}", error.what());
    }
}

Texture Device::createDynamicTexture(const Image& image, Texture::Options options) {
    const std::array<std::span<const std::byte>, 1> levels{std::as_bytes(image.getPixels())};
    return createDynamicTexture(TextureData{.width = image.getWidth(), .height = image.getHeight(), .levels = levels}, options);
}

Texture Device::createDynamicTexture(int width, int height, math::Color fill, Texture::Options options) {
    validateSize(width, height, getMaxTextureSize());
    return createDynamicTexture(Image(width, height, fill), options);
}

// The texture takes the bytes of the halves as they lie in memory, which is how the GPU reads them.
Texture Device::createDynamicDistanceTexture(int width, int height, std::span<const std::uint16_t> distances, Texture::Options options) {
    validateDistances(width, height, distances, getMaxTextureSize());
    const std::array<std::span<const std::byte>, 1> levels{std::as_bytes(distances)};
    return createDynamicTexture(TextureData{.width = width, .height = height, .format = gpu::Device::PixelFormat::R16Float, .levels = levels}, options);
}

void Device::updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels) {
    updateTexture(texture, pixels, {0, 0, texture.getWidth(), texture.getHeight()});
}

void Device::updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels, Region changed) {
    updateTexture(texture, 0, pixels, changed);
}

void Device::updateTexture(const Texture& texture, int layer, std::span<const std::uint8_t> pixels, Region changed) {
    const std::shared_ptr<TextureResource>& resource = texture.getResource();
    if (!resource || !resource->dynamic) {
        throw std::logic_error("Only a dynamic texture changes its pixels in place.");
    }

    if (layer < 0 || layer >= resource->layers) {
        throw std::invalid_argument(std::format("The dynamic texture has {} layers from 0, so it has no layer {}.", resource->layers, layer));
    }

    if (pixels.size() != DeviceState::getImageBytes(resource->width, resource->height, resource->format)) {
        throw std::invalid_argument("The pixels do not match the size of the dynamic texture.");
    }

    const bool inside = changed.x >= 0 && changed.y >= 0 && changed.width > 0 && changed.height > 0 && changed.x + changed.width <= resource->width && changed.y + changed.height <= resource->height;
    if (!inside) {
        throw std::invalid_argument("The region of a texture update must lie inside the dynamic texture and hold at least one pixel.");
    }

    state->stagePixels(resource, layer, pixels, changed);
}

// Every texture uploads the region that changed since the last upload, from the last pixels it received, all of them in one hand-over to the GPU device before the passes of the frame.
std::size_t Device::uploadTextures(std::span<const gpu::Device::ImageWrite> frameImages) {
    ++state->uploads;
    std::swap(state->changedTextures, state->uploadingTextures);
    state->uploading.clear();
    state->writes.clear();
    std::size_t bytes = 0;
    for (const std::weak_ptr<TextureResource>& changed : state->uploadingTextures) {
        if (std::shared_ptr<TextureResource> resource = changed.lock()) {
            for (std::size_t layer = 0; layer < resource->staged.size(); ++layer) {
                TextureResource::StagedLayer& staged = resource->staged[layer];
                if (!staged.pending) {
                    continue;
                }

                state->writes.push_back({.image = resource->image, .layer = static_cast<int>(layer), .region = staged.region, .pixels = std::as_bytes(std::span(staged.pixels))});
                bytes += staged.pixels.size();
                staged.pending = false;
            }

            resource->pending = false;
            resource->uploadedAt = state->uploads;
            state->uploading.push_back(std::move(resource));
        }
    }

    state->writes.insert(state->writes.end(), frameImages.begin(), frameImages.end());

    if (!state->writes.empty()) {
        state->gpu->writeImages(state->writes);
    }

    for (const std::shared_ptr<TextureResource>& resource : state->uploading) {
        if (resource->generatesMipmaps) {
            state->gpu->generateMipmaps(resource->image);
        }
    }

    state->writes.clear();
    state->uploading.clear();

    // A texture that changed for the upload before but not for this one gives its staging memory back.
    for (const std::weak_ptr<TextureResource>& uploaded : state->uploadedTextures) {
        const std::shared_ptr<TextureResource> resource = uploaded.lock();
        if (resource && !resource->pending && resource->uploadedAt != state->uploads) {
            for (TextureResource::StagedLayer& staged : resource->staged) {
                staged.pixels = std::vector<std::uint8_t>();
            }
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

std::optional<double> Device::getInputAge() const noexcept {
    return state->gpu->getInputAge();
}

std::string Device::getAdapterName() const {
    return state->gpu->getAdapterName();
}

// A texture never outgrows the longest side of an image, which the pixels of every texture and every read back pass through.
int Device::getMaxTextureSize() const noexcept {
    return std::min(state->gpu->getCapabilities().maxImageSize, Image::kMaxSide);
}

void Device::readPools(std::vector<Pool>& pools) const {
    state->gpu->readPools(pools);
}

void Device::collectGarbage() {
    state->graveyard->collect(*state->gpu);
}

void Device::followSpareMemory(double seconds) {
    const std::size_t spare = state->gpu->getSpareBytes();
    if (spare > state->spare) {
        state->sinceSpareGrew = 0.0;
    } else if (state->sinceSpareGrew) {
        *state->sinceSpareGrew += seconds;
    }

    state->spare = spare;
    if (state->sinceSpareGrew && *state->sinceSpareGrew >= kTrimDelay) {
        trim();
    }
}

void Device::trim() {
    collectGarbage();
    state->gpu->trim();
    state->spare = state->gpu->getSpareBytes();
    state->sinceSpareGrew.reset();
}

std::size_t Device::getSpareBytes() const noexcept {
    return state->gpu->getSpareBytes();
}

} // namespace haylen::graphics
