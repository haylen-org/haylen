#include "gpu/webgpu/Device.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <stdexcept>

#include "gpu/ImageInfo.hpp"
#include "gpu/webgpu/Conversion.hpp"
#include "gpu/webgpu/ImageReadback.hpp"

namespace haylen::gpu::webgpu {

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& surface, const DeviceFactory::Options&) {
    return std::make_unique<Device>(Connection::take(), surface);
}

Device::Device(std::unique_ptr<Connection> owned, const std::optional<Surface>& surface) : connection(std::move(owned)), device(connection->getDevice()), queue(connection->getQueue()), capabilities(describeCapabilities(*connection)), ring(device, connection->getLimits().minUniformBufferOffsetAlignment) {
    if (connection->hasFeature(WGPUFeatureName_TimestampQuery)) {
        timer = std::make_unique<FrameTimer>(device);
    }
    if (surface) {
        attachSurface(*surface);
    }
}

Device::~Device() {
    if (pass != nullptr) {
        wgpuRenderPassEncoderRelease(pass);
    }
    if (encoder != nullptr) {
        wgpuCommandEncoderRelease(encoder);
    }
    swapchain.reset();

    pipelines.forEach([](PipelineRecord& record) { wgpuRenderPipelineRelease(record.pipeline); });
    // clang-format off
    programs.forEach([](ProgramRecord& record) {
        wgpuShaderModuleRelease(record.vertex);
        wgpuShaderModuleRelease(record.fragment);
    });
    // clang-format on
    samplers.forEach([](SamplerRecord& record) { wgpuSamplerRelease(record.sampler); });
    images.forEach([this](ImageRecord& record) { releaseImage(record); });
    buffers.forEach([this](BufferRecord& record) { retire(Retired::Kind::Buffer, record.buffer); });
    releaseRetired();

    for (const auto& [source, module] : modules) {
        wgpuShaderModuleRelease(module.module);
    }
    for (const auto& [key, layout] : pipelineLayouts) {
        wgpuPipelineLayoutRelease(layout);
    }
    for (const auto& [key, layout] : imageLayouts) {
        wgpuBindGroupLayoutRelease(layout);
    }
    for (const auto& [key, layout] : blockLayouts) {
        for (const WGPUBindGroup group : layout->groups) {
            if (group != nullptr) {
                wgpuBindGroupRelease(group);
            }
        }
        wgpuBindGroupLayoutRelease(layout->layout);
    }
}

// WebGPU renders and samples every color format of the RHI, while 32-bit floats filter and blend only with their features, and the depth format with a 32-bit float and stencil exists only with its own.
gpu::Device::Capabilities Device::describeCapabilities(const Connection& connection) {
    const WGPULimits& limits = connection.getLimits();
    Capabilities result;
    result.maxImageSize = static_cast<int>(limits.maxTextureDimension2D);
    result.maxArrayLayers = static_cast<int>(limits.maxTextureArrayLayers);
    result.maxSampleCount = 4;
    result.originTopLeft = true;
    result.independentBlend = true;
    result.multisampledRenderTargets = true;
    result.gpuTiming = connection.hasFeature(WGPUFeatureName_TimestampQuery);

    const bool floatBlending = connection.hasFeature(WGPUFeatureName_Float32Blendable);
    const bool floatFiltering = connection.hasFeature(WGPUFeatureName_Float32Filterable);
    const bool depthStencil32 = connection.hasFeature(WGPUFeatureName_Depth32FloatStencil8);
    for (std::size_t index = 0; index < result.textureFormats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        const bool exists = format != PixelFormat::Depth32FloatStencil8 || depthStencil32;
        const bool wideFloat = format == PixelFormat::R32Float || format == PixelFormat::Rg32Float || format == PixelFormat::Rgba32Float;
        result.textureFormats[index] = exists;
        result.renderFormats[index] = exists;
        result.blendableFormats[index] = !ImageInfo::isDepth(format) && (!wideFloat || floatBlending);
        result.filterableFormats[index] = !ImageInfo::isDepth(format) && (!wideFloat || floatFiltering);
    }
    return result;
}

std::uint64_t Device::alignTo(std::uint64_t value, std::uint64_t alignment) noexcept {
    return (value + alignment - 1U) / alignment * alignment;
}

gpu::Device::Language Device::getLanguage() const noexcept {
    return Language::Wgsl;
}

gpu::Device::Api Device::getApi() const noexcept {
    return Api::WebGpu;
}

const gpu::Device::Capabilities& Device::getCapabilities() const noexcept {
    return capabilities;
}

std::string Device::getAdapterName() const {
    return connection->getAdapterName();
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    return timer ? timer->getSeconds() : std::nullopt;
}

std::vector<gpu::Device::Pool> Device::getPools() const {
    return {
        {.name = "buffers", .used = buffers.size(), .size = buffers.capacity()}, {.name = "images", .used = images.size(), .size = images.capacity()}, {.name = "samplers", .used = samplers.size(), .size = samplers.capacity()}, {.name = "programs", .used = programs.size(), .size = programs.capacity()}, {.name = "pipelines", .used = pipelines.size(), .size = pipelines.capacity()}, {.name = "bindGroups", .used = static_cast<int>(imageGroups.size()), .size = static_cast<int>(imageGroups.capacity())},
    };
}

bool Device::isLost() const noexcept {
    return connection->isLost();
}

// Buffers with data start mapped, which hands their contents over without a copy through the queue, and every size is a multiple of four bytes, which WebGPU writes in.
gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    const std::size_t size = std::max(desc.size, desc.data.size());
    if (size == 0) {
        throw std::runtime_error("The graphics device cannot create a buffer without bytes.");
    }

    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(desc.label);
    descriptor.usage = (desc.index ? WGPUBufferUsage_Index : WGPUBufferUsage_Vertex) | WGPUBufferUsage_CopyDst;
    descriptor.size = alignTo(size, 4);
    descriptor.mappedAtCreation = !desc.data.empty();
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(device, &descriptor);
    if (buffer == nullptr) {
        throw std::runtime_error(std::format("The graphics device found no memory for a buffer of {} bytes.", size));
    }

    if (!desc.data.empty()) {
        std::memcpy(wgpuBufferGetMappedRange(buffer, 0, static_cast<std::size_t>(descriptor.size)), desc.data.data(), desc.data.size());
        wgpuBufferUnmap(buffer);
    }
    return {buffers.add({.buffer = buffer, .size = static_cast<std::size_t>(descriptor.size)})};
}

