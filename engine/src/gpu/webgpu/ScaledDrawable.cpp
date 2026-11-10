#include "gpu/webgpu/ScaledDrawable.hpp"

#include <array>
#include <cstdint>

#include "gpu/webgpu/Conversion.hpp"

namespace haylen::gpu::webgpu {

ScaledDrawable::ScaledDrawable(WGPUDevice owner, WGPUTextureFormat canvasFormat, const Surface::Size& value) : device(owner), format(canvasFormat), size(value) {
    WGPUShaderSourceWGSL code = WGPU_SHADER_SOURCE_WGSL_INIT;
    code.code = Conversion::toText(kSource);
    WGPUShaderModuleDescriptor moduleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    moduleDescriptor.nextInChain = &code.chain;
    moduleDescriptor.label = Conversion::toText("haylen-scaled-drawable");
    module = wgpuDeviceCreateShaderModule(device, &moduleDescriptor);

    WGPUColorTargetState target = WGPU_COLOR_TARGET_STATE_INIT;
    target.format = format;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
    fragment.module = module;
    fragment.entryPoint = Conversion::toText("fragmentMain");
    fragment.targetCount = 1;
    fragment.targets = &target;
    WGPURenderPipelineDescriptor pipelineDescriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipelineDescriptor.label = Conversion::toText("haylen-scaled-drawable");
    pipelineDescriptor.vertex.module = module;
    pipelineDescriptor.vertex.entryPoint = Conversion::toText("vertexMain");
    pipelineDescriptor.fragment = &fragment;
    pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDescriptor);

    WGPUSamplerDescriptor samplerDescriptor = WGPU_SAMPLER_DESCRIPTOR_INIT;
    samplerDescriptor.label = Conversion::toText("haylen-scaled-drawable");
    samplerDescriptor.magFilter = WGPUFilterMode_Linear;
    samplerDescriptor.minFilter = WGPUFilterMode_Linear;
    sampler = wgpuDeviceCreateSampler(device, &samplerDescriptor);
    makeTexture();
}

ScaledDrawable::~ScaledDrawable() {
    releaseTexture();
    wgpuSamplerRelease(sampler);
    wgpuRenderPipelineRelease(pipeline);
    wgpuShaderModuleRelease(module);
}

void ScaledDrawable::resize(const Surface::Size& value) {
    if (value == size) {
        return;
    }
    releaseTexture();
    size = value;
    makeTexture();
}

void ScaledDrawable::makeTexture() {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-swapchain-drawable");
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    descriptor.size = {.width = static_cast<std::uint32_t>(size.width), .height = static_cast<std::uint32_t>(size.height), .depthOrArrayLayers = 1};
    descriptor.format = format;
    texture = wgpuDeviceCreateTexture(device, &descriptor);
    view = wgpuTextureCreateView(texture, nullptr);

    std::array<WGPUBindGroupEntry, 2> entries{WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0;
    entries[0].textureView = view;
    entries[1].binding = 1;
    entries[1].sampler = sampler;
    WGPUBindGroupDescriptor groupDescriptor = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    groupDescriptor.label = Conversion::toText("haylen-scaled-drawable");
    groupDescriptor.layout = wgpuRenderPipelineGetBindGroupLayout(pipeline, 0);
    groupDescriptor.entryCount = entries.size();
    groupDescriptor.entries = entries.data();
    group = wgpuDeviceCreateBindGroup(device, &groupDescriptor);
    wgpuBindGroupLayoutRelease(groupDescriptor.layout);
}

// The frames in flight may still read the texture, which WebGPU keeps until they finish.
void ScaledDrawable::releaseTexture() noexcept {
    wgpuBindGroupRelease(group);
    wgpuTextureViewRelease(view);
    wgpuTextureDestroy(texture);
    wgpuTextureRelease(texture);
    group = nullptr;
    view = nullptr;
    texture = nullptr;
}

void ScaledDrawable::record(WGPUCommandEncoder encoder, WGPUTextureView canvas) const {
    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = canvas;
    color.loadOp = WGPULoadOp_Clear;
    color.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.label = Conversion::toText("haylen-scaled-drawable");
    descriptor.colorAttachmentCount = 1;
    descriptor.colorAttachments = &color;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);
    wgpuRenderPassEncoderSetPipeline(pass, pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, group, 0, nullptr);
    wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
}

} // namespace haylen::gpu::webgpu
