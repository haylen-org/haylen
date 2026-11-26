#include "gpu/NullDevice.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "gpu/ProgramSlots.hpp"
#include "gpu/SamplerRules.hpp"
#include "haylen/gpu/ContractError.hpp"

namespace haylen::gpu {

std::optional<std::vector<std::uint8_t>> NullDevice::NullReadback::poll() {
    if (!std::exchange(started, true) || std::exchange(delivered, true)) {
        return std::nullopt;
    }

    return std::vector<std::uint8_t>(size, 0);
}

Device::Capabilities NullDevice::getDefaultCapabilities() noexcept {
    Capabilities capabilities{.maxImageSize = kMaxImageSize, .maxArrayLayers = kMaxArrayLayers, .maxVolumeSize = kMaxVolumeSize, .maxCubeSize = kMaxImageSize, .originTopLeft = true, .independentBlend = false, .gpuTiming = false, .maxAnisotropy = kMaxAnisotropy, .maxLodBias = kMaxLodBias, .borderColors = true, .storageBuffers = true, .indirectBuffers = true, .maxUniformBlockSize = kMaxUniformBlockSize};
    capabilities.cubeArrays = true;
    capabilities.imageViews = true;
    capabilities.depthLayers = true;
    for (std::size_t format = 0; format < capabilities.formats.size(); ++format) {
        const auto pixelFormat = static_cast<PixelFormat>(format);
        const bool wide = pixelFormat == PixelFormat::R32Float || pixelFormat == PixelFormat::Rg32Float || pixelFormat == PixelFormat::Rgba32Float;
        const bool depth = ImageInfo::isDepth(pixelFormat);
        const bool compressed = ImageInfo::isCompressed(pixelFormat);
        const bool color = !depth && !compressed;
        FormatSupport& support = capabilities.formats[format];
        support.sample = true;
        support.filter = !wide && !depth;
        support.render = !compressed;
        support.blend = color && !wide;
        support.storage = color && pixelFormat != PixelFormat::Rgba8Srgb && pixelFormat != PixelFormat::Bgra8;
        support.mipmaps = color && support.filter;
        support.sampleCounts = compressed ? std::uint8_t{0} : kSampleCounts;
    }

    return capabilities;
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
    const BufferRules::Description description = BufferRules::check(desc, features);
    requireRoom(buffers.size(), kBufferPoolSize, "The graphics device has no room for another buffer. At most {} baked sprite batches and draw buffers can exist at once.");
    const std::uint64_t id = nextId++;
    buffers.emplace(id, BufferState{.description = description});
    return {id};
}

Device::Image NullDevice::makeImage(const ImageDesc& desc) {
    const ImageRules::Description description = ImageRules::check(desc, features);
    requireRoom(images.size(), kImagePoolSize, "The graphics device has no room for another texture. At most {} textures and render targets can exist at once.");
    const std::uint64_t id = nextId++;
    images.emplace(id, description);
    return {id};
}

Device::Image NullDevice::makeView(const ViewDesc& desc) {
    const ImageRules::Description description = ImageRules::checkView(getImage(desc.image), desc, features);
    requireRoom(images.size(), kImagePoolSize, "The graphics device has no room for another texture. At most {} textures and render targets can exist at once.");
    const std::uint64_t id = nextId++;
    images.emplace(id, description);
    return {id};
}

Device::Sampler NullDevice::makeSampler(const SamplerDesc& desc) {
    SamplerRules::check(desc, features);
    requireRoom(samplers.size(), kSamplerPoolSize, "The graphics device has no room for another sampler. At most {} samplers can exist at once.");
    const std::uint64_t id = nextId++;
    SamplerDesc kept = desc;
    kept.label = {};
    samplers.emplace(id, kept);
    return {id};
}

Device::Program NullDevice::makeProgram(const ProgramDesc& desc) {
    ProgramSlots::check(desc);
    if (std::ranges::any_of(desc.blocks, [this](const BlockBinding& block) { return block.size > features.maxUniformBlockSize; })) {
        throw ContractError(ContractError::Rule::Program);
    }

    ProgramRecord program{.blocks = {desc.blocks.begin(), desc.blocks.end()}, .textures = {desc.textures.begin(), desc.textures.end()}, .samplers = {desc.samplers.begin(), desc.samplers.end()}, .vertexBytes = desc.vertex.bytecode.size(), .fragmentBytes = desc.fragment.bytecode.size()};

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

        if (ImageInfo::isDepth(color.format) || !features.getFormat(color.format).render) {
            throw ContractError(ContractError::Rule::Pipeline);
        }

        if (color.blend.enabled && !features.getFormat(color.format).blend) {
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

    if (desc.swapchain) {
        return;
    }

    // An offscreen pipeline draws with the samples that every one of its attachments takes.
    for (const ColorTarget& color : desc.colors) {
        if (!ImageRules::takesSamples(features, color.format, desc.sampleCount)) {
            throw ContractError(ContractError::Rule::Pipeline);
        }
    }

    if (desc.depthStencil.format && !ImageRules::takesSamples(features, *desc.depthStencil.format, desc.sampleCount)) {
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
    const auto found = buffers.find(buffer.id);
    if (found != buffers.end()) {
        spare += found->second.description.size;
        buffers.erase(found);
    }
}

// Destroying an image destroys its views with it, while a view holds no memory of its own.
void NullDevice::destroy(Image image) noexcept {
    const auto found = images.find(image.id);
    if (found == images.end()) {
        return;
    }

    const ImageRules::Description& desc = found->second;
    if (!desc.isView()) {
        spare += ImageInfo::getRegionBytes(desc.format, desc.width, desc.height) * static_cast<std::size_t>(desc.layers);
        std::erase_if(images, [image](const auto& entry) { return entry.second.viewed == image; });
    }

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

const ImageRules::Description& NullDevice::getImage(Image image) const {
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

void NullDevice::requireNoPass(ContractError::Rule broken) const {
    if (pass) {
        throw ContractError(broken);
    }
}

const NullDevice::PipelineState& NullDevice::getAppliedPipeline() const {
    requirePass();

    if (!applied.isValid()) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    return pipelines.at(applied.id);
}

void NullDevice::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    requireNoPass(ContractError::Rule::BufferWrite);
    BufferState& state = getBuffer(buffer);
    BufferRules::checkWrite(state.description, offset, data.size());

    // A device records the write into a dynamic buffer as a copy between the passes around it.
    if (state.description.update == BufferUpdate::Dynamic) {
        interruptedSincePass = true;
    } else {
        writeStream(state, offset, data.size());
    }

    ++currentFrame.bufferWrites;
}

// A frame writes each part of a stream buffer once, before any pass of the frame binds the buffer, so a device may place the writes of a frame where its draws read them.
void NullDevice::writeStream(BufferState& state, std::size_t offset, std::size_t size) {
    if (state.boundFrame == frame) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    if (state.writtenFrame != frame) {
        state.writtenFrame = frame;
        state.written.clear();
    }

    const auto overlaps = [offset, end = offset + size](const WrittenRange& range) { return offset < range.offset + range.size && range.offset < end; };
    if (std::ranges::any_of(state.written, overlaps)) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    state.written.push_back({.offset = offset, .size = size});
}

void NullDevice::copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) {
    requireNoPass(ContractError::Rule::BufferWrite);
    BufferRules::checkCopy(source, getBuffer(source).description, sourceOffset, destination, getBuffer(destination).description, destinationOffset, size);
    interruptedSincePass = true;
    ++currentFrame.bufferCopies;
}

void NullDevice::writeImages(std::span<const ImageWrite> writes) {
    if (pass || recordedPasses > 0 || mipmapsGenerated || imagesWritten) {
        throw ContractError(ContractError::Rule::ImageWrite);
    }

    imagesWritten = true;

    for (const ImageWrite& write : writes) {
        ImageRules::checkWrite(getImage(write.image), write);
        ++currentFrame.imageWrites;
        currentFrame.imageWriteBytes += write.pixels.size();
    }

    ImageRules::checkDistinctWrites(writes);
}

void NullDevice::generateMipmaps(Image image) {
    requireNoPass(ContractError::Rule::Mipmaps);
    ImageRules::checkMipmaps(getImage(image), features);
    mipmapsGenerated = true;
    interruptedSincePass = true;
    ++currentFrame.mipmapGenerations;
}

// Every attachment of a pass has the size of the first one at its mip level and its sample count, and a multisampled color resolves into a mip level and a layer of a render target of one sample that is no volume, of the same size and format there.
void NullDevice::checkColorAttachment(const ColorAttachment& color, ImageInfo::Extent& extent, int& sampleCount) const {
    const ImageRules::Description& image = getImage(color.image);
    if (image.isView() || image.kind != ImageKind::RenderTarget) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (color.mipLevel < 0 || color.mipLevel >= image.mipLevels) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const ImageInfo::Extent level = image.getLevelExtent(color.mipLevel);
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

    const ImageRules::Description& resolve = getImage(color.resolve);
    if (image.sampleCount == 1 || resolve.isView() || resolve.kind != ImageKind::RenderTarget || resolve.type == TextureType::Volume || resolve.sampleCount != 1 || resolve.format != image.format) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (color.resolveMipLevel < 0 || color.resolveMipLevel >= resolve.mipLevels) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const ImageInfo::Extent resolved = resolve.getLevelExtent(color.resolveMipLevel);
    if (color.resolveLayer < 0 || color.resolveLayer >= resolved.depth || resolved.width != level.width || resolved.height != level.height) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }
}

void NullDevice::checkDepthStencilAttachment(const DepthStencilAttachment& depthStencil, ImageInfo::Extent& extent, int& sampleCount) const {
    const ImageRules::Description& depth = getImage(depthStencil.image);
    if (depth.isView() || depth.kind != ImageKind::DepthStencil || depthStencil.mipLevel < 0 || depthStencil.mipLevel >= depth.mipLevels) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const ImageInfo::Extent level = depth.getLevelExtent(depthStencil.mipLevel);
    if (depthStencil.layer < 0 || depthStencil.layer >= level.depth || (depthStencil.layer > 0 && !features.depthLayers)) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    if (extent.width == 0) {
        extent = level;
        sampleCount = depth.sampleCount;
    }

    if (level.width != extent.width || level.height != extent.height || depth.sampleCount != sampleCount) {
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
    std::size_t drawn = 0;
    for (const ColorAttachment& color : desc.colors) {
        checkColorAttachment(color, extent, sampleCount);
        opened.drawn[drawn++] = {.image = color.image, .mipLevel = color.mipLevel, .layer = color.layer};
        if (color.resolve.isValid()) {
            opened.drawn[drawn++] = {.image = color.resolve, .mipLevel = color.resolveMipLevel, .layer = color.resolveLayer};
        }

        targets.colors[targets.colorCount++] = getImage(color.image).format;
    }

    if (desc.depthStencil) {
        checkDepthStencilAttachment(*desc.depthStencil, extent, sampleCount);
        opened.drawn[drawn++] = {.image = desc.depthStencil->image, .mipLevel = desc.depthStencil->mipLevel, .layer = desc.depthStencil->layer};
        targets.depthStencil = getImage(desc.depthStencil->image).format;
    }

    opened.drawnCount = drawn;
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

// A multisampled window keeps no color across an offscreen pass or a debug group between its swapchain passes, so a swapchain pass that keeps what an earlier one of the frame drew follows it directly.
void NullDevice::requireKeptSamples(int surface) const {
    if (surfaceSamples == 1 || recordedPasses == 0) {
        return;
    }

    const PassRecord& last = currentFrame.passes[recordedPasses - 1];
    if (last.swapchain && last.surface == surface && !interruptedSincePass) {
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
    interruptedSincePass = false;

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
    bufferBlocks = 0;
    currentFrame.passes[recordedPasses - 1].masks.push_back(found->second.firstMask);
}

// A texture slot takes an image or a view of its type that the pass does not draw or resolve into at any mip level and layer the slot reads.
void NullDevice::checkTexture(const TextureBinding& binding, Image image) const {
    const ImageRules::Description& desc = getImage(image);
    if (desc.type != binding.type || (desc.sampleCount > 1) != binding.multisampled) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    if ((binding.sampleType == SampleType::Depth) != ImageInfo::isDepth(desc.format)) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    if (binding.sampleType == SampleType::Float && !features.getFormat(desc.format).filter) {
        throw ContractError(ContractError::Rule::Bindings);
    }

    for (std::size_t index = 0; index < pass->drawnCount; ++index) {
        const Subresource& drawn = pass->drawn[index];
        if (ImageRules::reaches(image, desc, drawn.image, drawn.mipLevel, drawn.layer)) {
            throw ContractError(ContractError::Rule::Bindings);
        }
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
        BufferRules::checkVertex(buffer.description);
        if (bindings.vertexOffsets[index] < 0 || static_cast<std::size_t>(bindings.vertexOffsets[index]) >= buffer.description.size) {
            throw ContractError(ContractError::Rule::Bindings);
        }

        buffer.boundFrame = frame;
    }

    if (pipeline.indexType != IndexType::None) {
        BufferState& buffer = getBuffer(bindings.indexBuffer);
        BufferRules::checkIndex(buffer.description);
        if (bindings.indexOffset < 0) {
            throw ContractError(ContractError::Rule::Bindings);
        }

        buffer.boundFrame = frame;
    }

    const ProgramRecord& program = programs.at(pipeline.program.id);
    bufferBlocks = bindUniformBuffers(program, bindings);
    for (const TextureBinding& texture : program.textures) {
        checkTexture(texture, bindings.textures[static_cast<std::size_t>(texture.slot)]);
    }

    for (const SamplerBinding& sampler : program.samplers) {
        checkSampler(sampler, bindings.samplers[static_cast<std::size_t>(sampler.slot)]);
    }

    bound = bindings;
    bindingsApplied = true;
}

// A uniform block of the program reads the uniform buffer that the bindings name at its slot, which `applyUniforms` never fed since the pipeline, so no device has to choose between the two.
std::uint32_t NullDevice::bindUniformBuffers(const ProgramRecord& program, const Bindings& bindings) {
    std::uint32_t fed = 0;
    for (const BlockBinding& block : program.blocks) {
        const BufferRange& range = bindings.uniformBuffers[static_cast<std::size_t>(block.slot)];
        if (!range.buffer.isValid()) {
            continue;
        }

        const std::uint32_t bit = 1U << static_cast<unsigned>(block.slot);
        if ((appliedBlocks & bit) != 0) {
            throw ContractError(ContractError::Rule::Uniforms);
        }

        BufferState& buffer = getBuffer(range.buffer);
        BufferRules::checkUniform(buffer.description, range.offset, block.size);
        buffer.boundFrame = frame;
        fed |= bit;
    }

    return fed;
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

    const std::uint32_t bit = 1U << static_cast<unsigned>(slot);
    if ((bufferBlocks & bit) != 0) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    appliedBlocks |= bit;
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
    if (end > getBuffer(bound.indexBuffer).description.size) {
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
        if (((appliedBlocks | bufferBlocks) & (1U << static_cast<unsigned>(block.slot))) == 0) {
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
    interruptedSincePass = interruptedSincePass || !pass;
}

void NullDevice::popDebugGroup() {
    if (debugGroups.empty()) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    if (debugGroups.back() != pass.has_value() || (pass && debugGroups.size() <= pass->debugGroups)) {
        throw ContractError(ContractError::Rule::DebugGroup);
    }

    debugGroups.pop_back();
    interruptedSincePass = interruptedSincePass || !pass;
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
        presentedSinceWait = presentedSinceWait || made.swapchain;
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
    lastFrame.bufferCopies = std::exchange(currentFrame.bufferCopies, 0);
    lastFrame.mipmapGenerations = std::exchange(currentFrame.mipmapGenerations, 0);
    recordedPasses = 0;
    imagesWritten = false;
    mipmapsGenerated = false;
    ++frame;
}

bool NullDevice::paceWindow(int framesPerSecond, std::function<void()>) {
    requireNoPass();

    if (framesPerSecond < 0) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return false;
}

// The platform waits between frames, where readbacks that it started after a commit may already have opened the next frame, so only a pass that is open breaks the contract.
void NullDevice::waitForDisplay() {
    requireNoPass();

    ++displayWaits;
    if (std::exchange(presentedSinceWait, false)) {
        ++presentedWaits;
    }
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

// A window draws into a color image of the swapchain and its depth and stencil, which both take its samples.
void NullDevice::checkSurface(const Surface& surface) const {
    checkSize(surface.size);

    if (!ImageRules::takesSamples(features, PixelFormat::Rgba8, surface.sampleCount) || !ImageRules::takesSamples(features, PixelFormat::Depth24Stencil8, surface.sampleCount)) {
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

    checkSurface(surface);
    surfaceAttached = true;
    surfaceSize = surface.size;
    surfaceSamples = surface.sampleCount;
}

int NullDevice::addSurface(const Surface& surface) {
    requireNoPass();
    checkSurface(surface);
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
    const ImageRules::Description& desc = getImage(image);
    if (desc.isView() || desc.kind != ImageKind::RenderTarget || desc.sampleCount != 1) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (width != desc.width || height != desc.height) {
        throw ContractError(ContractError::Rule::Readback);
    }

    return std::make_unique<NullReadback>(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
}

std::unique_ptr<Device::Readback> NullDevice::readBuffer(Buffer buffer, std::size_t offset, std::size_t size) {
    requireNoPass(ContractError::Rule::Readback);
    BufferRules::checkRead(getBuffer(buffer).description, offset, size);
    return std::make_unique<NullReadback>(size);
}

Device::CaptureSupport NullDevice::getCaptureSupport() const {
    if (captureTool == CaptureTool::None) {
        return {.tool = CaptureTool::None, .missing = std::string(kMissingCaptureTool)};
    }

    return {.tool = captureTool, .missing = {}};
}

void NullDevice::trim() {
    requireNoPass();
    spare = 0;
    ++trims;
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