void Device::validateImage(const ImageDesc& desc) const {
    if (!capabilities.textureFormats[static_cast<std::size_t>(desc.format)]) {
        throw std::runtime_error("This WebGPU adapter has no depth format with a 32-bit float and stencil.");
    }
    if (desc.width < 1 || desc.height < 1 || desc.width > capabilities.maxImageSize || desc.height > capabilities.maxImageSize) {
        throw std::runtime_error(std::format("The graphics device cannot create a {}x{} image, since its sides are between 1 and {} pixels.", desc.width, desc.height, capabilities.maxImageSize));
    }
    if (desc.layers < 1 || (desc.type != TextureType::Volume && desc.layers > capabilities.maxArrayLayers)) {
        throw std::runtime_error(std::format("The graphics device cannot create an image of {} layers, since it holds between 1 and {}.", desc.layers, capabilities.maxArrayLayers));
    }
    if (desc.type == TextureType::Cube && (desc.layers != 6 || desc.width != desc.height)) {
        throw std::runtime_error("A cube image has six square faces.");
    }
    if (desc.mipLevels < 1 || desc.mipLevels > ImageInfo::getMaxMipLevels(desc)) {
        throw std::runtime_error(std::format("The graphics device cannot create a {}x{} image with {} mip levels.", desc.width, desc.height, desc.mipLevels));
    }
    if (desc.sampleCount != 1 && desc.sampleCount != 4) {
        throw std::runtime_error(std::format("WebGPU draws images of one or four samples, not {}.", desc.sampleCount));
    }
    if (desc.sampleCount > 1 && (desc.type != TextureType::Flat || desc.mipLevels > 1 || desc.kind == ImageKind::Texture || desc.kind == ImageKind::Dynamic)) {
        throw std::runtime_error("A multisampled image is a flat render target or depth stencil image of one mip level.");
    }
    if (ImageInfo::isDepth(desc.format) != (desc.kind == ImageKind::DepthStencil)) {
        throw std::runtime_error("Depth formats belong to depth stencil images, and only to them.");
    }
    if (desc.kind == ImageKind::Texture && desc.levels.size() != static_cast<std::size_t>(desc.mipLevels)) {
        throw std::runtime_error(std::format("A texture of {} mip levels comes with the pixels of each, not of {}.", desc.mipLevels, desc.levels.size()));
    }
}

void Device::writeLevel(WGPUTexture texture, int level, int width, int height, int depth, PixelFormat format, std::span<const std::byte> pixels) {
    const std::size_t rowBytes = static_cast<std::size_t>(width) * ImageInfo::getBytesPerPixel(format);
    const std::size_t bytes = rowBytes * static_cast<std::size_t>(height) * static_cast<std::size_t>(depth);
    if (pixels.size() < bytes) {
        throw std::runtime_error(std::format("The mip level {} of a {}x{} image holds {} bytes, not {}.", level, width, height, bytes, pixels.size()));
    }

    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture;
    destination.mipLevel = static_cast<std::uint32_t>(level);
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = static_cast<std::uint32_t>(rowBytes);
    layout.rowsPerImage = static_cast<std::uint32_t>(height);
    const WGPUExtent3D extent{.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height), .depthOrArrayLayers = static_cast<std::uint32_t>(depth)};
    wgpuQueueWriteTexture(queue, &destination, pixels.data(), bytes, &layout, &extent);
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    validateImage(desc);

    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(desc.label);
    descriptor.dimension = Conversion::toDimension(desc.type);
    descriptor.size = {.width = static_cast<std::uint32_t>(desc.width), .height = static_cast<std::uint32_t>(desc.height), .depthOrArrayLayers = static_cast<std::uint32_t>(desc.layers)};
    descriptor.format = Conversion::toTextureFormat(desc.format);
    descriptor.mipLevelCount = static_cast<std::uint32_t>(desc.mipLevels);
    descriptor.sampleCount = static_cast<std::uint32_t>(desc.sampleCount);
    switch (desc.kind) {
    case ImageKind::Texture:
    case ImageKind::Dynamic:
        descriptor.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        break;
    case ImageKind::RenderTarget:
        descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | (desc.sampleCount == 1 ? WGPUTextureUsage_CopySrc : WGPUTextureUsage_None);
        break;
    case ImageKind::DepthStencil:
        descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
        break;
    }
    WGPUTexture texture = wgpuDeviceCreateTexture(device, &descriptor);

    // Draws sample the depth of a depth stencil image, and a multisampled image as one flat image.
    WGPUTextureViewDescriptor view = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    view.dimension = desc.sampleCount > 1 ? WGPUTextureViewDimension_2D : Conversion::toViewDimension(desc.type);
    view.aspect = ImageInfo::isDepth(desc.format) ? WGPUTextureAspect_DepthOnly : WGPUTextureAspect_All;
    ImageRecord record{.texture = texture, .sampled = wgpuTextureCreateView(texture, &view), .type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .sampleCount = desc.sampleCount, .format = desc.format, .kind = desc.kind};

    for (std::size_t level = 0; level < desc.levels.size() && desc.kind == ImageKind::Texture; ++level) {
        const ImageInfo::Extent extent = ImageInfo::getLevelExtent(desc, static_cast<int>(level));
        writeLevel(texture, static_cast<int>(level), extent.width, extent.height, extent.depth, desc.format, desc.levels[level]);
    }
    return {images.add(std::move(record))};
}

