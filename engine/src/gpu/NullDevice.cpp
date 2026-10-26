#include "gpu/NullDevice.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "gpu/ImageInfo.hpp"

namespace haylen::gpu {

std::optional<std::vector<std::uint8_t>> NullDevice::NullReadback::poll() {
    if (!std::exchange(started, true)) {
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(size, 0);
}

Device::Capabilities NullDevice::getDefaultCapabilities() noexcept {
    Capabilities capabilities{.maxImageSize = kMaxImageSize, .maxArrayLayers = kMaxArrayLayers, .maxSampleCount = kMaxSampleCount, .originTopLeft = true, .independentBlend = false, .multisampledRenderTargets = true, .gpuTiming = false};
    capabilities.textureFormats.fill(true);
    capabilities.renderFormats.fill(true);
    for (std::size_t format = 0; format < capabilities.blendableFormats.size(); ++format) {
        const auto pixelFormat = static_cast<PixelFormat>(format);
        const bool wide = pixelFormat == PixelFormat::R32Float || pixelFormat == PixelFormat::Rg32Float || pixelFormat == PixelFormat::Rgba32Float;
        capabilities.blendableFormats[format] = !wide && !ImageInfo::isDepth(pixelFormat);
    }
    return capabilities;
}

bool NullDevice::isPowerOfTwo(int value) noexcept {
    return value > 0 && (value & (value - 1)) == 0;
}

void NullDevice::requireRoom(std::size_t used, int size, std::string_view message) {
    if (used >= static_cast<std::size_t>(size)) {
        throw std::runtime_error(std::vformat(message, std::make_format_args(size)));
    }
}

Device::Language NullDevice::getLanguage() const noexcept {
    return language;
}

Device::Api NullDevice::getApi() const noexcept {
    return Api::None;
}

const Device::Capabilities& NullDevice::getCapabilities() const noexcept {
    return features;
}

std::string NullDevice::getAdapterName() const {
    return {};
}

std::optional<double> NullDevice::getGpuFrameSeconds() const noexcept {
    return std::nullopt;
}

std::vector<Device::Pool> NullDevice::getPools() const {
    return {
        {.name = "images", .used = static_cast<int>(images.size()), .size = kImagePoolSize}, {.name = "buffers", .used = static_cast<int>(buffers.size()), .size = kBufferPoolSize}, {.name = "samplers", .used = static_cast<int>(samplers.size()), .size = kSamplerPoolSize}, {.name = "programs", .used = static_cast<int>(programs.size()), .size = kProgramPoolSize}, {.name = "pipelines", .used = static_cast<int>(pipelines.size()), .size = kPipelinePoolSize},
    };
}

bool NullDevice::isLost() const noexcept {
    return false;
}

Device::Buffer NullDevice::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw std::logic_error(std::format("The buffer \"{}\" has no size.", desc.label));
    }
    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw std::logic_error(std::format("The immutable buffer \"{}\" has no data.", desc.label));
    }

    requireRoom(buffers.size(), kBufferPoolSize, "The graphics device has no room for another buffer. At most {} baked sprite batches and draw buffers can exist at once.");
    const std::uint32_t id = nextId++;
    buffers.emplace(id, BufferState{.size = size, .usage = desc.usage, .index = desc.index});
    return {id};
}

void NullDevice::checkImageShape(const ImageDesc& desc) const {
    if (desc.width <= 0 || desc.height <= 0 || desc.width > features.maxImageSize || desc.height > features.maxImageSize) {
        throw std::logic_error(std::format("The image \"{}\" of {}x{} pixels does not fit the device.", desc.label, desc.width, desc.height));
    }

    const bool layered = desc.type == TextureType::Array || desc.type == TextureType::Volume;
    const int maxLayers = desc.type == TextureType::Volume ? features.maxImageSize : features.maxArrayLayers;
    if (desc.type == TextureType::Flat && desc.layers != 1) {
        throw std::logic_error(std::format("The flat image \"{}\" has {} layers, and a flat image has one.", desc.label, desc.layers));
    }
    if (desc.type == TextureType::Cube && (desc.layers != 6 || desc.width != desc.height)) {
        throw std::logic_error(std::format("The cube image \"{}\" needs six square faces.", desc.label));
    }
    if (layered && (desc.layers < 1 || desc.layers > maxLayers)) {
        throw std::logic_error(std::format("The image \"{}\" has {} layers, and the device holds 1 to {}.", desc.label, desc.layers, maxLayers));
    }

    if (desc.mipLevels < 1 || desc.mipLevels > ImageInfo::getMaxMipLevels(desc)) {
        throw std::logic_error(std::format("The image \"{}\" has {} mip levels, and its size holds 1 to {}.", desc.label, desc.mipLevels, ImageInfo::getMaxMipLevels(desc)));
    }
    if (!isPowerOfTwo(desc.sampleCount) || desc.sampleCount > features.maxSampleCount) {
        throw std::logic_error(std::format("The image \"{}\" has {} samples, and the device takes a power of two up to {}.", desc.label, desc.sampleCount, features.maxSampleCount));
    }
    if (desc.sampleCount > 1 && (desc.type != TextureType::Flat || desc.mipLevels != 1)) {
        throw std::logic_error(std::format("The multisampled image \"{}\" is not a flat image of one mip level.", desc.label));
    }
}

