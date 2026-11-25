#include "gpu/NullDevice.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "gpu/ImageInfo.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

std::optional<std::vector<std::uint8_t>> NullDevice::NullReadback::poll() {
    if (!std::exchange(started, true) || std::exchange(delivered, true)) {
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
        capabilities.filterableFormats[format] = !wide && !ImageInfo::isDepth(pixelFormat);
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
    return api;
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

void NullDevice::readPools(std::vector<Pool>& pools) const {
    pools.assign({
        {.name = "images", .used = static_cast<int>(images.size()), .size = kImagePoolSize},
        {.name = "buffers", .used = static_cast<int>(buffers.size()), .size = kBufferPoolSize},
        {.name = "samplers", .used = static_cast<int>(samplers.size()), .size = kSamplerPoolSize},
        {.name = "programs", .used = static_cast<int>(programs.size()), .size = kProgramPoolSize},
        {.name = "pipelines", .used = static_cast<int>(pipelines.size()), .size = kPipelinePoolSize},
    });
}

bool NullDevice::isLost() const noexcept {
    return lost;
}

Device::Buffer NullDevice::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = desc.data.empty() ? desc.size : desc.data.size();
    if (size == 0) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    if (desc.usage == Usage::Immutable && desc.data.empty()) {
        throw ContractError(ContractError::Rule::Buffer);
    }

    requireRoom(buffers.size(), kBufferPoolSize, "The graphics device has no room for another buffer. At most {} baked sprite batches and draw buffers can exist at once.");
    const std::uint64_t id = nextId++;
    buffers.emplace(id, BufferState{.size = size, .usage = desc.usage, .index = desc.index});
    return {id};
}

void NullDevice::checkImageShape(const ImageDesc& desc) const {
    if (desc.width <= 0 || desc.height <= 0 || desc.width > features.maxImageSize || desc.height > features.maxImageSize) {
        throw ContractError(ContractError::Rule::Image);
    }

    const bool layered = desc.type == TextureType::Array || desc.type == TextureType::Volume;
    const int maxLayers = desc.type == TextureType::Volume ? features.maxImageSize : features.maxArrayLayers;

    if (desc.type == TextureType::Flat && desc.layers != 1) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.type == TextureType::Cube && (desc.layers != 6 || desc.width != desc.height)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (layered && (desc.layers < 1 || desc.layers > maxLayers)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.mipLevels < 1 || desc.mipLevels > ImageInfo::getMaxMipLevels(desc)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (!isPowerOfTwo(desc.sampleCount) || desc.sampleCount > features.maxSampleCount) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.sampleCount > 1 && (desc.type != TextureType::Flat || desc.mipLevels != 1)) {
        throw ContractError(ContractError::Rule::Image);
    }
}

void NullDevice::checkImageKind(const ImageDesc& desc) {
    const bool depth = ImageInfo::isDepth(desc.format);
    if (depth != (desc.kind == ImageKind::DepthStencil)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.kind != ImageKind::Texture && !desc.levels.empty()) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.sampleCount > 1 && (desc.kind == ImageKind::Texture || desc.kind == ImageKind::Dynamic)) {
        throw ContractError(ContractError::Rule::Image);
    }

    if (desc.kind == ImageKind::Dynamic && (desc.mipLevels != 1 || desc.type != TextureType::Flat)) {
        throw ContractError(ContractError::Rule::Image);
    }
}