// Anisotropic filtering exists only for linear filters, so a sampler with a nearest filter samples one texel.
gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    const bool linear = desc.minification == Filter::Linear && desc.magnification == Filter::Linear && desc.mipmap == Filter::Linear;

    WGPUSamplerDescriptor descriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(desc.label);
    descriptor.addressModeU = Conversion::toAddressMode(desc.wrapU);
    descriptor.addressModeV = Conversion::toAddressMode(desc.wrapV);
    descriptor.addressModeW = Conversion::toAddressMode(desc.wrapW);
    descriptor.magFilter = Conversion::toFilter(desc.magnification);
    descriptor.minFilter = Conversion::toFilter(desc.minification);
    descriptor.mipmapFilter = Conversion::toMipmapFilter(desc.mipmap);
    descriptor.lodMinClamp = desc.minLod;
    descriptor.lodMaxClamp = std::max(desc.minLod, desc.maxLod);
    descriptor.compare = desc.compare ? Conversion::toCompare(*desc.compare) : WGPUCompareFunction_Undefined;
    descriptor.maxAnisotropy = static_cast<std::uint16_t>(linear ? std::clamp(desc.maxAnisotropy, 1, 16) : 1);
    return {samplers.add({.sampler = wgpuDeviceCreateSampler(device, &descriptor)})};
}

// Programs that run the same source share its module, and a module that `prepareProgram` made earlier is ready to use.
WGPUShaderModule Device::acquireModule(std::string_view source, std::string_view label, const std::string*& key) {
    auto found = modules.find(std::string(source));
    if (found == modules.end()) {
        WGPUShaderSourceWGSL code = WGPU_SHADER_SOURCE_WGSL_INIT;
        code.code = Conversion::toText(source);
        WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
        descriptor.nextInChain = &code.chain;
        descriptor.label = Conversion::toText(label);
        found = modules.emplace(std::string(source), ShaderModule{.module = wgpuDeviceCreateShaderModule(device, &descriptor)}).first;
    }
    key = &found->first;
    return found->second.module;
}

void Device::releaseModule(const std::string* key) noexcept {
    const auto found = modules.find(*key);
    if (found == modules.end() || --found->second.programs > 0) {
        return;
    }
    retire(Retired::Kind::ShaderModule, found->second.module);
    modules.erase(found);
}

// The layouts of the bind groups are shared by the programs whose bindings match, so their bind groups serve each other.
Device::BlockLayout& Device::getBlockLayout(std::span<const BlockBinding> blocks) {
    std::vector<BlockBinding> sorted(blocks.begin(), blocks.end());
    std::ranges::sort(sorted, {}, &BlockBinding::binding);
    std::vector<std::uint64_t> key;
    for (const BlockBinding& block : sorted) {
        key.push_back((static_cast<std::uint64_t>(block.binding) << 40U) | (static_cast<std::uint64_t>(block.stage) << 32U) | block.size);
    }
    const auto found = blockLayouts.find(key);
    if (found != blockLayouts.end()) {
        return *found->second;
    }

    auto layout = std::make_unique<BlockLayout>();
    std::vector<WGPUBindGroupLayoutEntry> entries;
    for (const BlockBinding& block : sorted) {
        WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        entry.binding = static_cast<std::uint32_t>(block.binding);
        entry.visibility = Conversion::toStage(block.stage);
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        entry.buffer.hasDynamicOffset = 1U;
        entry.buffer.minBindingSize = block.size;
        entries.push_back(entry);
        layout->blocks.emplace_back(static_cast<std::uint32_t>(block.binding), block.size);
    }
    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-uniform-blocks");
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    layout->layout = wgpuDeviceCreateBindGroupLayout(device, &descriptor);
    return *blockLayouts.emplace(std::move(key), std::move(layout)).first->second;
}

WGPUBindGroupLayout Device::getImageLayout(std::span<const TextureBinding> textures, std::span<const SamplerBinding> textureSamplers) {
    std::vector<std::uint64_t> key;
    for (const TextureBinding& texture : textures) {
        key.push_back((static_cast<std::uint64_t>(texture.binding) << 32U) | (static_cast<std::uint64_t>(texture.stage) << 24U) | (static_cast<std::uint64_t>(texture.type) << 16U) | (static_cast<std::uint64_t>(texture.sampleType) << 8U) | (texture.multisampled ? 1U : 0U));
    }
    for (const SamplerBinding& sampler : textureSamplers) {
        key.push_back((std::uint64_t{1} << 63U) | (static_cast<std::uint64_t>(sampler.binding) << 32U) | (static_cast<std::uint64_t>(sampler.stage) << 24U) | static_cast<std::uint64_t>(sampler.type));
    }
    const auto found = imageLayouts.find(key);
    if (found != imageLayouts.end()) {
        return found->second;
    }

    std::vector<WGPUBindGroupLayoutEntry> entries;
    for (const TextureBinding& texture : textures) {
        WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        entry.binding = static_cast<std::uint32_t>(texture.binding);
        entry.visibility = Conversion::toStage(texture.stage);
        entry.texture.sampleType = Conversion::toSampleType(texture.sampleType);
        entry.texture.viewDimension = texture.multisampled ? WGPUTextureViewDimension_2D : Conversion::toViewDimension(texture.type);
        entry.texture.multisampled = texture.multisampled ? 1U : 0U;
        entries.push_back(entry);
    }
    for (const SamplerBinding& sampler : textureSamplers) {
        WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        entry.binding = static_cast<std::uint32_t>(sampler.binding);
        entry.visibility = Conversion::toStage(sampler.stage);
        entry.sampler.type = Conversion::toSamplerType(sampler.type);
        entries.push_back(entry);
    }
    WGPUBindGroupLayoutDescriptor descriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-images");
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    WGPUBindGroupLayout layout = wgpuDeviceCreateBindGroupLayout(device, &descriptor);
    imageLayouts.emplace(std::move(key), layout);
    return layout;
}

WGPUPipelineLayout Device::getPipelineLayout(WGPUBindGroupLayout blocks, WGPUBindGroupLayout textures) {
    const auto found = pipelineLayouts.find({blocks, textures});
    if (found != pipelineLayouts.end()) {
        return found->second;
    }
    const std::array<WGPUBindGroupLayout, 2> groups{blocks, textures};
    WGPUPipelineLayoutDescriptor descriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    descriptor.bindGroupLayoutCount = groups.size();
    descriptor.bindGroupLayouts = groups.data();
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(device, &descriptor);
    pipelineLayouts.emplace(std::pair{blocks, textures}, layout);
    return layout;
}