void NullDevice::checkImageKind(const ImageDesc& desc) {
    const bool depth = ImageInfo::isDepth(desc.format);
    if (depth != (desc.kind == ImageKind::DepthStencil)) {
        throw std::logic_error(std::format("The image \"{}\" pairs a depth format with a kind that is not a depth stencil image, or the other way around.", desc.label));
    }
    if (desc.kind != ImageKind::Texture && !desc.levels.empty()) {
        throw std::logic_error(std::format("The image \"{}\" starts without pixels, since only a texture is made with them.", desc.label));
    }
    if (desc.sampleCount > 1 && (desc.kind == ImageKind::Texture || desc.kind == ImageKind::Dynamic)) {
        throw std::logic_error(std::format("The image \"{}\" is multisampled, which only render targets and depth stencil images are.", desc.label));
    }
    if (desc.kind == ImageKind::Dynamic && (desc.mipLevels != 1 || desc.type != TextureType::Flat)) {
        throw std::logic_error(std::format("The dynamic image \"{}\" is not a flat image of one mip level.", desc.label));
    }
}

void NullDevice::checkTextureLevels(const ImageDesc& desc) {
    if (desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw std::logic_error(std::format("The texture \"{}\" has {} mip levels and pixels for {}.", desc.label, desc.mipLevels, desc.levels.size()));
    }
    for (std::size_t level = 0; level < desc.levels.size(); ++level) {
        const ImageInfo::Extent extent = ImageInfo::getLevelExtent(desc, static_cast<int>(level));
        if (desc.levels[level].size() != ImageInfo::getLevelBytes(desc, static_cast<int>(level))) {
            throw std::logic_error(std::format("A level of the texture \"{}\" does not hold {}x{}x{} pixels.", desc.label, extent.width, extent.height, extent.depth));
        }
    }
}

Device::Image NullDevice::makeImage(const ImageDesc& desc) {
    checkImageShape(desc);
    checkImageKind(desc);
    if (desc.kind == ImageKind::Texture) {
        checkTextureLevels(desc);
    }

    const bool renders = desc.kind == ImageKind::RenderTarget || desc.kind == ImageKind::DepthStencil;
    const std::array<bool, 16>& supported = renders ? features.renderFormats : features.textureFormats;
    if (!supported[static_cast<std::size_t>(desc.format)]) {
        throw std::logic_error(std::format("The image \"{}\" has a format that the device does not support for its kind.", desc.label));
    }

    requireRoom(images.size(), kImagePoolSize, "The graphics device has no room for another texture. At most {} textures and render targets can exist at once.");
    const std::uint32_t id = nextId++;
    ImageDesc kept = desc;
    kept.levels = {};
    kept.label = {};
    images.emplace(id, kept);
    return {id};
}

Device::Sampler NullDevice::makeSampler(const SamplerDesc& desc) {
    if (desc.maxAnisotropy < 1 || desc.maxAnisotropy > kMaxAnisotropy) {
        throw std::logic_error(std::format("The sampler \"{}\" filters with an anisotropy of {}, and the device takes 1 to {}.", desc.label, desc.maxAnisotropy, kMaxAnisotropy));
    }
    if (desc.minLod < 0.0F || desc.minLod > desc.maxLod) {
        throw std::logic_error(std::format("The sampler \"{}\" reads mip levels from {} to {}.", desc.label, desc.minLod, desc.maxLod));
    }

    requireRoom(samplers.size(), kSamplerPoolSize, "The graphics device has no room for another sampler. At most {} samplers can exist at once.");
    const std::uint32_t id = nextId++;
    SamplerDesc kept = desc;
    kept.label = {};
    samplers.emplace(id, kept);
    return {id};
}

