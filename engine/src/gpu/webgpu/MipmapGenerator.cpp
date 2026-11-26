#include "gpu/webgpu/MipmapGenerator.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstring>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

MipmapGenerator::MipmapGenerator(WGPUDevice owner, std::uint32_t uniformAlignment) : device(owner), stride(std::max(uniformAlignment, kIndexBytes)) {
    WGPUSamplerDescriptor samplerDescriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    samplerDescriptor.label = Conversion::toText("haylen-mipmaps");
    samplerDescriptor.addressModeU = WGPUAddressMode_ClampToEdge;
    samplerDescriptor.addressModeV = WGPUAddressMode_ClampToEdge;
    samplerDescriptor.addressModeW = WGPUAddressMode_ClampToEdge;
    samplerDescriptor.magFilter = WGPUFilterMode_Linear;
    samplerDescriptor.minFilter = WGPUFilterMode_Linear;
    sampler = wgpuDeviceCreateSampler(device, &samplerDescriptor);

    WGPUBindGroupLayoutEntry index = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    index.binding = 0;
    index.visibility = WGPUShaderStage_Fragment;
    index.buffer.type = WGPUBufferBindingType_Uniform;
    index.buffer.hasDynamicOffset = 1U;
    index.buffer.minBindingSize = kIndexBytes;
    WGPUBindGroupLayoutDescriptor indexDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    indexDescriptor.label = Conversion::toText("haylen-mipmaps");
    indexDescriptor.entryCount = 1;
    indexDescriptor.entries = &index;
    indexLayout = wgpuDeviceCreateBindGroupLayout(device, &indexDescriptor);

    variants[0] = makeVariant(kLayersSource, WGPUTextureViewDimension_2DArray);
    variants[1] = makeVariant(kVolumeSource, WGPUTextureViewDimension_3D);
}

MipmapGenerator::~MipmapGenerator() {
    releaseOutgrown();

    if (indices != nullptr) {
        wgpuBindGroupRelease(indexGroup);
        wgpuBufferDestroy(indices);
        wgpuBufferRelease(indices);
    }

    for (const auto& [key, pipeline] : pipelines) {
        wgpuRenderPipelineRelease(pipeline);
    }

    for (const Variant& variant : variants) {
        wgpuPipelineLayoutRelease(variant.layout);
        wgpuBindGroupLayoutRelease(variant.sources);
        wgpuShaderModuleRelease(variant.module);
    }

    wgpuBindGroupLayoutRelease(indexLayout);
    wgpuSamplerRelease(sampler);
}

MipmapGenerator::Variant MipmapGenerator::makeVariant(std::string_view source, WGPUTextureViewDimension dimension) const {
    Variant variant;
    WGPUShaderSourceWGSL code = WGPU_SHADER_SOURCE_WGSL_INIT;
    code.code = Conversion::toText(source);
    WGPUShaderModuleDescriptor moduleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    moduleDescriptor.nextInChain = &code.chain;
    moduleDescriptor.label = Conversion::toText("haylen-mipmaps");
    variant.module = wgpuDeviceCreateShaderModule(device, &moduleDescriptor);

    std::array<WGPUBindGroupLayoutEntry, 2> entries{WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].visibility = WGPUShaderStage_Fragment;
    entries[0].texture.sampleType = WGPUTextureSampleType_Float;
    entries[0].texture.viewDimension = dimension;
    entries[1].binding = 1;
    entries[1].visibility = WGPUShaderStage_Fragment;
    entries[1].sampler.type = WGPUSamplerBindingType_Filtering;
    WGPUBindGroupLayoutDescriptor layoutDescriptor = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layoutDescriptor.label = Conversion::toText("haylen-mipmaps");
    layoutDescriptor.entryCount = entries.size();
    layoutDescriptor.entries = entries.data();
    variant.sources = wgpuDeviceCreateBindGroupLayout(device, &layoutDescriptor);

    const std::array<WGPUBindGroupLayout, 2> groups{variant.sources, indexLayout};
    WGPUPipelineLayoutDescriptor pipelineDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineDescriptor.label = Conversion::toText("haylen-mipmaps");
    pipelineDescriptor.bindGroupLayoutCount = groups.size();
    pipelineDescriptor.bindGroupLayouts = groups.data();
    variant.layout = wgpuDeviceCreatePipelineLayout(device, &pipelineDescriptor);
    return variant;
}