// WebGPU programs of Haylen keep their uniform blocks in bind group 0 and their textures and samplers in bind group 1, as the shader compiler writes them.
gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    const bool blocksInGroupZero = std::ranges::all_of(desc.blocks, [](const BlockBinding& block) { return block.set == 0; });
    const bool imagesInGroupOne = std::ranges::all_of(desc.textures, [](const TextureBinding& texture) { return texture.set == 1; }) && std::ranges::all_of(desc.samplers, [](const SamplerBinding& sampler) { return sampler.set == 1; });
    if (!blocksInGroupZero || !imagesInGroupOne) {
        throw std::runtime_error(std::format("The program \"{}\" places its bindings in other bind groups than the WebGPU programs of Haylen, which keep uniform blocks in group 0 and textures and samplers in group 1.", desc.label));
    }

    ProgramRecord record{.vertexEntry = std::string(desc.vertex.entry), .fragmentEntry = std::string(desc.fragment.entry), .textures = {desc.textures.begin(), desc.textures.end()}, .samplers = {desc.samplers.begin(), desc.samplers.end()}};
    record.vertex = acquireModule(desc.vertex.source, desc.label, record.vertexSource);
    record.fragment = acquireModule(desc.fragment.source, desc.label, record.fragmentSource);
    ++modules.find(*record.vertexSource)->second.programs;
    ++modules.find(*record.fragmentSource)->second.programs;
    wgpuShaderModuleAddRef(record.vertex);
    wgpuShaderModuleAddRef(record.fragment);

    record.blocks = &getBlockLayout(desc.blocks);
    record.images = getImageLayout(desc.textures, desc.samplers);
    record.layout = getPipelineLayout(record.blocks->layout, record.images);

    std::vector<BlockBinding> sorted(desc.blocks.begin(), desc.blocks.end());
    std::ranges::sort(sorted, {}, &BlockBinding::binding);
    for (const BlockBinding& block : sorted) {
        record.blockSlots[record.blockCount++] = block.slot;
    }
    return {programs.add(std::move(record))};
}

// The browser compiles a module in the process of its GPU once it is made, so making the modules early lets that run while the app loads.
void Device::prepareProgram(const ProgramDesc& desc, core::JobSystem&) {
    const std::string* key = nullptr;
    static_cast<void>(acquireModule(desc.vertex.source, desc.label, key));
    static_cast<void>(acquireModule(desc.fragment.source, desc.label, key));
}

Device::SwapchainPipeline Device::SwapchainPipeline::from(const PipelineDesc& desc) {
    return {.program = desc.program, .buffers = {desc.buffers.begin(), desc.buffers.end()}, .attributes = {desc.attributes.begin(), desc.attributes.end()}, .primitive = desc.primitive, .indexType = desc.indexType, .colors = {desc.colors.begin(), desc.colors.end()}, .depthStencil = desc.depthStencil, .cullMode = desc.cullMode, .frontFace = desc.frontFace, .alphaToCoverage = desc.alphaToCoverage, .label = std::string(desc.label)};
}

gpu::Device::PipelineDesc Device::SwapchainPipeline::describe() const noexcept {
    return {.program = program, .buffers = buffers, .attributes = attributes, .primitive = primitive, .indexType = indexType, .colors = colors, .depthStencil = depthStencil, .cullMode = cullMode, .frontFace = frontFace, .alphaToCoverage = alphaToCoverage, .swapchain = true, .label = label};
}

// A pipeline of the swapchain draws into its color format, its depth and stencil format and its sample count, keeping the depth and stencil tests the description asks for, or none.
WGPURenderPipeline Device::createPipeline(const PipelineDesc& desc) {
    const ProgramRecord& program = programs.get(desc.program.id);

    std::array<std::vector<WGPUVertexAttribute>, kMaxVertexBuffers> attributes;
    for (const VertexAttribute& attribute : desc.attributes) {
        attributes.at(static_cast<std::size_t>(attribute.buffer)).push_back({.format = Conversion::toVertexFormat(attribute.format), .offset = static_cast<std::uint64_t>(attribute.offset), .shaderLocation = static_cast<std::uint32_t>(attribute.location)});
    }
    std::array<WGPUVertexBufferLayout, kMaxVertexBuffers> layouts{};
    for (std::size_t index = 0; index < desc.buffers.size(); ++index) {
        layouts[index] = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        layouts[index].stepMode = desc.buffers[index].perInstance ? WGPUVertexStepMode_Instance : WGPUVertexStepMode_Vertex;
        layouts[index].arrayStride = static_cast<std::uint64_t>(desc.buffers[index].stride);
        layouts[index].attributeCount = attributes[index].size();
        layouts[index].attributes = attributes[index].data();
    }

    std::array<WGPUBlendState, kMaxColorTargets> blends{};
    std::array<WGPUColorTargetState, kMaxColorTargets> targets{};
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorTarget& color = desc.colors[index];
        blends[index] = {.color = Conversion::toBlendComponent(color.blend.sourceColor, color.blend.destinationColor, color.blend.colorOperation), .alpha = Conversion::toBlendComponent(color.blend.sourceAlpha, color.blend.destinationAlpha, color.blend.alphaOperation)};
        targets[index] = WGPU_COLOR_TARGET_STATE_INIT;
        targets[index].format = Conversion::toTextureFormat(desc.swapchain ? swapchainFormat : color.format);
        targets[index].blend = color.blend.enabled ? &blends[index] : nullptr;
        targets[index].writeMask = Conversion::toWriteMask(color.writeMask);
    }

    const DepthStencil& tests = desc.depthStencil;
    const std::optional<PixelFormat> depthFormat = desc.swapchain ? std::optional(Swapchain::kDepthFormat) : tests.format;
    const bool triangles = desc.primitive == Primitive::Triangles || desc.primitive == Primitive::TriangleStrip;
    WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
    if (depthFormat) {
        depthStencil.format = Conversion::toTextureFormat(*depthFormat);
        depthStencil.depthWriteEnabled = tests.format && tests.depthWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
        depthStencil.depthCompare = tests.format ? Conversion::toCompare(tests.depthCompare) : WGPUCompareFunction_Always;
        if (tests.format && tests.stencilEnabled) {
            depthStencil.stencilFront = Conversion::toStencilFace(tests.stencilFront);
            depthStencil.stencilBack = Conversion::toStencilFace(tests.stencilBack);
            depthStencil.stencilReadMask = tests.stencilReadMask;
            depthStencil.stencilWriteMask = tests.stencilWriteMask;
        }
        depthStencil.depthBias = triangles ? static_cast<std::int32_t>(tests.depthBias) : 0;
        depthStencil.depthBiasSlopeScale = triangles ? tests.depthBiasSlope : 0.0F;
    }

    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = program.fragment;
    fragment.entryPoint = Conversion::toText(program.fragmentEntry);
    fragment.targetCount = desc.colors.size();
    fragment.targets = targets.data();

    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(desc.label);
    descriptor.layout = program.layout;
    descriptor.vertex.module = program.vertex;
    descriptor.vertex.entryPoint = Conversion::toText(program.vertexEntry);
    descriptor.vertex.bufferCount = desc.buffers.size();
    descriptor.vertex.buffers = layouts.data();
    descriptor.primitive.topology = Conversion::toTopology(desc.primitive);
    descriptor.primitive.stripIndexFormat = desc.primitive == Primitive::TriangleStrip || desc.primitive == Primitive::LineStrip ? Conversion::toIndexFormat(desc.indexType) : WGPUIndexFormat_Undefined;
    descriptor.primitive.frontFace = Conversion::toFrontFace(desc.frontFace);
    descriptor.primitive.cullMode = Conversion::toCullMode(desc.cullMode);
    descriptor.depthStencil = depthFormat ? &depthStencil : nullptr;
    descriptor.multisample.count = static_cast<std::uint32_t>(desc.swapchain ? swapchainSamples : desc.sampleCount);
    descriptor.multisample.alphaToCoverageEnabled = desc.alphaToCoverage ? 1U : 0U;
    descriptor.fragment = &fragment;
    return wgpuDeviceCreateRenderPipeline(device, &descriptor);
}