Device::Program NullDevice::makeProgram(const ProgramDesc& desc) {
    ProgramRecord program{.blocks = {desc.blocks.begin(), desc.blocks.end()}, .textures = {desc.textures.begin(), desc.textures.end()}, .samplers = {desc.samplers.begin(), desc.samplers.end()}, .vertexBytes = desc.vertex.bytecode.size(), .fragmentBytes = desc.fragment.bytecode.size()};
    for (const BlockBinding& block : program.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(kMaxBlocks) || block.size == 0) {
            throw std::logic_error(std::format("The program \"{}\" has a uniform block at slot {} that the device cannot bind.", desc.label, block.slot));
        }
    }
    for (const TextureBinding& texture : program.textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(kMaxTextures)) {
            throw std::logic_error(std::format("The program \"{}\" has a texture at slot {} that the device cannot bind.", desc.label, texture.slot));
        }
    }
    for (const SamplerBinding& sampler : program.samplers) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(kMaxSamplers)) {
            throw std::logic_error(std::format("The program \"{}\" has a sampler at slot {} that the device cannot bind.", desc.label, sampler.slot));
        }
    }

    // SPIR-V comes as the words of a module, and every other language as text.
    if (language == Language::Spirv && (desc.vertex.bytecode.empty() || desc.fragment.bytecode.empty() || desc.vertex.bytecode.size() % 4 != 0 || desc.fragment.bytecode.size() % 4 != 0)) {
        throw std::logic_error(std::format("The program \"{}\" has no SPIR-V module of whole words for each stage.", desc.label));
    }
    if (language != Language::None && language != Language::Spirv && (desc.vertex.source.empty() || desc.fragment.source.empty())) {
        throw std::logic_error(std::format("The program \"{}\" has no source for each stage.", desc.label));
    }

    requireRoom(programs.size(), kProgramPoolSize, "The graphics device has no room for another shader. At most {} shader programs can exist at once.");
    const std::uint32_t id = nextId++;
    programs.emplace(id, std::move(program));
    return {id};
}

// A device without independent blending applies the blend and the write mask of the first color to every color, so the engine never asks it for different ones.
void NullDevice::checkColorTargets(const PipelineDesc& desc) const {
    if (desc.colors.size() > kMaxColorTargets || (desc.colors.empty() && !desc.depthStencil.format)) {
        throw std::logic_error(std::format("The pipeline \"{}\" needs one to {} color targets, or a depth stencil format without any.", desc.label, kMaxColorTargets));
    }
    for (const ColorTarget& color : desc.colors) {
        if (!features.independentBlend && (color.blend != desc.colors.front().blend || color.writeMask != desc.colors.front().writeMask)) {
            throw std::logic_error(std::format("The color targets of the pipeline \"{}\" differ in their blend or write mask, which the device applies alike to every color.", desc.label));
        }
        if (desc.swapchain) {
            continue;
        }
        if (ImageInfo::isDepth(color.format) || !features.renderFormats[static_cast<std::size_t>(color.format)]) {
            throw std::logic_error(std::format("The pipeline \"{}\" draws into a color format that the device does not render.", desc.label));
        }
        if (color.blend.enabled && !features.blendableFormats[static_cast<std::size_t>(color.format)]) {
            throw std::logic_error(std::format("The pipeline \"{}\" blends into a color format that the device does not blend.", desc.label));
        }
    }
}

void NullDevice::checkDepthStencilState(const PipelineDesc& desc) {
    const DepthStencil& state = desc.depthStencil;
    if (state.format && !ImageInfo::isDepth(*state.format)) {
        throw std::logic_error(std::format("The pipeline \"{}\" names a depth stencil format that is not a depth format.", desc.label));
    }
    if (!state.format && (state.depthWrite || state.depthCompare != CompareFunction::Always || state.stencilEnabled)) {
        throw std::logic_error(std::format("The pipeline \"{}\" tests depth or stencil without a depth stencil format.", desc.label));
    }
    if (state.stencilEnabled && state.format && !ImageInfo::hasStencil(*state.format)) {
        throw std::logic_error(std::format("The pipeline \"{}\" tests the stencil of a format without stencil.", desc.label));
    }
}

