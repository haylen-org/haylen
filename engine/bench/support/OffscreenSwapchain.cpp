#include "support/OffscreenSwapchain.hpp"

#include <utility>

namespace haylen::bench {

OffscreenSwapchain::OffscreenSwapchain(std::unique_ptr<gpu::Device> wrapped, int width, int height) : device(std::move(wrapped)) {
    target = device->makeImage({.width = width, .height = height, .format = kFormat, .kind = ImageKind::RenderTarget, .label = "haylen-benchmark-swapchain"});
}

OffscreenSwapchain::~OffscreenSwapchain() {
    device->destroy(target);
}

gpu::Device::Language OffscreenSwapchain::getLanguage() const noexcept {
    return device->getLanguage();
}

gpu::Device::Api OffscreenSwapchain::getApi() const noexcept {
    return device->getApi();
}

const gpu::Device::Capabilities& OffscreenSwapchain::getCapabilities() const noexcept {
    return device->getCapabilities();
}

std::string OffscreenSwapchain::getAdapterName() const {
    return device->getAdapterName();
}

std::optional<double> OffscreenSwapchain::getGpuFrameSeconds() const noexcept {
    return device->getGpuFrameSeconds();
}

std::vector<gpu::Device::Pool> OffscreenSwapchain::getPools() const {
    return device->getPools();
}

bool OffscreenSwapchain::isLost() const noexcept {
    return device->isLost();
}

gpu::Device::Buffer OffscreenSwapchain::makeBuffer(const BufferDesc& desc) {
    return device->makeBuffer(desc);
}

gpu::Device::Image OffscreenSwapchain::makeImage(const ImageDesc& desc) {
    return device->makeImage(desc);
}

gpu::Device::Sampler OffscreenSwapchain::makeSampler(const SamplerDesc& desc) {
    return device->makeSampler(desc);
}

gpu::Device::Program OffscreenSwapchain::makeProgram(const ProgramDesc& desc) {
    return device->makeProgram(desc);
}

// A pipeline of the swapchain draws into the render target that stands for it, with its format, one sample and no depth, since no pipeline of the swapchain tests depth or stencil.
gpu::Device::Pipeline OffscreenSwapchain::makePipeline(const PipelineDesc& desc) {
    if (!desc.swapchain) {
        return device->makePipeline(desc);
    }
    ColorTarget color = desc.colors.empty() ? ColorTarget{} : desc.colors.front();
    color.format = kFormat;
    PipelineDesc offscreen = desc;
    offscreen.colors = std::span(&color, 1);
    offscreen.depthStencil = {};
    offscreen.sampleCount = 1;
    offscreen.swapchain = false;
    return device->makePipeline(offscreen);
}

void OffscreenSwapchain::prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) {
    device->prepareProgram(desc, jobs);
}

void OffscreenSwapchain::destroy(Buffer buffer) noexcept {
    device->destroy(buffer);
}

void OffscreenSwapchain::destroy(Image image) noexcept {
    device->destroy(image);
}

void OffscreenSwapchain::destroy(Sampler sampler) noexcept {
    device->destroy(sampler);
}

void OffscreenSwapchain::destroy(Program program) noexcept {
    device->destroy(program);
}

void OffscreenSwapchain::destroy(Pipeline pipeline) noexcept {
    device->destroy(pipeline);
}

void OffscreenSwapchain::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    device->writeBuffer(buffer, offset, data);
}

void OffscreenSwapchain::writeImages(std::span<const ImageWrite> writes) {
    device->writeImages(writes);
}

void OffscreenSwapchain::beginPass(const PassDesc& desc) {
    if (!desc.colors.empty() || desc.depthStencil) {
        device->beginPass(desc);
        return;
    }
    const ColorAttachment color{.image = target, .load = desc.load, .clear = desc.clear};
    PassDesc offscreen = desc;
    offscreen.colors = std::span(&color, 1);
    device->beginPass(offscreen);
}

void OffscreenSwapchain::applyViewport(const math::Rect& pixels) {
    device->applyViewport(pixels);
}

void OffscreenSwapchain::applyScissor(const math::Rect& pixels) {
    device->applyScissor(pixels);
}

void OffscreenSwapchain::applyPipeline(Pipeline pipeline) {
    device->applyPipeline(pipeline);
}

void OffscreenSwapchain::applyBindings(const Bindings& bindings) {
    device->applyBindings(bindings);
}

void OffscreenSwapchain::applyUniforms(int slot, std::span<const std::byte> data) {
    device->applyUniforms(slot, data);
}

void OffscreenSwapchain::applyStencilReference(std::uint8_t value) {
    device->applyStencilReference(value);
}

void OffscreenSwapchain::applyBlendColor(math::Color value) {
    device->applyBlendColor(value);
}

void OffscreenSwapchain::draw(int first, int count, int instances) {
    device->draw(first, count, instances);
}

void OffscreenSwapchain::endPass() {
    device->endPass();
}

void OffscreenSwapchain::pushDebugGroup(std::string_view name) {
    device->pushDebugGroup(name);
}

void OffscreenSwapchain::popDebugGroup() {
    device->popDebugGroup();
}

void OffscreenSwapchain::commit() {
    device->commit();
}

void OffscreenSwapchain::resizeSurface(int width, int height) {
    device->resizeSurface(width, height);
}

void OffscreenSwapchain::detachSurface() {
    device->detachSurface();
}

void OffscreenSwapchain::attachSurface(const gpu::Surface& surface) {
    device->attachSurface(surface);
}

std::unique_ptr<gpu::Device::Readback> OffscreenSwapchain::readImage(Image image, int width, int height) {
    return device->readImage(image, width, height);
}

} // namespace haylen::bench