gpu::Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    PipelineRecord record{.pipeline = createPipeline(desc), .program = desc.program.id, .indexFormat = Conversion::toIndexFormat(desc.indexType)};
    if (desc.swapchain) {
        record.swapchain = SwapchainPipeline::from(desc);
    }
    return {pipelines.add(std::move(record))};
}

void Device::rebuildSwapchainPipelines() {
    // clang-format off
    pipelines.forEach([this](PipelineRecord& record) {
        if (!record.swapchain) {
            return;
        }
        retire(Retired::Kind::Pipeline, record.pipeline);
        record.pipeline = createPipeline(record.swapchain->describe());
    });
    // clang-format on
}

void Device::retire(Retired::Kind kind, void* handle) noexcept {
    retired.push_back({.kind = kind, .handle = handle});
}

void Device::releaseRetired() noexcept {
    for (const Retired& object : retired) {
        switch (object.kind) {
        case Retired::Kind::Buffer:
            wgpuBufferDestroy(static_cast<WGPUBuffer>(object.handle));
            wgpuBufferRelease(static_cast<WGPUBuffer>(object.handle));
            break;
        case Retired::Kind::Texture:
            wgpuTextureDestroy(static_cast<WGPUTexture>(object.handle));
            wgpuTextureRelease(static_cast<WGPUTexture>(object.handle));
            break;
        case Retired::Kind::TextureView:
            wgpuTextureViewRelease(static_cast<WGPUTextureView>(object.handle));
            break;
        case Retired::Kind::Sampler:
            wgpuSamplerRelease(static_cast<WGPUSampler>(object.handle));
            break;
        case Retired::Kind::Pipeline:
            wgpuRenderPipelineRelease(static_cast<WGPURenderPipeline>(object.handle));
            break;
        case Retired::Kind::ShaderModule:
            wgpuShaderModuleRelease(static_cast<WGPUShaderModule>(object.handle));
            break;
        }
    }
    retired.clear();
}

void Device::releaseImage(ImageRecord& image) noexcept {
    for (const auto& [key, view] : image.attachments) {
        retire(Retired::Kind::TextureView, view);
    }
    retire(Retired::Kind::TextureView, image.sampled);
    retire(Retired::Kind::Texture, image.texture);
}

// A destroyed object may hand its address to the next one, so the state the pass encoder holds is sent again.
void Device::destroy(Buffer buffer) noexcept {
    if (std::optional<BufferRecord> record = buffers.remove(buffer.id)) {
        retire(Retired::Kind::Buffer, record->buffer);
        bound = {};
    }
}

void Device::destroy(Image image) noexcept {
    if (std::optional<ImageRecord> record = images.remove(image.id)) {
        imageGroups.forgetImage(image.id);
        releaseImage(*record);
        bound = {};
        imagesDirty = true;
    }
}

void Device::destroy(Sampler sampler) noexcept {
    if (std::optional<SamplerRecord> record = samplers.remove(sampler.id)) {
        imageGroups.forgetSampler(sampler.id);
        retire(Retired::Kind::Sampler, record->sampler);
        bound = {};
        imagesDirty = true;
    }
}

void Device::destroy(Program program) noexcept {
    if (std::optional<ProgramRecord> record = programs.remove(program.id)) {
        retire(Retired::Kind::ShaderModule, record->vertex);
        retire(Retired::Kind::ShaderModule, record->fragment);
        releaseModule(record->vertexSource);
        releaseModule(record->fragmentSource);
    }
}

void Device::destroy(Pipeline pipeline) noexcept {
    if (std::optional<PipelineRecord> record = pipelines.remove(pipeline.id)) {
        retire(Retired::Kind::Pipeline, record->pipeline);
        bound = {};
    }
}