void NullDevice::checkVertexLayout(const PipelineDesc& desc) const {
    if (desc.buffers.size() > kMaxVertexBuffers || desc.attributes.size() > kMaxAttributes) {
        throw std::logic_error(std::format("The pipeline \"{}\" reads more vertex buffers or attributes than the device holds.", desc.label));
    }
    for (const VertexBufferLayout& layout : desc.buffers) {
        if (layout.stride <= 0) {
            throw std::logic_error(std::format("A vertex buffer of the pipeline \"{}\" has no stride.", desc.label));
        }
    }
    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size())) {
            throw std::logic_error(std::format("An attribute of the pipeline \"{}\" reads a vertex buffer it does not lay out.", desc.label));
        }
        if (attribute.location < 0 || attribute.location >= static_cast<int>(kMaxAttributes) || attribute.offset < 0) {
            throw std::logic_error(std::format("An attribute of the pipeline \"{}\" sits at a location or offset that the device cannot read.", desc.label));
        }
    }
    if (!isPowerOfTwo(desc.sampleCount) || desc.sampleCount > features.maxSampleCount) {
        throw std::logic_error(std::format("The pipeline \"{}\" draws with {} samples, and the device takes a power of two up to {}.", desc.label, desc.sampleCount, features.maxSampleCount));
    }
}

Device::Pipeline NullDevice::makePipeline(const PipelineDesc& desc) {
    if (!programs.contains(desc.program.id)) {
        throw std::logic_error(std::format("The pipeline \"{}\" names a program that does not exist.", desc.label));
    }
    checkColorTargets(desc);
    checkDepthStencilState(desc);
    checkVertexLayout(desc);

    PipelineState state{.program = desc.program, .buffers = {desc.buffers.begin(), desc.buffers.end()}, .indexType = desc.indexType};
    state.firstMask = desc.colors.empty() ? ColorMask::None : desc.colors.front().writeMask;
    state.targets.swapchain = desc.swapchain;
    if (!desc.swapchain) {
        for (const ColorTarget& color : desc.colors) {
            state.targets.colors[state.targets.colorCount++] = color.format;
        }
        state.targets.depthStencil = desc.depthStencil.format;
        state.targets.sampleCount = desc.sampleCount;
    }

    requireRoom(pipelines.size(), kPipelinePoolSize, "The graphics device has no room for another pipeline. At most {} pipelines can exist at once.");
    const std::uint32_t id = nextId++;
    pipelines.emplace(id, std::move(state));
    return {id};
}

void NullDevice::prepareProgram(const ProgramDesc&, core::JobSystem&) {
    ++preparedPrograms;
}

void NullDevice::destroy(Buffer buffer) noexcept {
    buffers.erase(buffer.id);
}

void NullDevice::destroy(Image image) noexcept {
    images.erase(image.id);
}

void NullDevice::destroy(Sampler sampler) noexcept {
    samplers.erase(sampler.id);
}

void NullDevice::destroy(Program program) noexcept {
    programs.erase(program.id);
}

void NullDevice::destroy(Pipeline pipeline) noexcept {
    pipelines.erase(pipeline.id);
}

NullDevice::BufferState& NullDevice::getBuffer(Buffer buffer) {
    const auto found = buffers.find(buffer.id);
    if (found == buffers.end()) {
        throw std::logic_error(std::format("The buffer {} does not exist.", buffer.id));
    }
    return found->second;
}

const NullDevice::BufferState& NullDevice::getBuffer(Buffer buffer) const {
    const auto found = buffers.find(buffer.id);
    if (found == buffers.end()) {
        throw std::logic_error(std::format("The buffer {} does not exist.", buffer.id));
    }
    return found->second;
}

const Device::ImageDesc& NullDevice::getImage(Image image) const {
    const auto found = images.find(image.id);
    if (found == images.end()) {
        throw std::logic_error(std::format("The image {} does not exist.", image.id));
    }
    return found->second;
}

void NullDevice::requirePass(std::string_view call) const {
    if (!pass) {
        throw std::logic_error(std::format("The call \"{}\" happened outside a pass.", call));
    }
}