// The source of a level reads the one level above it, every layer of it, or the volume of it, which the passes of the level draw from while they draw into the level itself.
MipmapGenerator::Source MipmapGenerator::makeSource(WGPUTexture texture, bool volume, int level, int layers) const {
    WGPUTextureViewDescriptor viewDescriptor = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    viewDescriptor.label = Conversion::toText("haylen-mipmaps");
    viewDescriptor.dimension = volume ? WGPUTextureViewDimension_3D : WGPUTextureViewDimension_2DArray;
    viewDescriptor.baseMipLevel = static_cast<std::uint32_t>(level - 1);
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = volume ? 1U : static_cast<std::uint32_t>(layers);
    viewDescriptor.aspect = WGPUTextureAspect_All;
    WGPUTextureView view = wgpuTextureCreateView(texture, &viewDescriptor);

    std::array<WGPUBindGroupEntry, 2> entries{WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = view;
    entries[1].binding = 1;
    entries[1].sampler = sampler;
    WGPUBindGroupDescriptor groupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    groupDescriptor.label = Conversion::toText("haylen-mipmaps");
    groupDescriptor.layout = variants[volume ? 1 : 0].sources;
    groupDescriptor.entryCount = entries.size();
    groupDescriptor.entries = entries.data();
    return {.view = view, .group = wgpuDeviceCreateBindGroup(device, &groupDescriptor)};
}

WGPURenderPipeline MipmapGenerator::prepare(WGPUTextureFormat format, bool volume, int count) {
    reserveIndices(static_cast<std::uint32_t>(count));

    const auto found = pipelines.find({format, volume});
    if (found != pipelines.end()) {
        return found->second;
    }

    const Variant& variant = variants[volume ? 1 : 0];
    WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
    target.format = format;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = variant.module;
    fragment.entryPoint = Conversion::toText("fragmentMain");
    fragment.targetCount = 1;
    fragment.targets = &target;
    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-mipmaps");
    descriptor.layout = variant.layout;
    descriptor.vertex.module = variant.module;
    descriptor.vertex.entryPoint = Conversion::toText("vertexMain");
    descriptor.fragment = &fragment;
    WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &descriptor);
    pipelines.emplace(std::pair{format, volume}, pipeline);
    return pipeline;
}

// The passes of this frame that were recorded before may still read the buffer that a larger one replaces, so it stays until the frame is submitted, while its bind group goes at once, since the commands that recorded it keep it.
void MipmapGenerator::reserveIndices(std::uint32_t count) {
    if (count <= capacity) {
        return;
    }

    if (indices != nullptr) {
        outgrown.push_back(indices);
        wgpuBindGroupRelease(indexGroup);
    }

    capacity = std::bit_ceil(count);
    const std::size_t size = static_cast<std::size_t>(capacity) * stride;
    WGPUBufferDescriptor bufferDescriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    bufferDescriptor.label = Conversion::toText("haylen-mipmaps");
    bufferDescriptor.usage = WGPUBufferUsage_Uniform;
    bufferDescriptor.size = size;
    bufferDescriptor.mappedAtCreation = 1U;
    indices = wgpuDeviceCreateBuffer(device, &bufferDescriptor);

    auto* bytes = static_cast<std::byte*>(wgpuBufferGetMappedRange(indices, 0, size));
    for (std::uint32_t index = 0; index < capacity; ++index) {
        std::memcpy(bytes + (static_cast<std::size_t>(index) * stride), &index, sizeof(index));
    }

    wgpuBufferUnmap(indices);

    WGPUBindGroupEntry entry = WGPU_BIND_GROUP_ENTRY_INIT;
    entry.binding = 0;
    entry.buffer = indices;
    entry.size = kIndexBytes;
    WGPUBindGroupDescriptor groupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    groupDescriptor.label = Conversion::toText("haylen-mipmaps");
    groupDescriptor.layout = indexLayout;
    groupDescriptor.entryCount = 1;
    groupDescriptor.entries = &entry;
    indexGroup = wgpuDeviceCreateBindGroup(device, &groupDescriptor);
}

void MipmapGenerator::draw(WGPUCommandEncoder encoder, WGPURenderPipeline pipeline, const Source& source, WGPUTextureView target, std::uint32_t depthSlice, int index) const {
    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = target;
    color.depthSlice = depthSlice;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-mipmaps");
    descriptor.colorAttachmentCount = 1;
    descriptor.colorAttachments = &color;

    const std::uint32_t offset = static_cast<std::uint32_t>(index) * stride;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, source.group, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(pass, 1, indexGroup, 1, &offset);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
}

void MipmapGenerator::releaseOutgrown() noexcept {
    for (WGPUBuffer buffer : outgrown) {
        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
    }

    outgrown.clear();
}

} // namespace haylen::gpu::webgpu