// Queue writes take offsets and sizes in multiples of four bytes, so the last bytes of data of another size go through a padded copy that ends inside the buffer, whose size is a multiple of four.
void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    const BufferRecord& record = buffers.get(buffer.id);
    if (offset % 4U != 0) {
        throw std::logic_error(std::format("WebGPU writes buffers at offsets that are multiples of four bytes, not at {}.", offset));
    }
    if (offset + data.size() > record.size) {
        throw std::logic_error(std::format("The write of {} bytes at {} ends past the {} bytes of the buffer.", data.size(), offset, record.size));
    }

    const std::size_t whole = data.size() & ~std::size_t{3U};
    if (whole != 0) {
        wgpuQueueWriteBuffer(queue, record.buffer, offset, data.data(), whole);
    }
    if (whole == data.size()) {
        return;
    }
    padded.assign(4, std::byte{0});
    std::memcpy(padded.data(), data.data() + whole, data.size() - whole);
    wgpuQueueWriteBuffer(queue, record.buffer, offset + whole, padded.data(), 4);
}

void Device::writeImages(std::span<const ImageWrite> writes) {
    for (const ImageWrite& write : writes) {
        const ImageRecord& image = images.get(write.image.id);
        if (image.kind != ImageKind::Dynamic) {
            throw std::logic_error(std::format("The image {} is not dynamic, so it keeps the pixels it was made with.", write.image.id));
        }
        writeLevel(image.texture, 0, image.width, image.height, image.layers, image.format, write.pixels);
    }
}

void Device::openEncoder() {
    if (encoder == nullptr) {
        encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    }
}

// The view of an attachment covers one mip level and one layer, and a volume is drawn into through a view of its level with the slice the pass names.
WGPUTextureView Device::getAttachmentView(ImageRecord& image, int mipLevel, int layer) {
    const int viewLayer = image.type == TextureType::Volume ? 0 : layer;
    const std::uint32_t key = (static_cast<std::uint32_t>(mipLevel) << 16U) | static_cast<std::uint32_t>(viewLayer);
    const auto found = std::ranges::find(image.attachments, key, &std::pair<std::uint32_t, WGPUTextureView>::first);
    if (found != image.attachments.end()) {
        return found->second;
    }

    WGPUTextureViewDescriptor descriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    descriptor.dimension = image.type == TextureType::Volume ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2D;
    descriptor.baseMipLevel = static_cast<std::uint32_t>(mipLevel);
    descriptor.mipLevelCount = 1;
    descriptor.baseArrayLayer = static_cast<std::uint32_t>(viewLayer);
    descriptor.arrayLayerCount = 1;
    WGPUTextureView view = wgpuTextureCreateView(image.texture, &descriptor);
    image.attachments.emplace_back(key, view);
    return view;
}

void Device::beginPass(const PassDesc& desc) {
    if (pass != nullptr || skipping) {
        throw std::logic_error("A pass begins while another pass is open.");
    }
    openEncoder();
    if (desc.colors.empty() && !desc.depthStencil) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }
    bound = {};
    blocksDirty = true;
    imagesDirty = true;
    currentPipeline = 0;
}

// The swapchain keeps its depth and stencil between the passes of a frame, so a later pass that keeps the color keeps them too.
void Device::beginSwapchainPass(const PassDesc& desc) {
    if (!swapchain || !swapchain->acquire()) {
        skipping = true;
        return;
    }

    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = swapchain->getColorView();
    color.resolveTarget = swapchain->getResolveView();
    color.loadOp = Conversion::toLoad(desc.load);
    color.storeOp = WGPUStoreOp_Store;
    color.clearValue = Conversion::toColor(desc.clear);
    WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    depth.view = swapchain->getDepthView();
    depth.depthLoadOp = Conversion::toLoad(desc.load);
    depth.depthStoreOp = WGPUStoreOp_Store;
    depth.depthClearValue = desc.clearDepth;
    depth.stencilLoadOp = Conversion::toLoad(desc.load);
    depth.stencilStoreOp = WGPUStoreOp_Store;
    depth.stencilClearValue = desc.clearStencil;

    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(desc.label);
    descriptor.colorAttachmentCount = 1;
    descriptor.colorAttachments = &color;
    descriptor.depthStencilAttachment = &depth;
    descriptor.timestampWrites = timer ? timer->nextPass() : nullptr;
    pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);
    passWidth = swapchain->getWidth();
    passHeight = swapchain->getHeight();
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    std::array<WGPURenderPassColorAttachment, kMaxColorTargets> colors{};
    passWidth = 0;
    passHeight = 0;
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& attachment = desc.colors[index];
        ImageRecord& image = images.get(attachment.image.id);
        colors[index] = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colors[index].view = getAttachmentView(image, attachment.mipLevel, attachment.layer);
        colors[index].depthSlice = image.type == TextureType::Volume ? static_cast<std::uint32_t>(attachment.layer) : WGPU_DEPTH_SLICE_UNDEFINED;
        colors[index].resolveTarget = attachment.resolve.isValid() ? getAttachmentView(images.get(attachment.resolve.id), 0, 0) : nullptr;
        colors[index].loadOp = Conversion::toLoad(attachment.load);
        colors[index].storeOp = Conversion::toStore(attachment.store);
        colors[index].clearValue = Conversion::toColor(attachment.clear);
        if (index == 0) {
            passWidth = std::max(1, image.width >> attachment.mipLevel);
            passHeight = std::max(1, image.height >> attachment.mipLevel);
        }
    }

    WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        ImageRecord& image = images.get(attachment.image.id);
        depth.view = getAttachmentView(image, 0, 0);
        depth.depthLoadOp = Conversion::toLoad(attachment.depthLoad);
        depth.depthStoreOp = Conversion::toStore(attachment.depthStore);
        depth.depthClearValue = attachment.clearDepth;
        if (ImageInfo::hasStencil(image.format)) {
            depth.stencilLoadOp = Conversion::toLoad(attachment.stencilLoad);
            depth.stencilStoreOp = Conversion::toStore(attachment.stencilStore);
            depth.stencilClearValue = attachment.clearStencil;
        }
        if (desc.colors.empty()) {
            passWidth = image.width;
            passHeight = image.height;
        }
    }

    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText(desc.label);
    descriptor.colorAttachmentCount = desc.colors.size();
    descriptor.colorAttachments = colors.data();
    descriptor.depthStencilAttachment = desc.depthStencil ? &depth : nullptr;
    descriptor.timestampWrites = timer ? timer->nextPass() : nullptr;
    pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);
}