void NullDevice::requireNoPass(std::string_view call) const {
    if (pass) {
        throw std::logic_error(std::format("The call \"{}\" happened inside a pass.", call));
    }
}

const NullDevice::PipelineState& NullDevice::getAppliedPipeline(std::string_view call) const {
    requirePass(call);
    if (!applied.isValid()) {
        throw std::logic_error(std::format("The call \"{}\" happened before a pipeline.", call));
    }
    return pipelines.at(applied.id);
}

// A frame writes each part of a buffer once, before any pass of the frame binds the buffer, so a device may place the writes of a frame where its draws read them.
void NullDevice::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    requireNoPass("writeBuffer");
    BufferState& state = getBuffer(buffer);
    if (state.usage != Usage::Dynamic) {
        throw std::logic_error("Only a dynamic buffer takes new contents.");
    }
    if (offset + data.size() > state.size) {
        throw std::logic_error(std::format("The buffer of {} bytes cannot take {} bytes at {}.", state.size, data.size(), offset));
    }
    if (state.boundFrame == frame) {
        throw std::logic_error("A buffer took new contents after a pass of the frame bound it.");
    }

    if (state.writtenFrame != frame) {
        state.writtenFrame = frame;
        state.written.clear();
    }
    const auto overlaps = [offset, end = offset + data.size()](const WrittenRange& range) { return offset < range.offset + range.size && range.offset < end; };
    if (std::ranges::any_of(state.written, overlaps)) {
        throw std::logic_error(std::format("A frame wrote the bytes at {} of a buffer twice.", offset));
    }
    state.written.push_back({.offset = offset, .size = data.size()});
    ++currentFrame.bufferWrites;
}

void NullDevice::writeImages(std::span<const ImageWrite> writes) {
    if (pass || recordedPasses > 0) {
        throw std::logic_error("Images take new pixels before the first pass of a frame.");
    }
    if (imagesWritten) {
        throw std::logic_error("The images of a frame take their pixels in one call, and this frame made it already.");
    }
    imagesWritten = true;
    for (const ImageWrite& write : writes) {
        const ImageDesc& image = getImage(write.image);
        if (image.kind != ImageKind::Dynamic) {
            throw std::logic_error("Only a dynamic image takes new pixels.");
        }
        if (write.pixels.size() != ImageInfo::getLevelBytes(image, 0)) {
            throw std::logic_error(std::format("The pixels do not fill the image of {}x{}.", image.width, image.height));
        }
        ++currentFrame.imageWrites;
        currentFrame.imageWriteBytes += write.pixels.size();
    }
}

// Every attachment of a pass has the size of the first one at its mip level and its sample count, and a multisampled color resolves into a render target of one sample of the same size and format.
void NullDevice::checkColorAttachment(const ColorAttachment& color, ImageInfo::Extent& extent, int& sampleCount) const {
    const ImageDesc& image = getImage(color.image);
    if (image.kind != ImageKind::RenderTarget) {
        throw std::logic_error("A pass draws its colors only into render targets.");
    }
    if (color.mipLevel < 0 || color.mipLevel >= image.mipLevels) {
        throw std::logic_error(std::format("A pass draws into the mip level {} of an image of {}.", color.mipLevel, image.mipLevels));
    }
    const ImageInfo::Extent level = ImageInfo::getLevelExtent(image, color.mipLevel);
    if (color.layer < 0 || color.layer >= level.depth) {
        throw std::logic_error(std::format("A pass draws into the layer {} of an image level of {}.", color.layer, level.depth));
    }
    if (extent.width == 0) {
        extent = level;
        sampleCount = image.sampleCount;
    }
    if (level.width != extent.width || level.height != extent.height || image.sampleCount != sampleCount) {
        throw std::logic_error("The attachments of a pass differ in their size or sample count.");
    }

    if (!color.resolve.isValid()) {
        return;
    }
    const ImageDesc& resolve = getImage(color.resolve);
    if (!features.multisampledRenderTargets || image.sampleCount == 1) {
        throw std::logic_error("A pass resolves a color that is not a multisampled render target of a device that resolves them.");
    }
    if (resolve.kind != ImageKind::RenderTarget || resolve.sampleCount != 1 || resolve.format != image.format || resolve.width != level.width || resolve.height != level.height) {
        throw std::logic_error("A multisampled color resolves into a render target of one sample with another size or format.");
    }
}