void NullDevice::checkTextureLevels(const ImageDesc& desc) {
    if (desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw ContractError(ContractError::Rule::Image);
    }

    for (std::size_t level = 0; level < desc.levels.size(); ++level) {
        if (desc.levels[level].size() != ImageInfo::getLevelBytes(desc, static_cast<int>(level))) {
            throw ContractError(ContractError::Rule::Image);
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
        throw ContractError(ContractError::Rule::Image);
    }

    requireRoom(images.size(), kImagePoolSize, "The graphics device has no room for another texture. At most {} textures and render targets can exist at once.");
    const std::uint64_t id = nextId++;
    ImageDesc kept = desc;
    kept.levels = {};
    kept.label = {};
    images.emplace(id, kept);
    return {id};
}

Device::Sampler NullDevice::makeSampler(const SamplerDesc& desc) {
    if (desc.maxAnisotropy < 1 || desc.maxAnisotropy > kMaxAnisotropy) {
        throw ContractError(ContractError::Rule::Sampler);
    }

    if (desc.minLod < 0.0F || desc.minLod > desc.maxLod) {
        throw ContractError(ContractError::Rule::Sampler);
    }

    requireRoom(samplers.size(), kSamplerPoolSize, "The graphics device has no room for another sampler. At most {} samplers can exist at once.");
    const std::uint64_t id = nextId++;
    SamplerDesc kept = desc;
    kept.label = {};
    samplers.emplace(id, kept);
    return {id};
}

Device::Program NullDevice::makeProgram(const ProgramDesc& desc) {
    ProgramRecord program{.blocks = {desc.blocks.begin(), desc.blocks.end()}, .textures = {desc.textures.begin(), desc.textures.end()}, .samplers = {desc.samplers.begin(), desc.samplers.end()}, .vertexBytes = desc.vertex.bytecode.size(), .fragmentBytes = desc.fragment.bytecode.size()};
    for (const BlockBinding& block : program.blocks) {
        if (block.slot < 0 || block.slot >= static_cast<int>(kMaxBlocks) || block.size == 0) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    for (const TextureBinding& texture : program.textures) {
        if (texture.slot < 0 || texture.slot >= static_cast<int>(kMaxTextures)) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    for (const SamplerBinding& sampler : program.samplers) {
        if (sampler.slot < 0 || sampler.slot >= static_cast<int>(kMaxSamplers)) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    // SPIR-V comes as the words of a module, the language of a port as the bytes its shader tool wrote, and every other language as text.
    if (language == Language::Spirv && (desc.vertex.bytecode.empty() || desc.fragment.bytecode.empty() || desc.vertex.bytecode.size() % 4 != 0 || desc.fragment.bytecode.size() % 4 != 0)) {
        throw ContractError(ContractError::Rule::Program);
    }

    if (language == Language::Port && (desc.vertex.bytecode.empty() || desc.fragment.bytecode.empty())) {
        throw ContractError(ContractError::Rule::Program);
    }

    if (language != Language::None && language != Language::Spirv && language != Language::Port && (desc.vertex.source.empty() || desc.fragment.source.empty())) {
        throw ContractError(ContractError::Rule::Program);
    }

    requireRoom(programs.size(), kProgramPoolSize, "The graphics device has no room for another shader. At most {} shader programs can exist at once.");
    const std::uint64_t id = nextId++;
    programs.emplace(id, std::move(program));
    return {id};
}

// A device without independent blending applies the blend and the write mask of the first color to every color, so the engine never asks it for different ones.
void NullDevice::checkColorTargets(const PipelineDesc& desc) const {
    if (desc.colors.size() > kMaxColorTargets || (desc.colors.empty() && !desc.depthStencil.format)) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    for (const ColorTarget& color : desc.colors) {
        if (!features.independentBlend && (color.blend != desc.colors.front().blend || color.writeMask != desc.colors.front().writeMask)) {
            throw ContractError(ContractError::Rule::Pipeline);
        }

        if (desc.swapchain) {
            continue;
        }

        if (ImageInfo::isDepth(color.format) || !features.renderFormats[static_cast<std::size_t>(color.format)]) {
            throw ContractError(ContractError::Rule::Pipeline);
        }

        if (color.blend.enabled && !features.blendableFormats[static_cast<std::size_t>(color.format)]) {
            throw ContractError(ContractError::Rule::Pipeline);
        }
    }
}

void NullDevice::checkDepthStencilState(const PipelineDesc& desc) {
    const DepthStencil& state = desc.depthStencil;
    if (state.format && !ImageInfo::isDepth(*state.format)) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    if (!state.format && (state.depthWrite || state.depthCompare != CompareFunction::Always || state.stencilEnabled)) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    if (state.stencilEnabled && state.format && !ImageInfo::hasStencil(*state.format)) {
        throw ContractError(ContractError::Rule::Pipeline);
    }
}

void NullDevice::checkVertexLayout(const PipelineDesc& desc) const {
    if (desc.buffers.size() > kMaxVertexBuffers || desc.attributes.size() > kMaxAttributes) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    for (const VertexBufferLayout& layout : desc.buffers) {
        if (layout.stride <= 0) {
            throw ContractError(ContractError::Rule::Pipeline);
        }
    }

    for (const VertexAttribute& attribute : desc.attributes) {
        if (attribute.buffer < 0 || attribute.buffer >= static_cast<int>(desc.buffers.size())) {
            throw ContractError(ContractError::Rule::Pipeline);
        }

        if (attribute.location < 0 || attribute.location >= static_cast<int>(kMaxAttributes) || attribute.offset < 0) {
            throw ContractError(ContractError::Rule::Pipeline);
        }
    }

    if (!isPowerOfTwo(desc.sampleCount) || desc.sampleCount > features.maxSampleCount) {
        throw ContractError(ContractError::Rule::Pipeline);
    }
}

Device::Pipeline NullDevice::makePipeline(const PipelineDesc& desc) {
    if (!programs.contains(desc.program.id)) {
        throw ContractError(ContractError::Rule::MissingObject);
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
    const std::uint64_t id = nextId++;
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
        throw ContractError(ContractError::Rule::MissingObject);
    }

    return found->second;
}

const NullDevice::BufferState& NullDevice::getBuffer(Buffer buffer) const {
    const auto found = buffers.find(buffer.id);
    if (found == buffers.end()) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    return found->second;
}

const Device::ImageDesc& NullDevice::getImage(Image image) const {
    const auto found = images.find(image.id);
    if (found == images.end()) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    return found->second;
}

void NullDevice::requirePass() const {
    if (!pass) {
        throw ContractError(ContractError::Rule::PassOrder);
    }
}

void NullDevice::requireNoPass() const {
    if (pass) {
        throw ContractError(ContractError::Rule::PassOrder);
    }
}

const NullDevice::PipelineState& NullDevice::getAppliedPipeline() const {
    requirePass();

    if (!applied.isValid()) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    return pipelines.at(applied.id);
}

// A frame writes each part of a buffer once, before any pass of the frame binds the buffer, so a device may place the writes of a frame where its draws read them.
void NullDevice::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    requireNoPass();
    BufferState& state = getBuffer(buffer);
    if (state.usage != Usage::Dynamic) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (offset % 4 != 0) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (offset + data.size() > state.size) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (state.boundFrame == frame) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (state.writtenFrame != frame) {
        state.writtenFrame = frame;
        state.written.clear();
    }

    const auto overlaps = [offset, end = offset + data.size()](const WrittenRange& range) { return offset < range.offset + range.size && range.offset < end; };
    if (std::ranges::any_of(state.written, overlaps)) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    state.written.push_back({.offset = offset, .size = data.size()});
    ++currentFrame.bufferWrites;
}

void NullDevice::writeImages(std::span<const ImageWrite> writes) {
    if (pass || recordedPasses > 0) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }

    if (imagesWritten) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }

    imagesWritten = true;

    for (auto write = writes.begin(); write != writes.end(); ++write) {
        const ImageDesc& image = getImage(write->image);
        if (image.kind != ImageKind::Dynamic) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        const Region& region = write->region;
        const bool inside = region.x >= 0 && region.y >= 0 && region.width > 0 && region.height > 0 && region.x + region.width <= image.width && region.y + region.height <= image.height;
        if (!inside) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        const std::size_t bytes = static_cast<std::size_t>(region.width) * static_cast<std::size_t>(region.height) * ImageInfo::getBytesPerPixel(image.format);
        if (write->pixels.size() != bytes) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        if (std::any_of(writes.begin(), write, [image = write->image](const ImageWrite& earlier) { return earlier.image == image; })) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        ++currentFrame.imageWrites;
        currentFrame.imageWriteBytes += write->pixels.size();
    }
}

// Every attachment of a pass has the size of the first one at its mip level and its sample count, and a multisampled color resolves into a render target of one sample of the same size and format.
void NullDevice::checkColorAttachment(const ColorAttachment& color, ImageInfo::Extent& extent, int& sampleCount) const {
    const ImageDesc& image = getImage(color.image);
    if (image.kind != ImageKind::RenderTarget) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (color.mipLevel < 0 || color.mipLevel >= image.mipLevels) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const ImageInfo::Extent level = ImageInfo::getLevelExtent(image, color.mipLevel);
    if (color.layer < 0 || color.layer >= level.depth) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (extent.width == 0) {
        extent = level;
        sampleCount = image.sampleCount;
    }

    if (level.width != extent.width || level.height != extent.height || image.sampleCount != sampleCount) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (!color.resolve.isValid()) {
        return;
    }

    const ImageDesc& resolve = getImage(color.resolve);

    if (!features.multisampledRenderTargets || image.sampleCount == 1) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (resolve.kind != ImageKind::RenderTarget || resolve.sampleCount != 1 || resolve.format != image.format || resolve.width != level.width || resolve.height != level.height) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }
}