void Device::applyViewport(const math::Rect& pixels) {
    if (pass != nullptr) {
        wgpuRenderPassEncoderSetViewport(pass, pixels.x, pixels.y, std::max(pixels.width, 0.0F), std::max(pixels.height, 0.0F), 0.0F, 1.0F);
    }
}

// WebGPU keeps the scissor inside the attachments, which a rectangle that reaches past them is cut to.
void Device::applyScissor(const math::Rect& pixels) {
    if (pass == nullptr) {
        return;
    }
    const long left = std::clamp(std::lround(pixels.x), 0L, static_cast<long>(passWidth));
    const long top = std::clamp(std::lround(pixels.y), 0L, static_cast<long>(passHeight));
    const long right = std::clamp(std::lround(pixels.x + pixels.width), left, static_cast<long>(passWidth));
    const long bottom = std::clamp(std::lround(pixels.y + pixels.height), top, static_cast<long>(passHeight));
    wgpuRenderPassEncoderSetScissorRect(pass, static_cast<std::uint32_t>(left), static_cast<std::uint32_t>(top), static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top));
}

void Device::applyPipeline(Pipeline pipeline) {
    if (pass == nullptr) {
        return;
    }
    const PipelineRecord& record = pipelines.get(pipeline.id);
    if (record.pipeline != bound.pipeline) {
        wgpuRenderPassEncoderSetPipeline(pass, record.pipeline);
        bound.pipeline = record.pipeline;
    }
    currentPipeline = pipeline.id;
    blocksDirty = true;
    imagesDirty = true;
}

void Device::applyBindings(const Bindings& value) {
    if (pass == nullptr) {
        return;
    }
    for (std::size_t index = 0; index < kMaxVertexBuffers; ++index) {
        if (!value.vertexBuffers[index].isValid()) {
            continue;
        }
        WGPUBuffer buffer = buffers.get(value.vertexBuffers[index].id).buffer;
        if (buffer == bound.vertexBuffers[index] && value.vertexOffsets[index] == bound.vertexOffsets[index]) {
            continue;
        }
        wgpuRenderPassEncoderSetVertexBuffer(pass, static_cast<std::uint32_t>(index), buffer, static_cast<std::uint64_t>(value.vertexOffsets[index]), WGPU_WHOLE_SIZE);
        bound.vertexBuffers[index] = buffer;
        bound.vertexOffsets[index] = value.vertexOffsets[index];
    }
    imagesDirty = imagesDirty || value.textures != bindings.textures || value.samplers != bindings.samplers;
    bindings = value;
}

// A frame whose uniforms outgrow the chunk of the ring moves on to the next one, and the blocks that draws still read move with them, so one bind group reaches all of them.
void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    if (pass == nullptr) {
        return;
    }
    if (data.size() > connection->getLimits().maxUniformBufferBindingSize) {
        throw std::logic_error(std::format("A uniform block of {} bytes is larger than WebGPU binds.", data.size()));
    }
    if (!ring.fits(data.size())) {
        ring.advance();
        carryUniforms();
    }
    uniforms.at(static_cast<std::size_t>(slot)) = {.chunk = ring.getChunk(), .offset = ring.push(data), .size = static_cast<std::uint32_t>(data.size())};
    blocksDirty = true;
}

void Device::carryUniforms() noexcept {
    for (UniformBlock& block : uniforms) {
        if (block.size == 0) {
            continue;
        }
        block.offset = ring.push(ring.view(block.chunk, block.offset, block.size));
        block.chunk = ring.getChunk();
    }
}

void Device::applyStencilReference(std::uint8_t value) {
    if (pass != nullptr) {
        wgpuRenderPassEncoderSetStencilReference(pass, value);
    }
}

void Device::applyBlendColor(math::Color value) {
    if (pass != nullptr) {
        const WGPUColor color = Conversion::toColor(value);
        wgpuRenderPassEncoderSetBlendConstant(pass, &color);
    }
}

WGPUBindGroup Device::getBlockGroup(BlockLayout& layout, std::uint32_t chunk) {
    if (layout.groups.size() <= chunk) {
        layout.groups.resize(ring.getChunkCount(), nullptr);
    }
    WGPUBindGroup& group = layout.groups[chunk];
    if (group != nullptr) {
        return group;
    }

    std::vector<WGPUBindGroupEntry> entries;
    for (const auto& [binding, size] : layout.blocks) {
        WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entry.binding = binding;
        entry.buffer = ring.getBuffer(chunk);
        entry.size = size;
        entries.push_back(entry);
    }
    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-uniform-blocks");
    descriptor.layout = layout.layout;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    group = wgpuDeviceCreateBindGroup(device, &descriptor);
    return group;
}

WGPUBindGroup Device::makeImageGroup(const ProgramRecord& program, const BindGroupCache::Key& key) {
    std::vector<WGPUBindGroupEntry> entries;
    for (const TextureBinding& texture : program.textures) {
        const ImageRecord* image = images.find(key.images[static_cast<std::size_t>(texture.slot)]);
        if (image == nullptr) {
            throw std::logic_error(std::format("The draw binds no image at the texture slot {}, which the program samples.", texture.slot));
        }
        WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entry.binding = static_cast<std::uint32_t>(texture.binding);
        entry.textureView = image->sampled;
        entries.push_back(entry);
    }
    for (const SamplerBinding& sampler : program.samplers) {
        const SamplerRecord* record = samplers.find(key.samplers[static_cast<std::size_t>(sampler.slot)]);
        if (record == nullptr) {
            throw std::logic_error(std::format("The draw binds no sampler at the slot {}, which the program samples with.", sampler.slot));
        }
        WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
        entry.binding = static_cast<std::uint32_t>(sampler.binding);
        entry.sampler = record->sampler;
        entries.push_back(entry);
    }

    WGPUBindGroupDescriptor descriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-images");
    descriptor.layout = program.images;
    descriptor.entryCount = entries.size();
    descriptor.entries = entries.data();
    WGPUBindGroup group = wgpuDeviceCreateBindGroup(device, &descriptor);
    imageGroups.add(key, group, frame);
    return group;
}