NullDevice::Targets NullDevice::describeOffscreenPass(const PassDesc& desc, OpenPass& opened) const {
    if (desc.colors.size() > kMaxColorTargets) {
        throw std::logic_error(std::format("A pass draws into {} colors, and the device holds {}.", desc.colors.size(), kMaxColorTargets));
    }
    Targets targets;
    ImageInfo::Extent extent;
    int sampleCount = 1;
    for (const ColorAttachment& color : desc.colors) {
        checkColorAttachment(color, extent, sampleCount);
        opened.images[targets.colorCount] = color.image.id;
        targets.colors[targets.colorCount++] = getImage(color.image).format;
    }

    if (desc.depthStencil) {
        const ImageDesc& depth = getImage(desc.depthStencil->image);
        if (depth.kind != ImageKind::DepthStencil) {
            throw std::logic_error("A pass tests depth and stencil only in a depth stencil image.");
        }
        if (extent.width == 0) {
            extent = ImageInfo::getLevelExtent(depth, 0);
            sampleCount = depth.sampleCount;
        }
        if (depth.width != extent.width || depth.height != extent.height || depth.sampleCount != sampleCount) {
            throw std::logic_error("The depth stencil attachment of a pass differs from its colors in size or sample count.");
        }
        opened.images[targets.colorCount] = desc.depthStencil->image.id;
        targets.depthStencil = depth.format;
    }
    targets.sampleCount = sampleCount;
    return targets;
}

void NullDevice::recordPass(bool swapchain, Load load, math::Color clear) {
    if (recordedPasses == currentFrame.passes.size()) {
        currentFrame.passes.emplace_back();
    }
    PassRecord& record = currentFrame.passes[recordedPasses++];
    record.swapchain = swapchain;
    record.load = load;
    record.clear = clear;
    record.masks.clear();
    record.draws = 0;
}

void NullDevice::beginPass(const PassDesc& desc) {
    if (pass) {
        throw std::logic_error("A pass began while another pass was open.");
    }
    OpenPass opened{.debugGroups = debugGroups.size()};
    const bool swapchain = desc.colors.empty() && !desc.depthStencil;
    if (swapchain) {
        opened.targets.swapchain = true;
    } else {
        opened.targets = describeOffscreenPass(desc, opened);
    }
    pass = opened;
    applied = {};
    bindingsApplied = false;

    const bool firstColor = !desc.colors.empty();
    recordPass(swapchain, firstColor ? desc.colors.front().load : desc.load, firstColor ? desc.colors.front().clear : desc.clear);
}

void NullDevice::applyViewport(const math::Rect& pixels) {
    requirePass("applyViewport");
    if (pixels.width < 0.0F || pixels.height < 0.0F) {
        throw std::logic_error("A viewport has a negative size.");
    }
}

void NullDevice::applyScissor(const math::Rect& pixels) {
    requirePass("applyScissor");
    if (pixels.width < 0.0F || pixels.height < 0.0F) {
        throw std::logic_error("A scissor rectangle has a negative size.");
    }
}

// A pipeline draws into attachments of the formats and sample count of the pass, and a pipeline of the swapchain draws only into the swapchain.
void NullDevice::applyPipeline(Pipeline pipeline) {
    requirePass("applyPipeline");
    const auto found = pipelines.find(pipeline.id);
    if (found == pipelines.end()) {
        throw std::logic_error(std::format("The pipeline {} does not exist.", pipeline.id));
    }
    if (found->second.targets != pass->targets) {
        throw std::logic_error("A pipeline was applied in a pass whose attachments differ from the ones it draws into.");
    }

    applied = pipeline;
    bindingsApplied = false;
    appliedBlocks = 0;
    currentFrame.passes[recordedPasses - 1].masks.push_back(found->second.firstMask);
}

void NullDevice::checkTexture(const TextureBinding& binding, Image image) const {
    const ImageDesc& desc = getImage(image);
    if (desc.type != binding.type || (desc.sampleCount > 1) != binding.multisampled) {
        throw std::logic_error(std::format("The texture slot {} of the program takes another type of image.", binding.slot));
    }
    if ((binding.sampleType == SampleType::Depth) != ImageInfo::isDepth(desc.format)) {
        throw std::logic_error(std::format("The texture slot {} of the program samples depth from an image of another kind, or color from a depth image.", binding.slot));
    }
    if (std::ranges::find(pass->images, image.id) != pass->images.end()) {
        throw std::logic_error(std::format("The texture slot {} of the program samples an attachment of the pass that draws into it.", binding.slot));
    }
}