NullDevice::Targets NullDevice::describeOffscreenPass(const PassDesc& desc, OpenPass& opened) const {
    if (desc.colors.size() > kMaxColorTargets) {
        throw ContractError(ContractError::Rule::PassAttachments);
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
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        if (extent.width == 0) {
            extent = ImageInfo::getLevelExtent(depth, 0);
            sampleCount = depth.sampleCount;
        }

        if (depth.width != extent.width || depth.height != extent.height || depth.sampleCount != sampleCount) {
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        opened.images[targets.colorCount] = desc.depthStencil->image.id;
        targets.depthStencil = depth.format;
    }

    targets.sampleCount = sampleCount;
    return targets;
}

void NullDevice::recordPass(bool swapchain, int surface, Load load, math::Color clear, std::size_t colors) {
    if (recordedPasses == currentFrame.passes.size()) {
        currentFrame.passes.emplace_back();
    }

    PassRecord& record = currentFrame.passes[recordedPasses++];
    record.swapchain = swapchain;
    record.surface = surface;
    record.load = load;
    record.clear = clear;
    record.colors = colors;
    record.masks.clear();
    record.draws = 0;
}

// A device may present a surface once a pass into another surface leaves it, so the swapchain passes of a frame into one surface follow one another.
void NullDevice::requireFollowingPasses(int surface) const {
    bool left = false;
    for (std::size_t index = recordedPasses; index > 0; --index) {
        const PassRecord& record = currentFrame.passes[index - 1];
        if (!record.swapchain) {
            continue;
        }

        if (record.surface != surface) {
            left = true;
            continue;
        }

        if (left) {
            throw ContractError(ContractError::Rule::Surface);
        }

        return;
    }
}

// A multisampled window keeps no color across an offscreen pass between its swapchain passes, so a swapchain pass that keeps what an earlier one of the frame drew follows it directly.
void NullDevice::requireKeptSamples(int surface) const {
    if (surfaceSamples == 1 || recordedPasses == 0) {
        return;
    }

    const PassRecord& last = currentFrame.passes[recordedPasses - 1];
    if (last.swapchain && last.surface == surface) {
        return;
    }

    for (std::size_t index = 0; index < recordedPasses; ++index) {
        const PassRecord& record = currentFrame.passes[index];
        if (record.swapchain && record.surface == surface) {
            throw ContractError(ContractError::Rule::Surface);
        }
    }
}

void NullDevice::beginPass(const PassDesc& desc) {
    if (pass) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    OpenPass opened{.debugGroups = debugGroups.size()};
    const bool swapchain = desc.colors.empty() && !desc.depthStencil;
    if (swapchain) {
        if (desc.surface != 0 && !addedSurfaces.contains(desc.surface)) {
            throw ContractError(ContractError::Rule::Surface);
        }

        requireFollowingPasses(desc.surface);

        if (desc.load == Load::Keep) {
            requireKeptSamples(desc.surface);
        }

        opened.targets.swapchain = true;
    } else {
        if (desc.surface != 0) {
            throw ContractError(ContractError::Rule::Surface);
        }

        opened.targets = describeOffscreenPass(desc, opened);
    }

    pass = opened;
    applied = {};
    bindingsApplied = false;

    const bool firstColor = !desc.colors.empty();
    recordPass(swapchain, desc.surface, firstColor ? desc.colors.front().load : desc.load, firstColor ? desc.colors.front().clear : desc.clear, desc.colors.size());
}

void NullDevice::applyViewport(const math::Rect& pixels) {
    requirePass();

    if (pixels.width < 0.0F || pixels.height < 0.0F) {
        throw ContractError(ContractError::Rule::Draw);
    }
}

void NullDevice::applyScissor(const math::Rect& pixels) {
    requirePass();

    if (pixels.width < 0.0F || pixels.height < 0.0F) {
        throw ContractError(ContractError::Rule::Draw);
    }
}

// A pipeline draws into attachments of the formats and sample count of the pass, and a pipeline of the swapchain draws only into the swapchain.
void NullDevice::applyPipeline(Pipeline pipeline) {
    requirePass();
    const auto found = pipelines.find(pipeline.id);
    if (found == pipelines.end()) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    if (found->second.targets != pass->targets) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    applied = pipeline;
    bindingsApplied = false;
    appliedBlocks = 0;
    currentFrame.passes[recordedPasses - 1].masks.push_back(found->second.firstMask);
}

void NullDevice::checkTexture(const TextureBinding& binding, Image image) const {
    const ImageDesc& desc = getImage(image);
    if (desc.type != binding.type || (desc.sampleCount > 1) != binding.multisampled) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    if ((binding.sampleType == SampleType::Depth) != ImageInfo::isDepth(desc.format)) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    if (binding.sampleType == SampleType::Float && !features.filterableFormats[static_cast<std::size_t>(desc.format)]) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    if (std::ranges::find(pass->images, image.id) != pass->images.end()) {
        throw ContractError(ContractError::Rule::Bindings);
    }
}

void NullDevice::checkSampler(const SamplerBinding& binding, Sampler sampler) const {
    const auto found = samplers.find(sampler.id);
    if (found == samplers.end()) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    if ((binding.type == SamplerType::Comparison) != found->second.compare.has_value()) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    const SamplerDesc& desc = found->second;
    const bool filters = desc.minification == Filter::Linear || desc.magnification == Filter::Linear || desc.mipmap == Filter::Linear;
    if (binding.type == SamplerType::NonFiltering && filters) {
        throw ContractError(ContractError::Rule::Bindings);
    }
}

void NullDevice::applyBindings(const Bindings& bindings) {
    const PipelineState& pipeline = getAppliedPipeline();
    for (std::size_t index = 0; index < pipeline.buffers.size(); ++index) {
        BufferState& buffer = getBuffer(bindings.vertexBuffers[index]);
        if (buffer.index || bindings.vertexOffsets[index] < 0 || static_cast<std::size_t>(bindings.vertexOffsets[index]) >= buffer.size) {
            throw ContractError(ContractError::Rule::Bindings);
        }

        buffer.boundFrame = frame;
    }

    if (pipeline.indexType != IndexType::None) {
        BufferState& buffer = getBuffer(bindings.indexBuffer);
        if (!buffer.index || bindings.indexOffset < 0) {
            throw ContractError(ContractError::Rule::PipelineState);
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
    const PipelineState& pipeline = getAppliedPipeline();
    const ProgramRecord& program = programs.at(pipeline.program.id);
    const auto block = std::ranges::find(program.blocks, slot, &BlockBinding::slot);
    if (block == program.blocks.end()) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    if (data.size() != block->size) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    appliedBlocks |= 1U << static_cast<unsigned>(slot);
}

void NullDevice::applyStencilReference(std::uint8_t) {
    requirePass();
}

void NullDevice::applyBlendColor(math::Color) {
    requirePass();
}

void NullDevice::checkIndexRange(int first, int count) const {
    const PipelineState& pipeline = pipelines.at(applied.id);
    if (pipeline.indexType == IndexType::None) {
        return;
    }

    const std::size_t indexSize = pipeline.indexType == IndexType::UInt16 ? 2 : 4;
    const std::size_t end = static_cast<std::size_t>(bound.indexOffset) + (static_cast<std::size_t>(first) + static_cast<std::size_t>(count)) * indexSize;
    if (end > getBuffer(bound.indexBuffer).size) {
        throw ContractError(ContractError::Rule::Draw);
    }
}

// A draw reads the bindings and every uniform block of its program applied since its pipeline, which a device may otherwise hold from another pipeline.
void NullDevice::draw(int first, int count, int instances) {
    const PipelineState& pipeline = getAppliedPipeline();

    if (first < 0 || count < 0 || instances < 0) {
        throw ContractError(ContractError::Rule::Draw);
    }

    const ProgramRecord& program = programs.at(pipeline.program.id);
    const bool needsBindings = !pipeline.buffers.empty() || pipeline.indexType != IndexType::None || !program.textures.empty() || !program.samplers.empty();
    if (needsBindings && !bindingsApplied) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    for (const BlockBinding& block : program.blocks) {
        if ((appliedBlocks & (1U << static_cast<unsigned>(block.slot))) == 0) {
            throw ContractError(ContractError::Rule::PipelineState);
        }
    }

    checkIndexRange(first, count);
    ++currentFrame.passes[recordedPasses - 1].draws;
}

void NullDevice::endPass() {
    requirePass();

    if (debugGroups.size() != pass->debugGroups) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    pass.reset();
}

// A group pushed inside a pass closes in that pass, and one pushed between passes closes between passes, the way every graphics API nests them.
void NullDevice::pushDebugGroup(std::string_view name) {
    if (name.empty()) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    debugGroups.push_back(pass.has_value());
}

void NullDevice::popDebugGroup() {
    if (debugGroups.empty()) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    if (debugGroups.back() != pass.has_value() || (pass && debugGroups.size() <= pass->debugGroups)) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    debugGroups.pop_back();
}

void NullDevice::commit() {
    requireNoPass();

    if (!debugGroups.empty()) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    // The records of both frames keep their storage, so a frame that repeats the one before it allocates nothing.
    lastFrame.passes.resize(recordedPasses);

    for (std::size_t index = 0; index < recordedPasses; ++index) {
        const PassRecord& made = currentFrame.passes[index];
        PassRecord& kept = lastFrame.passes[index];
        kept.swapchain = made.swapchain;
        kept.surface = made.surface;
        kept.load = made.load;
        kept.clear = made.clear;
        kept.colors = made.colors;
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

void NullDevice::resizeSurface(const Surface::Size& size) {
    requireNoPass();
    checkSize(size);
    surfaceSize = size;
}

void NullDevice::detachSurface() {
    requireNoPass();

    if (!surfaceAttached) {
        throw ContractError(ContractError::Rule::Surface);
    }

    surfaceAttached = false;
}

void NullDevice::checkSurface(const Surface& surface, int maxSampleCount) {
    checkSize(surface.size);

    if (!isPowerOfTwo(surface.sampleCount) || surface.sampleCount > maxSampleCount) {
        throw ContractError(ContractError::Rule::Surface);
    }
}

// The platform hands the device only a drawable with an area, and a window shows its drawable at the size of the drawable or larger.
void NullDevice::checkSize(const Surface::Size& size) {
    if (!size.hasArea() || size.windowWidth < size.width || size.windowHeight < size.height) {
        throw ContractError(ContractError::Rule::Surface);
    }
}

void NullDevice::attachSurface(const Surface& surface) {
    requireNoPass();

    if (surfaceAttached) {
        throw ContractError(ContractError::Rule::Surface);
    }

    checkSurface(surface, features.maxSampleCount);
    surfaceAttached = true;
    surfaceSize = surface.size;
    surfaceSamples = surface.sampleCount;
}

int NullDevice::addSurface(const Surface& surface) {
    requireNoPass();
    checkSurface(surface, features.maxSampleCount);
    const int number = nextSurface++;
    addedSurfaces.emplace(number, surface);
    return number;
}

void NullDevice::resizeSurface(int surface, const Surface::Size& size) {
    requireNoPass();
    const auto found = addedSurfaces.find(surface);
    if (found == addedSurfaces.end()) {
        throw ContractError(ContractError::Rule::Surface);
    }

    checkSize(size);
    found->second.size = size;
}

void NullDevice::removeSurface(int surface) {
    requireNoPass();

    if (addedSurfaces.erase(surface) == 0) {
        throw ContractError(ContractError::Rule::Surface);
    }
}

std::unique_ptr<Device::Readback> NullDevice::readImage(Image image, int width, int height) {
    requireNoPass();
    const ImageDesc& desc = getImage(image);
    if (desc.kind != ImageKind::RenderTarget || desc.sampleCount != 1) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (width != desc.width || height != desc.height) {
        throw ContractError(ContractError::Rule::Readback);
    }

    return std::make_unique<NullReadback>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
}

Device::CaptureSupport NullDevice::getCaptureSupport() const {
    if (captureTool == CaptureTool::None) {
        return {.tool = CaptureTool::None, .missing = std::string(kMissingCaptureTool)};
    }

    return {.tool = captureTool, .missing = {}};
}

void NullDevice::beginCapture(const std::filesystem::path& path) {
    requireNoPass();

    if (captureTool == CaptureTool::None) {
        throw ContractError(ContractError::Rule::Capture);
    }

    if (capturePath) {
        throw ContractError(ContractError::Rule::Capture);
    }

    capturePath = path;
    captureFrame = frame;
}

std::filesystem::path NullDevice::endCapture() {
    requireNoPass();

    if (!capturePath) {
        throw ContractError(ContractError::Rule::Capture);
    }

    capturedFrames.push_back(frame - captureFrame);
    std::filesystem::path written = *std::exchange(capturePath, std::nullopt);
    written += ".capture";
    return written;
}

} // namespace haylen::gpu