void Device::bindBlocks(const ProgramRecord& program) {
    if (!blocksDirty) {
        return;
    }
    blocksDirty = false;

    std::array<std::uint32_t, kMaxBlocks> offsets{};
    for (std::uint32_t index = 0; index < program.blockCount; ++index) {
        offsets[index] = uniforms[static_cast<std::size_t>(program.blockSlots[index])].offset;
    }
    WGPUBindGroup group = getBlockGroup(*program.blocks, ring.getChunk());
    if (group == bound.blockGroup && offsets == bound.blockOffsets) {
        return;
    }
    wgpuRenderPassEncoderSetBindGroup(pass, 0, group, program.blockCount, offsets.data());
    bound.blockGroup = group;
    bound.blockOffsets = offsets;
}

void Device::bindImages(const ProgramRecord& program) {
    if (!imagesDirty) {
        return;
    }
    imagesDirty = false;

    BindGroupCache::Key key{.layout = program.images};
    for (const TextureBinding& texture : program.textures) {
        key.images[static_cast<std::size_t>(texture.slot)] = bindings.textures[static_cast<std::size_t>(texture.slot)].id;
    }
    for (const SamplerBinding& sampler : program.samplers) {
        key.samplers[static_cast<std::size_t>(sampler.slot)] = bindings.samplers[static_cast<std::size_t>(sampler.slot)].id;
    }
    WGPUBindGroup group = imageGroups.find(key, frame);
    if (group == nullptr) {
        group = makeImageGroup(program, key);
    }
    if (group != bound.imageGroup) {
        wgpuRenderPassEncoderSetBindGroup(pass, 1, group, 0, nullptr);
        bound.imageGroup = group;
    }
}

void Device::bindIndexBuffer(WGPUIndexFormat format) {
    WGPUBuffer buffer = buffers.get(bindings.indexBuffer.id).buffer;
    if (buffer == bound.indexBuffer && bindings.indexOffset == bound.indexOffset && format == bound.indexFormat) {
        return;
    }
    wgpuRenderPassEncoderSetIndexBuffer(pass, buffer, format, static_cast<std::uint64_t>(bindings.indexOffset), WGPU_WHOLE_SIZE);
    bound.indexBuffer = buffer;
    bound.indexOffset = bindings.indexOffset;
    bound.indexFormat = format;
}

void Device::draw(int first, int count, int instances) {
    if (pass == nullptr) {
        return;
    }
    const PipelineRecord& pipeline = pipelines.get(currentPipeline);
    const ProgramRecord& program = programs.get(pipeline.program);
    bindBlocks(program);
    bindImages(program);

    if (pipeline.indexFormat == WGPUIndexFormat_Undefined) {
        wgpuRenderPassEncoderDraw(pass, static_cast<std::uint32_t>(count), static_cast<std::uint32_t>(instances), static_cast<std::uint32_t>(first), 0);
        return;
    }
    bindIndexBuffer(pipeline.indexFormat);
    wgpuRenderPassEncoderDrawIndexed(pass, static_cast<std::uint32_t>(count), static_cast<std::uint32_t>(instances), static_cast<std::uint32_t>(first), 0, 0);
}

void Device::endPass() {
    if (pass != nullptr) {
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
        pass = nullptr;
        passGroups = 0;
    }
    skipping = false;
}

void Device::pushDebugGroup(std::string_view name) {
    if (pass != nullptr) {
        wgpuRenderPassEncoderPushDebugGroup(pass, Conversion::toText(name));
        ++passGroups;
        return;
    }
    openEncoder();
    wgpuCommandEncoderPushDebugGroup(encoder, Conversion::toText(name));
    ++encoderGroups;
}

void Device::popDebugGroup() {
    if (pass != nullptr && passGroups > 0) {
        wgpuRenderPassEncoderPopDebugGroup(pass);
        --passGroups;
        return;
    }
    if (encoderGroups > 0) {
        wgpuCommandEncoderPopDebugGroup(encoder);
        --encoderGroups;
    }
}

// The uniforms of the frame reach the GPU through the queue before the commands that read them, and what the frame destroyed goes to WebGPU once the commands that may record it are submitted.
void Device::commit() {
    ring.flush(queue);
    if (encoder != nullptr) {
        if (timer) {
            timer->resolve(encoder);
        }
        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
        wgpuQueueSubmit(queue, 1, &commands);
        wgpuCommandBufferRelease(commands);
        wgpuCommandEncoderRelease(encoder);
        encoder = nullptr;
        encoderGroups = 0;
        if (timer) {
            timer->submitted();
        }
    }
    if (swapchain) {
        swapchain->release();
    }

    releaseRetired();
    imageGroups.trim(frame);
    uniforms.fill({});
    ++frame;
}

void Device::resizeSurface(int width, int height) {
    if (swapchain) {
        swapchain->resize(width, height);
    }
}

void Device::detachSurface() {
    swapchain.reset();
}

void Device::attachSurface(const Surface& surface) {
    swapchain = std::make_unique<Swapchain>(connection->getInstance(), connection->getAdapter(), device, surface);
    if (swapchain->getFormat() == swapchainFormat && swapchain->getSampleCount() == swapchainSamples) {
        return;
    }
    swapchainFormat = swapchain->getFormat();
    swapchainSamples = swapchain->getSampleCount();
    rebuildSwapchainPipelines();
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    const ImageRecord& record = images.get(image.id);
    if (record.kind != ImageKind::RenderTarget || record.sampleCount != 1) {
        throw std::logic_error(std::format("The image {} is not a render target of one sample, whose pixels can be read back.", image.id));
    }
    return std::make_unique<ImageReadback>(device, queue, record.texture, record.format, width, height);
}

} // namespace haylen::gpu::webgpu