void NullDevice::checkSampler(const SamplerBinding& binding, Sampler sampler) const {
    const auto found = samplers.find(sampler.id);
    if (found == samplers.end()) {
        throw std::logic_error(std::format("The sampler slot {} of the program is empty.", binding.slot));
    }
    if ((binding.type == SamplerType::Comparison) != found->second.compare.has_value()) {
        throw std::logic_error(std::format("The sampler slot {} of the program takes a sampler that compares depth, or one that does not.", binding.slot));
    }
}

void NullDevice::applyBindings(const Bindings& bindings) {
    const PipelineState& pipeline = getAppliedPipeline("applyBindings");
    for (std::size_t index = 0; index < pipeline.buffers.size(); ++index) {
        BufferState& buffer = getBuffer(bindings.vertexBuffers[index]);
        if (buffer.index || bindings.vertexOffsets[index] < 0 || static_cast<std::size_t>(bindings.vertexOffsets[index]) >= buffer.size) {
            throw std::logic_error(std::format("The vertex buffer {} of a draw is an index buffer or starts past its end.", index));
        }
        buffer.boundFrame = frame;
    }
    if (pipeline.indexType != IndexType::None) {
        BufferState& buffer = getBuffer(bindings.indexBuffer);
        if (!buffer.index || bindings.indexOffset < 0) {
            throw std::logic_error("An indexed pipeline needs an index buffer.");
        }
        buffer.boundFrame = frame;
    }

    const ProgramRecord& program = programs.at(pipeline.program.id);
    for (const TextureBinding& texture : program.textures) {
        checkTexture(texture, bindings.textures[static_cast<std::size_t>(texture.slot)]);
    }
    for (const SamplerBinding& sampler : program.samplers) {
        checkSampler(sampler, bindings.samplers[static_cast<std::size_t>(sampler.slot)]);
    }
    bound = bindings;
    bindingsApplied = true;
}

void NullDevice::applyUniforms(int slot, std::span<const std::byte> data) {
    const PipelineState& pipeline = getAppliedPipeline("applyUniforms");
    const ProgramRecord& program = programs.at(pipeline.program.id);
    const auto block = std::ranges::find(program.blocks, slot, &BlockBinding::slot);
    if (block == program.blocks.end()) {
        throw std::logic_error(std::format("The program has no uniform block at slot {}.", slot));
    }
    if (data.size() != block->size) {
        throw std::logic_error(std::format("The uniform block at slot {} holds {} bytes, not {}.", slot, block->size, data.size()));
    }
    appliedBlocks |= 1U << static_cast<unsigned>(slot);
}

void NullDevice::applyStencilReference(std::uint8_t) {
    requirePass("applyStencilReference");
}

void NullDevice::applyBlendColor(math::Color) {
    requirePass("applyBlendColor");
}

void NullDevice::checkIndexRange(int first, int count) const {
    const PipelineState& pipeline = pipelines.at(applied.id);
    if (pipeline.indexType == IndexType::None) {
        return;
    }
    const std::size_t indexSize = pipeline.indexType == IndexType::UInt16 ? 2 : 4;
    const std::size_t end = static_cast<std::size_t>(bound.indexOffset) + (static_cast<std::size_t>(first) + static_cast<std::size_t>(count)) * indexSize;
    if (end > getBuffer(bound.indexBuffer).size) {
        throw std::logic_error(std::format("A draw reads indices up to the byte {} of an index buffer of {}.", end, getBuffer(bound.indexBuffer).size));
    }
}

// A draw reads the bindings and every uniform block of its program applied since its pipeline, which a device may otherwise hold from another pipeline.
void NullDevice::draw(int first, int count, int instances) {
    const PipelineState& pipeline = getAppliedPipeline("draw");
    if (first < 0 || count < 0 || instances < 0) {
        throw std::logic_error("A draw needs counts of zero or more.");
    }

    const ProgramRecord& program = programs.at(pipeline.program.id);
    const bool needsBindings = !pipeline.buffers.empty() || pipeline.indexType != IndexType::None || !program.textures.empty() || !program.samplers.empty();
    if (needsBindings && !bindingsApplied) {
        throw std::logic_error("A draw happened before the bindings of its pipeline.");
    }
    for (const BlockBinding& block : program.blocks) {
        if ((appliedBlocks & (1U << static_cast<unsigned>(block.slot))) == 0) {
            throw std::logic_error(std::format("A draw happened before the uniform block at slot {} of its pipeline.", block.slot));
        }
    }
    checkIndexRange(first, count);
    ++currentFrame.passes[recordedPasses - 1].draws;
}

void NullDevice::endPass() {
    requirePass("endPass");
    if (debugGroups.size() != pass->debugGroups) {
        throw std::logic_error("A pass ended with a debug group open that it pushed.");
    }
    pass.reset();
}

// A group pushed inside a pass closes in that pass, and one pushed between passes closes between passes, the way every graphics API nests them.
void NullDevice::pushDebugGroup(std::string_view name) {
    if (name.empty()) {
        throw std::logic_error("A debug group has no name.");
    }
    debugGroups.push_back(pass.has_value());
}

void NullDevice::popDebugGroup() {
    if (debugGroups.empty()) {
        throw std::logic_error("A debug group was popped without one open.");
    }
    if (debugGroups.back() != pass.has_value() || (pass && debugGroups.size() <= pass->debugGroups)) {
        throw std::logic_error("A debug group was popped across the edge of a pass.");
    }
    debugGroups.pop_back();
}

void NullDevice::commit() {
    requireNoPass("commit");
    if (!debugGroups.empty()) {
        throw std::logic_error("The frame was committed with a debug group open.");
    }

    // The records of both frames keep their storage, so a frame that repeats the one before it allocates nothing.
    lastFrame.passes.resize(recordedPasses);
    for (std::size_t index = 0; index < recordedPasses; ++index) {
        const PassRecord& made = currentFrame.passes[index];
        PassRecord& kept = lastFrame.passes[index];
        kept.swapchain = made.swapchain;
        kept.load = made.load;
        kept.clear = made.clear;
        kept.masks.assign(made.masks.begin(), made.masks.end());
        kept.draws = made.draws;
    }
    lastFrame.imageWrites = std::exchange(currentFrame.imageWrites, 0);
    lastFrame.imageWriteBytes = std::exchange(currentFrame.imageWriteBytes, 0);
    lastFrame.bufferWrites = std::exchange(currentFrame.bufferWrites, 0);
    recordedPasses = 0;
    imagesWritten = false;
    ++frame;
}

// A minimized window has a drawable of no pixels.
void NullDevice::resizeSurface(int width, int height) {
    requireNoPass("resizeSurface");
    if (width < 0 || height < 0) {
        throw std::logic_error(std::format("A surface cannot be {}x{} pixels.", width, height));
    }
}

void NullDevice::detachSurface() {
    requireNoPass("detachSurface");
    if (!surfaceAttached) {
        throw std::logic_error("The surface was detached while the device had none.");
    }
    surfaceAttached = false;
}

void NullDevice::attachSurface(const Surface& surface) {
    requireNoPass("attachSurface");
    if (surfaceAttached) {
        throw std::logic_error("A surface was attached while the device had one.");
    }
    if (surface.width < 0 || surface.height < 0 || !isPowerOfTwo(surface.sampleCount) || surface.sampleCount > features.maxSampleCount) {
        throw std::logic_error(std::format("The surface of {}x{} pixels and {} samples does not fit the device.", surface.width, surface.height, surface.sampleCount));
    }
    surfaceAttached = true;
}

std::unique_ptr<Device::Readback> NullDevice::readImage(Image image, int width, int height) {
    requireNoPass("readImage");
    const ImageDesc& desc = getImage(image);
    if (desc.kind != ImageKind::RenderTarget || desc.sampleCount != 1) {
        throw std::logic_error("Only a render target of one sample can be read.");
    }
    if (width != desc.width || height != desc.height) {
        throw std::logic_error(std::format("A read of {}x{} pixels names a render target of {}x{}.", width, height, desc.width, desc.height));
    }
    return std::make_unique<NullReadback>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
}

} // namespace haylen::gpu
