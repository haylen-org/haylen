#include "gpu/vulkan/Device.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>

#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "gpu/vulkan/ImageLayouts.hpp"
#include "gpu/vulkan/ImageReadback.hpp"
#include "gpu/vulkan/Instance.hpp"
#include "gpu/vulkan/PreparedModules.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::vulkan {

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& surface, const DeviceFactory::Options& options) {
    return std::make_unique<Device>(surface, Settings{.validation = options.validation});
}

Device::Device(const std::optional<Surface>& surface, const Settings& settings) : instance(std::make_unique<Instance>(settings.validation)), swapchain(surface ? std::make_unique<Swapchain>(instance->getHandle(), instance->createSurface(*surface)) : nullptr), context(std::make_unique<Context>(*instance, swapchain ? swapchain->getSurface() : VK_NULL_HANDLE)), releases(*context), layouts(*context), descriptors(*context, releases), renderPasses(*context, releases), staging(*context, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, kStagingChunkSize, kFramesInFlight, "haylen-staging"), uniforms(*context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, kUniformChunkSize, kFramesInFlight, "haylen-uniforms"), readbacks(*context), prepared(std::make_shared<PreparedModules>(*context)), vk(context->getFunctions()), dynamicRendering(settings.dynamicRendering && context->hasDynamicRendering()) {
    try {
        const VkPipelineCacheCreateInfo cacheInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        Context::check(vk.vkCreatePipelineCache(context->getDevice(), &cacheInfo, nullptr, &pipelineCache), "make its pipeline cache");
        if (dynamicRendering) {
            const bool core = context->getApiVersion() >= VK_API_VERSION_1_3;
            beginRenderingCommand = core ? vk.vkCmdBeginRendering : vk.vkCmdBeginRenderingKHR;
            endRenderingCommand = core ? vk.vkCmdEndRendering : vk.vkCmdEndRenderingKHR;
        }
        labels = instance->hasDebugUtils() && vkCmdBeginDebugUtilsLabelEXT != nullptr;
        createFrames();
        fillCapabilities();
        swapchainTarget.depth = context->getDepthStencilFormat();

        if (surface) {
            swapchain->attach(*context);
            surfaceSettings = {.width = surface->width, .height = surface->height, .sampleCount = surface->sampleCount, .vsync = surface->vsync, .transparent = surface->transparent};
            swapchain->configure(surfaceSettings);
            updateSwapchainTarget();
        }
    } catch (...) {
        shutdown();
        throw;
    }
    core::Log::info("The Vulkan device draws with \"{}\" through Vulkan {}.{} with {}.", context->getProperties().deviceName, VK_API_VERSION_MAJOR(context->getApiVersion()), VK_API_VERSION_MINOR(context->getApiVersion()), dynamicRendering ? "dynamic rendering" : "render passes");
}

Device::~Device() {
    shutdown();
}

void Device::createFrames() {
    const VkDevice device = context->getDevice();
    const VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = context->getQueueFamily()};
    const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    const VkSemaphoreCreateInfo semaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    const VkQueryPoolCreateInfo queryInfo{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2};
    for (Frame& frame : frames) {
        Context::check(vk.vkCreateCommandPool(device, &poolInfo, nullptr, &frame.pool), "make the command pool of a frame");
        const VkCommandBufferAllocateInfo commandsInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool = frame.pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 2};
        std::array<VkCommandBuffer, 2> commands{};
        Context::check(vk.vkAllocateCommandBuffers(device, &commandsInfo, commands.data()), "allocate the command buffers of a frame");
        frame.uploads = commands[0];
        frame.graphics = commands[1];
        Context::check(vk.vkCreateFence(device, &fenceInfo, nullptr, &frame.fence), "make the fence of a frame");
        Context::check(vk.vkCreateSemaphore(device, &semaphoreInfo, nullptr, &frame.acquired), "make a semaphore of a frame");
        if (context->getTimestampBits() > 0) {
            Context::check(vk.vkCreateQueryPool(device, &queryInfo, nullptr, &frame.timestamps), "make the timestamp queries of a frame");
        }
    }
}

void Device::fillCapabilities() {
    const VkPhysicalDeviceLimits& limits = context->getProperties().limits;
    capabilities.maxImageSize = static_cast<int>(limits.maxImageDimension2D);
    capabilities.maxArrayLayers = static_cast<int>(limits.maxImageArrayLayers);
    const VkSampleCountFlags counts = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;
    capabilities.maxSampleCount = static_cast<int>(std::bit_floor(counts & VK_SAMPLE_COUNT_64_BIT ? 64U : counts & 0x3FU));
    capabilities.originTopLeft = true;
    capabilities.independentBlend = context->getEnabledFeatures().independentBlend == VK_TRUE;
    capabilities.multisampledRenderTargets = true;
    capabilities.gpuTiming = context->getTimestampBits() > 0 && limits.timestampPeriod > 0.0F;
    for (std::size_t index = 0; index < capabilities.blendableFormats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        capabilities.blendableFormats[index] = !Conversions::isDepth(format) && context->hasFormatFeatures(Conversions::toFormat(format), VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
    }
}

// The device waits for the GPU and destroys what it made in the order its objects need: the ones that frames used, the frames, and the swapchain before the logical device.
void Device::shutdown() noexcept {
    if (!context) {
        return;
    }
    vk.vkDeviceWaitIdle(context->getDevice());
    prepared->close();

    const VkDevice device = context->getDevice();
    const VmaAllocator allocator = context->getAllocator();
    buffers.forEach([&](const BufferRecord& record) { vmaDestroyBuffer(allocator, record.buffer, record.allocation); });
    // clang-format off
    images.forEach([&](const ImageRecord& record) {
        for (const AttachmentView& view : record.attachmentViews) {
            vk.vkDestroyImageView(device, view.view, nullptr);
        }
        if (record.sampleView != VK_NULL_HANDLE) {
            vk.vkDestroyImageView(device, record.sampleView, nullptr);
        }
        vmaDestroyImage(allocator, record.image, record.allocation);
    });
    // clang-format on
    samplers.forEach([&](const SamplerRecord& record) { vk.vkDestroySampler(device, record.sampler, nullptr); });
    pipelines.forEach([&](const PipelineRecord& record) { vk.vkDestroyPipeline(device, record.pipeline, nullptr); });
    // clang-format off
    programs.forEach([&](const ProgramRecord& record) {
        vk.vkDestroyShaderModule(device, record.vertex, nullptr);
        vk.vkDestroyShaderModule(device, record.fragment, nullptr);
    });
    // clang-format on
    buffers = {};
    images = {};
    samplers = {};
    pipelines = {};
    programs = {};

    for (const Frame& frame : frames) {
        vk.vkDestroyQueryPool(device, frame.timestamps, nullptr);
        vk.vkDestroySemaphore(device, frame.acquired, nullptr);
        vk.vkDestroyFence(device, frame.fence, nullptr);
        vk.vkDestroyCommandPool(device, frame.pool, nullptr);
    }
    frames = {};
    vk.vkDestroyPipelineCache(device, pipelineCache, nullptr);
    pipelineCache = VK_NULL_HANDLE;
    releases.clear();
    swapchain.reset();
}

gpu::Device::Language Device::getLanguage() const noexcept {
    return Language::Spirv;
}

gpu::Device::Api Device::getApi() const noexcept {
    return Api::Vulkan;
}

const gpu::Device::Capabilities& Device::getCapabilities() const noexcept {
    return capabilities;
}

std::string Device::getAdapterName() const {
    return context->getProperties().deviceName;
}

std::optional<double> Device::getGpuFrameSeconds() const noexcept {
    return gpuFrameSeconds;
}

std::vector<gpu::Device::Pool> Device::getPools() const {
    return {
        {.name = "images", .used = images.size(), .size = images.capacity()}, {.name = "buffers", .used = buffers.size(), .size = buffers.capacity()}, {.name = "samplers", .used = samplers.size(), .size = samplers.capacity()}, {.name = "programs", .used = programs.size(), .size = programs.capacity()}, {.name = "pipelines", .used = pipelines.size(), .size = pipelines.capacity()},
    };
}

bool Device::isLost() const noexcept {
    return lost;
}

bool Device::ensureFrame() {
    if (frameOpen) {
        return true;
    }
    if (lost) {
        return false;
    }

    Frame& frame = frames[frameIndex];
    if (frame.submitted) {
        const VkResult waited = vk.vkWaitForFences(context->getDevice(), 1, &frame.fence, VK_TRUE, UINT64_MAX);
        if (waited != VK_SUCCESS) {
            markLost(waited);
            return false;
        }
        completeFrame(frame);
    }
    Context::check(vk.vkResetFences(context->getDevice(), 1, &frame.fence), "reset the fence of a frame");
    releases.collect(completedSerial);
    staging.begin(static_cast<int>(frameIndex));
    uniforms.begin(static_cast<int>(frameIndex));

    Context::check(vk.vkResetCommandPool(context->getDevice(), frame.pool, 0), "reset the command pool of a frame");
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    Context::check(vk.vkBeginCommandBuffer(frame.uploads, &begin), "record the uploads of a frame");
    Context::check(vk.vkBeginCommandBuffer(frame.graphics, &begin), "record the passes of a frame");
    if (frame.timestamps != VK_NULL_HANDLE) {
        vk.vkCmdResetQueryPool(frame.uploads, frame.timestamps, 0, 2);
        vk.vkCmdWriteTimestamp(frame.uploads, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.timestamps, 0);
    }

    frame.serial = nextSerial;
    frameOpen = true;
    bufferCopies = false;
    swapchainDrawn = false;
    bound.uniformBuffer = VK_NULL_HANDLE;
    bound.uniformMask = 0;
    return true;
}

// The GPU time of a frame runs from the start of its uploads to the end of its passes.
void Device::completeFrame(Frame& frame) {
    frame.submitted = false;
    completedSerial = std::max(completedSerial, frame.serial);
    if (frame.timestamps == VK_NULL_HANDLE) {
        return;
    }
    std::array<std::uint64_t, 2> ticks{};
    if (vk.vkGetQueryPoolResults(context->getDevice(), frame.timestamps, 0, 2, sizeof(ticks), ticks.data(), sizeof(std::uint64_t), VK_QUERY_RESULT_64_BIT) != VK_SUCCESS) {
        return;
    }
    const std::uint32_t bits = context->getTimestampBits();
    const std::uint64_t mask = bits >= 64 ? ~std::uint64_t{0} : (std::uint64_t{1} << bits) - 1;
    const std::uint64_t elapsed = ((ticks[1] & mask) - (ticks[0] & mask)) & mask;
    gpuFrameSeconds = static_cast<double>(elapsed) * static_cast<double>(context->getProperties().limits.timestampPeriod) * 1e-9;
}

void Device::pollFrames() {
    for (Frame& frame : frames) {
        if (!frame.submitted) {
            continue;
        }
        const VkResult status = vk.vkGetFenceStatus(context->getDevice(), frame.fence);
        if (status == VK_SUCCESS) {
            completeFrame(frame);
        } else if (status != VK_NOT_READY) {
            markLost(status);
            return;
        }
    }
}

void Device::markLost(VkResult result) {
    if (!lost) {
        core::Log::error("The Vulkan device stopped working ({}), so the app starts again on a new device.", Conversions::describe(result));
    }
    lost = true;
}

void Device::waitIdle() {
    const VkResult result = vk.vkDeviceWaitIdle(context->getDevice());
    if (result != VK_SUCCESS) {
        markLost(result);
        return;
    }
    for (Frame& frame : frames) {
        if (frame.submitted) {
            completeFrame(frame);
        }
    }
}

void Device::commit() {
    if (pass.open) {
        throw std::logic_error("A pass is still open. Call \"endPass\" before \"commit\".");
    }
    if (lost) {
        return;
    }
    if (!frameOpen) {
        pollFrames();
        releases.collect(completedSerial);
        readbacks.sweep();
        return;
    }

    Frame& frame = frames[frameIndex];
    if (bufferCopies) {
        const VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT};
        vk.vkCmdPipelineBarrier(frame.uploads, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
    if (swapchainDrawn) {
        const VkImageMemoryBarrier toPresent{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = 0, .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = swapchain->getImage(imageIndex), .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1}};
        vk.vkCmdPipelineBarrier(frame.graphics, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);
    }
    if (frame.timestamps != VK_NULL_HANDLE) {
        vk.vkCmdWriteTimestamp(frame.graphics, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timestamps, 1);
    }
    Context::check(vk.vkEndCommandBuffer(frame.uploads), "record the uploads of a frame");
    Context::check(vk.vkEndCommandBuffer(frame.graphics), "record the passes of a frame");
    staging.flush();
    uniforms.flush();

    const std::array<VkCommandBuffer, 2> commands{frame.uploads, frame.graphics};
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSemaphore finished = acquired ? swapchain->getFinished(imageIndex) : VK_NULL_HANDLE;
    const VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = acquired ? 1U : 0U,
        .pWaitSemaphores = &frame.acquired,
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = static_cast<std::uint32_t>(commands.size()),
        .pCommandBuffers = commands.data(),
        .signalSemaphoreCount = acquired ? 1U : 0U,
        .pSignalSemaphores = &finished,
    };
    frameOpen = false;
    const VkResult submitted = vk.vkQueueSubmit(context->getQueue(), 1, &submit, frame.fence);
    if (submitted == VK_ERROR_DEVICE_LOST) {
        markLost(submitted);
        return;
    }
    Context::check(submitted, "submit a frame");
    frame.submitted = true;

    if (acquired) {
        const VkResult presented = swapchain->present(imageIndex);
        if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR || presented == VK_ERROR_SURFACE_LOST_KHR) {
            swapchainDirty = true;
        } else if (presented == VK_ERROR_DEVICE_LOST) {
            markLost(presented);
        }
        acquired = false;
    }

    ++nextSerial;
    frameIndex = (frameIndex + 1) % frames.size();
    pollFrames();
    releases.collect(completedSerial);
    descriptors.trim(completedSerial);
    readbacks.sweep();
}

void Device::resizeSurface(int width, int height) {
    if (surfaceSettings.width == width && surfaceSettings.height == height) {
        return;
    }
    surfaceSettings.width = width;
    surfaceSettings.height = height;
    swapchainDirty = true;
}

void Device::detachSurface() {
    if (!swapchain) {
        return;
    }
    if (acquired) {
        throw std::logic_error("The surface goes between frames, after \"commit\".");
    }
    waitIdle();
    for (VkImageView view : swapchain->getViews()) {
        renderPasses.forgetView(view, completedSerial);
    }
    releases.collect(completedSerial);
    swapchain.reset();
}

void Device::attachSurface(const Surface& surface) {
    detachSurface();
    auto made = std::make_unique<Swapchain>(instance->getHandle(), instance->createSurface(surface));
    made->attach(*context);
    swapchain = std::move(made);
    surfaceSettings = {.width = surface.width, .height = surface.height, .sampleCount = surface.sampleCount, .vsync = surface.vsync, .transparent = surface.transparent};
    swapchainDirty = true;
}

// The frames in flight may still draw into the old swapchain, so the device waits for them before it makes the new one.
void Device::reconfigureSwapchain() {
    waitIdle();
    if (lost) {
        return;
    }
    for (VkImageView view : swapchain->getViews()) {
        renderPasses.forgetView(view, completedSerial);
    }
    releases.collect(completedSerial);
    swapchain->configure(surfaceSettings);
    swapchainDirty = false;
    updateSwapchainTarget();
}

void Device::updateSwapchainTarget() {
    if (!swapchain->hasImages()) {
        return;
    }
    const SwapchainTarget next{.color = swapchain->getFormat(), .depth = swapchain->getDepthFormat(), .samples = swapchain->getSamples(), .generation = swapchainTarget.generation};
    if (next.color != swapchainTarget.color || next.depth != swapchainTarget.depth || next.samples != swapchainTarget.samples) {
        swapchainTarget = next;
        ++swapchainTarget.generation;
    }
}

// A swapchain that went out of date is made again once, and a window without pixels or a lost surface draws no frame.
bool Device::acquireSwapchainImage() {
    if (acquired) {
        return true;
    }
    if (!swapchain) {
        return false;
    }
    if (swapchainDirty) {
        reconfigureSwapchain();
    }
    if (lost || !swapchain->hasImages()) {
        return false;
    }

    const VkSemaphore signaled = frames[frameIndex].acquired;
    VkResult result = swapchain->acquire(signaled, imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        reconfigureSwapchain();
        if (lost || !swapchain->hasImages()) {
            return false;
        }
        result = swapchain->acquire(signaled, imageIndex);
    }
    if (result == VK_SUBOPTIMAL_KHR) {
        swapchainDirty = true;
    } else if (result == VK_ERROR_DEVICE_LOST) {
        markLost(result);
        return false;
    } else if (result != VK_SUCCESS) {
        swapchainDirty = true;
        return false;
    }
    acquired = true;
    return true;
}

Device::BufferRecord& Device::requireBuffer(Buffer buffer) const {
    BufferRecord* record = buffers.find(buffer.id);
    if (record == nullptr) {
        throw std::logic_error(std::format("The buffer {} does not exist.", buffer.id));
    }
    return *record;
}

Device::ImageRecord& Device::requireImage(Image image) const {
    ImageRecord* record = images.find(image.id);
    if (record == nullptr) {
        throw std::logic_error(std::format("The image {} does not exist.", image.id));
    }
    return *record;
}

VkFormat Device::resolveFormat(PixelFormat format) const noexcept {
    return format == PixelFormat::Depth24Stencil8 ? context->getDepthStencilFormat() : Conversions::toFormat(format);
}

// A dynamic buffer stays mapped with one copy for each frame in flight. An immutable one lives where the GPU reads fastest, and takes its data directly when the CPU sees that memory and through the staging ring otherwise.
gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no buffer.");
    }
    const VkDeviceSize size = desc.size > 0 ? desc.size : desc.data.size();
    if (size == 0 || desc.data.size() > size) {
        throw std::invalid_argument(std::format("A buffer of {} bytes cannot hold {} bytes of data.", size, desc.data.size()));
    }

    auto record = std::make_unique<BufferRecord>(BufferRecord{.size = size, .usage = desc.usage});
    const bool dynamic = desc.usage == Usage::Dynamic;
    record->copies = dynamic ? static_cast<std::uint32_t>(kFramesInFlight) : 1U;
    record->stride = dynamic ? (size + kDynamicBufferAlignment - 1) / kDynamicBufferAlignment * kDynamicBufferAlignment : size;
    VkBufferUsageFlags usage = desc.index ? VK_BUFFER_USAGE_INDEX_BUFFER_BIT : VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    if (!dynamic) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = record->stride * record->copies, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VmaAllocationCreateInfo memory{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO};
    if (!dynamic) {
        memory.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
    }
    VmaAllocationInfo allocated{};
    Context::check(vmaCreateBuffer(context->getAllocator(), &info, &memory, &record->buffer, &record->allocation, &allocated), std::format("make a buffer of {} bytes", size));
    record->mapped = static_cast<std::byte*>(allocated.pMappedData);
    context->setName(VK_OBJECT_TYPE_BUFFER, record->buffer, desc.label);

    try {
        if (!desc.data.empty()) {
            uploadBuffer(*record, desc.data);
        }
        return {buffers.add(std::move(record))};
    } catch (...) {
        releases.push(ReleaseQueue::Buffer{.buffer = record->buffer, .allocation = record->allocation}, nextSerial);
        throw;
    }
}

void Device::uploadBuffer(const BufferRecord& record, std::span<const std::byte> data) {
    if (record.mapped != nullptr) {
        for (std::uint32_t copy = 0; copy < record.copies; ++copy) {
            std::memcpy(record.mapped + record.stride * copy, data.data(), data.size());
        }
        Context::check(vmaFlushAllocation(context->getAllocator(), record.allocation, 0, VK_WHOLE_SIZE), "write a buffer");
        return;
    }
    if (!ensureFrame()) {
        throw std::runtime_error("The Vulkan device stopped working, so it uploads no data.");
    }
    const UploadRing::Allocation source = staging.allocate(data.size(), 16);
    std::memcpy(source.data, data.data(), data.size());
    const VkBufferCopy region{.srcOffset = source.offset, .dstOffset = 0, .size = data.size()};
    vk.vkCmdCopyBuffer(frames[frameIndex].uploads, source.buffer, record.buffer, 1, &region);
    bufferCopies = true;
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no image.");
    }
    if (desc.width < 1 || desc.height < 1 || desc.width > capabilities.maxImageSize || desc.height > capabilities.maxImageSize) {
        throw std::invalid_argument(std::format("An image of {}x{} pixels is outside the sizes of the Vulkan device, from 1 to {}.", desc.width, desc.height, capabilities.maxImageSize));
    }
    if (desc.layers < 1 || desc.mipLevels < 1 || (desc.type == TextureType::Cube && desc.layers != 6)) {
        throw std::invalid_argument("An image needs one layer or more, one mip level or more, and a cube six layers.");
    }

    const VkFormat format = resolveFormat(desc.format);
    const bool depth = Conversions::isDepth(desc.format);
    const VkSampleCountFlagBits samples = Conversions::toSampleCount(desc.sampleCount);
    const VkPhysicalDeviceLimits& limits = context->getProperties().limits;
    const bool attachment = desc.kind == ImageKind::RenderTarget || desc.kind == ImageKind::DepthStencil;
    const bool sampled = samples == VK_SAMPLE_COUNT_1_BIT || (((depth ? limits.sampledImageDepthSampleCounts : limits.sampledImageColorSampleCounts) & static_cast<VkSampleCountFlags>(samples)) != 0);

    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (sampled) {
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (desc.kind == ImageKind::RenderTarget) {
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    } else if (desc.kind == ImageKind::DepthStencil) {
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    VkImageCreateFlags flags = 0;
    if (desc.type == TextureType::Cube) {
        flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    } else if (desc.type == TextureType::Volume && attachment) {
        flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    }
    const bool volume = desc.type == TextureType::Volume;
    const VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = flags,
        .imageType = volume ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = {static_cast<std::uint32_t>(desc.width), static_cast<std::uint32_t>(desc.height), volume ? static_cast<std::uint32_t>(desc.layers) : 1U},
        .mipLevels = static_cast<std::uint32_t>(desc.mipLevels),
        .arrayLayers = volume ? 1U : static_cast<std::uint32_t>(desc.layers),
        .samples = samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImageFormatProperties supported{};
    if (vkGetPhysicalDeviceImageFormatProperties(context->getPhysicalDevice(), format, info.imageType, info.tiling, usage, flags, &supported) != VK_SUCCESS || (supported.sampleCounts & static_cast<VkSampleCountFlags>(samples)) == 0) {
        throw std::runtime_error(std::format("The GPU of the Vulkan device cannot make the image \"{}\" in its format with {} samples.", desc.label, desc.sampleCount));
    }

    auto record = std::make_unique<ImageRecord>(ImageRecord{.format = format, .pixelFormat = desc.format, .kind = desc.kind, .type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .samples = samples, .aspects = Conversions::getAspects(format)});
    if (!sampled) {
        record->restLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    const VmaAllocationCreateInfo memory{.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE};
    Context::check(vmaCreateImage(context->getAllocator(), &info, &memory, &record->image, &record->allocation, nullptr), std::format("make the image \"{}\"", desc.label));
    context->setName(VK_OBJECT_TYPE_IMAGE, record->image, desc.label);

    try {
        if (sampled) {
            // A view that shaders sample reads the depth of a depth and stencil image.
            const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            const VkImageViewCreateInfo viewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = record->image, .viewType = Conversions::toViewType(desc.type), .format = format, .subresourceRange = {.aspectMask = aspect, .levelCount = VK_REMAINING_MIP_LEVELS, .layerCount = VK_REMAINING_ARRAY_LAYERS}};
            Context::check(vk.vkCreateImageView(context->getDevice(), &viewInfo, nullptr, &record->sampleView), "make the view of an image");
            context->setName(VK_OBJECT_TYPE_IMAGE_VIEW, record->sampleView, desc.label);
        }
        if (!ensureFrame()) {
            throw std::runtime_error("The Vulkan device stopped working, so it makes no image.");
        }
        if (desc.kind == ImageKind::Texture && !desc.levels.empty()) {
            uploadLevels(*record, desc.levels);
        } else {
            clearImage(*record);
        }
        return {images.add(std::move(record))};
    } catch (...) {
        releaseImage(*record, nextSerial);
        throw;
    }
}

// Each mip level holds every layer of its level, and a volume every slice, packed row after row.
void Device::uploadLevels(const ImageRecord& record, std::span<const std::span<const std::byte>> levels) {
    if (levels.size() > static_cast<std::size_t>(record.mipLevels)) {
        throw std::invalid_argument(std::format("The image has {} mip levels, not {}.", record.mipLevels, levels.size()));
    }
    const bool volume = record.type == TextureType::Volume;
    const std::size_t pixelBytes = Conversions::getBytesPerPixel(record.pixelFormat);
    copyRegions.clear();
    for (std::size_t level = 0; level < levels.size(); ++level) {
        const auto width = static_cast<std::uint32_t>(std::max(record.width >> level, 1));
        const auto height = static_cast<std::uint32_t>(std::max(record.height >> level, 1));
        const auto depth = volume ? static_cast<std::uint32_t>(std::max(record.layers >> level, 1)) : 1U;
        const auto layers = volume ? 1U : static_cast<std::uint32_t>(record.layers);
        const std::size_t bytes = std::size_t{width} * height * depth * layers * pixelBytes;
        if (levels[level].size() < bytes) {
            throw std::invalid_argument(std::format("The mip level {} of the image holds {} bytes, not the {} it needs.", level, levels[level].size(), bytes));
        }
        const UploadRing::Allocation source = staging.allocate(bytes, 16);
        std::memcpy(source.data, levels[level].data(), bytes);
        copyRegions.push_back({.bufferOffset = source.offset, .imageSubresource = {.aspectMask = record.aspects, .mipLevel = static_cast<std::uint32_t>(level), .layerCount = layers}, .imageExtent = {width, height, depth}});

        // The ring may move to another chunk between levels, so each level copies from its own buffer.
        const VkCommandBuffer commands = frames[frameIndex].uploads;
        if (level == 0) {
            const VkImageMemoryBarrier toDestination{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = record.image, .subresourceRange = {.aspectMask = record.aspects, .levelCount = VK_REMAINING_MIP_LEVELS, .layerCount = VK_REMAINING_ARRAY_LAYERS}};
            vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDestination);
        }
        vk.vkCmdCopyBufferToImage(commands, source.buffer, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegions.back());
    }

    // Mip levels without pixels start cleared.
    const VkCommandBuffer commands = frames[frameIndex].uploads;
    if (levels.size() < static_cast<std::size_t>(record.mipLevels)) {
        const VkClearColorValue clear{};
        const VkImageSubresourceRange rest{.aspectMask = record.aspects, .baseMipLevel = static_cast<std::uint32_t>(levels.size()), .levelCount = VK_REMAINING_MIP_LEVELS, .layerCount = VK_REMAINING_ARRAY_LAYERS};
        vk.vkCmdClearColorImage(commands, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &rest);
    }
    const VkImageSubresourceRange range{.aspectMask = record.aspects, .levelCount = VK_REMAINING_MIP_LEVELS, .layerCount = VK_REMAINING_ARRAY_LAYERS};
    const VkImageMemoryBarrier toRest = ImageLayouts::makeBarrier(record.image, range, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, record.restLayout);
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, ImageLayouts::getStages(record.restLayout), 0, 0, nullptr, 0, nullptr, 1, &toRest);
}

// Images without pixels start transparent, and depth and stencil images at the far depth and a stencil of 0, so every device starts them alike.
void Device::clearImage(const ImageRecord& record) {
    const VkCommandBuffer commands = frames[frameIndex].uploads;
    const VkImageSubresourceRange range{.aspectMask = record.aspects, .levelCount = VK_REMAINING_MIP_LEVELS, .layerCount = VK_REMAINING_ARRAY_LAYERS};
    const VkImageMemoryBarrier toDestination{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = record.image, .subresourceRange = range};
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDestination);

    if ((record.aspects & VK_IMAGE_ASPECT_COLOR_BIT) != 0) {
        const VkClearColorValue clear{};
        vk.vkCmdClearColorImage(commands, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    } else {
        const VkClearDepthStencilValue clear{.depth = 1.0F, .stencil = 0};
        vk.vkCmdClearDepthStencilImage(commands, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    }

    const VkImageMemoryBarrier toRest = ImageLayouts::makeBarrier(record.image, range, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, record.restLayout);
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, ImageLayouts::getStages(record.restLayout), 0, 0, nullptr, 0, nullptr, 1, &toRest);
}

gpu::Device::Sampler Device::makeSampler(const SamplerDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no sampler.");
    }
    const bool anisotropic = desc.maxAnisotropy > 1 && context->getEnabledFeatures().samplerAnisotropy == VK_TRUE;
    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = Conversions::toFilter(desc.magnification),
        .minFilter = Conversions::toFilter(desc.minification),
        .mipmapMode = Conversions::toMipmapMode(desc.mipmap),
        .addressModeU = Conversions::toAddressMode(desc.wrapU),
        .addressModeV = Conversions::toAddressMode(desc.wrapV),
        .addressModeW = Conversions::toAddressMode(desc.wrapW),
        .anisotropyEnable = anisotropic ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = anisotropic ? std::min(static_cast<float>(desc.maxAnisotropy), context->getProperties().limits.maxSamplerAnisotropy) : 1.0F,
        .compareEnable = desc.compare ? VK_TRUE : VK_FALSE,
        .compareOp = desc.compare ? Conversions::toCompare(*desc.compare) : VK_COMPARE_OP_ALWAYS,
        .minLod = desc.minLod,
        .maxLod = desc.maxLod,
        .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
    };
    auto record = std::make_unique<SamplerRecord>();
    Context::check(vk.vkCreateSampler(context->getDevice(), &info, nullptr, &record->sampler), "make a sampler");
    context->setName(VK_OBJECT_TYPE_SAMPLER, record->sampler, desc.label);
    try {
        return {samplers.add(std::move(record))};
    } catch (...) {
        vk.vkDestroySampler(context->getDevice(), record->sampler, nullptr);
        throw;
    }
}

// The blocks of a program take one set of dynamic uniform buffers, and its textures and samplers another, at the sets and bindings of the SPIR-V of its package. The layouts that a program skips are empty.
gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no program.");
    }
    for (const StageCode* stage : {&desc.vertex, &desc.fragment}) {
        if (stage->bytecode.empty() || stage->bytecode.size() % 4 != 0) {
            throw std::invalid_argument(std::format("The program \"{}\" has no SPIR-V code for a stage. Compile its shader again with the Haylen shader compiler.", desc.label));
        }
    }

    auto record = std::make_unique<ProgramRecord>();
    record->label = desc.label;
    record->vertexEntry = desc.vertex.entry.empty() ? "main" : std::string(desc.vertex.entry);
    record->fragmentEntry = desc.fragment.entry.empty() ? "main" : std::string(desc.fragment.entry);
    record->blockIndices.fill(-1);

    std::vector<LayoutCache::Binding> uniformBindings;
    for (const BlockBinding& block : desc.blocks) {
        if (block.size > context->getProperties().limits.maxUniformBufferRange) {
            throw std::invalid_argument(std::format("The uniform block at slot {} of the program \"{}\" is larger than the {} bytes that the GPU binds.", block.slot, desc.label, context->getProperties().limits.maxUniformBufferRange));
        }
        if (record->uniformSet != kNoSet && record->uniformSet != static_cast<std::uint32_t>(block.set)) {
            throw std::invalid_argument(std::format("The uniform blocks of the program \"{}\" lie in more than one descriptor set.", desc.label));
        }
        record->uniformSet = static_cast<std::uint32_t>(block.set);
        uniformBindings.push_back({.binding = static_cast<std::uint32_t>(block.binding), .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .stages = Conversions::toStage(block.stage)});
        record->uniformBlocks.push_back({.binding = static_cast<std::uint32_t>(block.binding), .size = block.size});
    }
    std::ranges::sort(uniformBindings);
    std::ranges::sort(record->uniformBlocks, {}, &DescriptorCache::UniformBlock::binding);
    for (const BlockBinding& block : desc.blocks) {
        const auto found = std::ranges::find(record->uniformBlocks, static_cast<std::uint32_t>(block.binding), &DescriptorCache::UniformBlock::binding);
        record->blockIndices[static_cast<std::size_t>(block.slot)] = static_cast<int>(found - record->uniformBlocks.begin());
    }

    std::vector<LayoutCache::Binding> resourceBindings;
    // clang-format off
    const auto addResource = [&](int set, int binding, VkDescriptorType type, Stage stage) {
        if (record->resourceSet != kNoSet && record->resourceSet != static_cast<std::uint32_t>(set)) {
            throw std::invalid_argument(std::format("The textures and samplers of the program \"{}\" lie in more than one descriptor set.", desc.label));
        }
        record->resourceSet = static_cast<std::uint32_t>(set);
        resourceBindings.push_back({.binding = static_cast<std::uint32_t>(binding), .type = type, .stages = Conversions::toStage(stage)});
    };
    // clang-format on
    for (const TextureBinding& texture : desc.textures) {
        addResource(texture.set, texture.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, texture.stage);
        record->textures.push_back({.slot = texture.slot, .binding = static_cast<std::uint32_t>(texture.binding)});
    }
    for (const SamplerBinding& sampler : desc.samplers) {
        addResource(sampler.set, sampler.binding, VK_DESCRIPTOR_TYPE_SAMPLER, sampler.stage);
        record->samplers.push_back({.slot = sampler.slot, .binding = static_cast<std::uint32_t>(sampler.binding)});
    }
    std::ranges::sort(resourceBindings);
    if (record->uniformSet != kNoSet && record->uniformSet == record->resourceSet) {
        throw std::invalid_argument(std::format("The program \"{}\" puts its uniform blocks and its textures in the same descriptor set.", desc.label));
    }

    if (record->uniformSet != kNoSet) {
        record->uniformLayout = layouts.getSetLayout(uniformBindings);
    }
    if (record->resourceSet != kNoSet) {
        record->resourceLayout = layouts.getSetLayout(resourceBindings);
    }
    const std::uint32_t setCount = std::max(record->uniformSet == kNoSet ? 0U : record->uniformSet + 1, record->resourceSet == kNoSet ? 0U : record->resourceSet + 1);
    std::vector<VkDescriptorSetLayout> sets(setCount, layouts.getSetLayout({}));
    if (record->uniformSet != kNoSet) {
        sets[record->uniformSet] = record->uniformLayout;
    }
    if (record->resourceSet != kNoSet) {
        sets[record->resourceSet] = record->resourceLayout;
    }
    record->layout = layouts.getPipelineLayout(sets);

    // The bytes of a module may lie anywhere, while Vulkan reads it as aligned words.
    // clang-format off
    const auto makeModule = [&](std::span<const std::byte> bytecode) {
        std::vector<std::uint32_t> words(bytecode.size() / 4);
        std::memcpy(words.data(), bytecode.data(), bytecode.size());
        VkShaderModule module = prepared->take(words);
        if (module != VK_NULL_HANDLE) {
            return module;
        }
        const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = bytecode.size(), .pCode = words.data()};
        Context::check(vk.vkCreateShaderModule(context->getDevice(), &info, nullptr, &module), std::format("make the shader modules of the program \"{}\"", desc.label));
        context->setName(VK_OBJECT_TYPE_SHADER_MODULE, module, desc.label);
        return module;
    };
    // clang-format on
    record->vertex = makeModule(desc.vertex.bytecode);
    try {
        record->fragment = makeModule(desc.fragment.bytecode);
        return {programs.add(std::move(record))};
    } catch (...) {
        vk.vkDestroyShaderModule(context->getDevice(), record->vertex, nullptr);
        vk.vkDestroyShaderModule(context->getDevice(), record->fragment, nullptr);
        throw;
    }
}

// Vulkan compiles the stages when a pipeline is made, so the I/O pool makes only their modules ahead.
void Device::prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) {
    // clang-format off
    const auto copyWords = [](std::span<const std::byte> bytecode) {
        std::vector<std::uint32_t> words(bytecode.size() / 4);
        std::memcpy(words.data(), bytecode.data(), words.size() * 4);
        return words;
    };
    jobs.postIo([modules = prepared, vertex = copyWords(desc.vertex.bytecode), fragment = copyWords(desc.fragment.bytecode)] {
        modules->prepare(vertex);
        modules->prepare(fragment);
    });
    // clang-format on
}

gpu::Device::Pipeline Device::makePipeline(const PipelineDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no pipeline.");
    }
    ProgramRecord* program = programs.find(desc.program.id);
    if (program == nullptr || program->destroyed) {
        throw std::logic_error(std::format("The program {} of the pipeline \"{}\" does not exist.", desc.program.id, desc.label));
    }

    auto record = std::make_unique<PipelineRecord>(PipelineRecord{.program = desc.program.id, .layout = program->layout, .indexType = desc.indexType, .bufferCount = static_cast<std::uint32_t>(desc.buffers.size()), .generation = swapchainTarget.generation});
    if (desc.swapchain) {
        record->stored = storePipeline(desc);
    }
    record->pipeline = buildPipeline(desc, *program);
    try {
        const std::uint32_t id = pipelines.add(std::move(record));
        ++program->pipelines;
        return {id};
    } catch (...) {
        vk.vkDestroyPipeline(context->getDevice(), record->pipeline, nullptr);
        throw;
    }
}

std::unique_ptr<Device::StoredPipeline> Device::storePipeline(const PipelineDesc& desc) {
    auto stored = std::make_unique<StoredPipeline>();
    stored->buffers.assign(desc.buffers.begin(), desc.buffers.end());
    stored->attributes.assign(desc.attributes.begin(), desc.attributes.end());
    stored->colors.assign(desc.colors.begin(), desc.colors.end());
    stored->label = desc.label;
    stored->desc = desc;
    stored->desc.buffers = stored->buffers;
    stored->desc.attributes = stored->attributes;
    stored->desc.colors = stored->colors;
    stored->desc.label = stored->label;
    return stored;
}

// A pipeline that draws into the swapchain takes its formats and sample count, and the others the formats and sample count that they name.
VkPipeline Device::buildPipeline(const PipelineDesc& desc, const ProgramRecord& program) {
    const std::array<VkPipelineShaderStageCreateInfo, 2> stages{{
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = program.vertex, .pName = program.vertexEntry.c_str()},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = program.fragment, .pName = program.fragmentEntry.c_str()},
    }};

    std::array<VkVertexInputBindingDescription, kMaxVertexBuffers> bindings{};
    for (std::size_t index = 0; index < desc.buffers.size(); ++index) {
        bindings[index] = {.binding = static_cast<std::uint32_t>(index), .stride = static_cast<std::uint32_t>(desc.buffers[index].stride), .inputRate = desc.buffers[index].perInstance ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX};
    }
    std::array<VkVertexInputAttributeDescription, kMaxAttributes> attributes{};
    for (std::size_t index = 0; index < desc.attributes.size(); ++index) {
        const VertexAttribute& attribute = desc.attributes[index];
        attributes[index] = {.location = static_cast<std::uint32_t>(attribute.location), .binding = static_cast<std::uint32_t>(attribute.buffer), .format = Conversions::toFormat(attribute.format), .offset = static_cast<std::uint32_t>(attribute.offset)};
    }
    const VkPipelineVertexInputStateCreateInfo vertexInput{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .vertexBindingDescriptionCount = static_cast<std::uint32_t>(desc.buffers.size()), .pVertexBindingDescriptions = bindings.data(), .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(desc.attributes.size()), .pVertexAttributeDescriptions = attributes.data()};

    // Strips restart at the largest index, as on Metal and Direct3D.
    const bool strip = desc.primitive == Primitive::TriangleStrip || desc.primitive == Primitive::LineStrip;
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = Conversions::toTopology(desc.primitive), .primitiveRestartEnable = strip && desc.indexType != IndexType::None ? VK_TRUE : VK_FALSE};
    const VkPipelineViewportStateCreateInfo viewport{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    const bool biased = desc.depthStencil.depthBias != 0.0F || desc.depthStencil.depthBiasSlope != 0.0F;
    const VkPipelineRasterizationStateCreateInfo rasterization{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = Conversions::toCullMode(desc.cullMode), .frontFace = Conversions::toFrontFace(desc.frontFace), .depthBiasEnable = biased ? VK_TRUE : VK_FALSE, .depthBiasConstantFactor = desc.depthStencil.depthBias, .depthBiasSlopeFactor = desc.depthStencil.depthBiasSlope, .lineWidth = 1.0F};

    const VkSampleCountFlagBits samples = desc.swapchain ? swapchainTarget.samples : Conversions::toSampleCount(desc.sampleCount);
    const VkPipelineMultisampleStateCreateInfo multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples = samples, .alphaToCoverageEnable = desc.alphaToCoverage ? VK_TRUE : VK_FALSE};

    const DepthStencil& depth = desc.depthStencil;
    const VkFormat depthFormat = desc.swapchain ? swapchainTarget.depth : (depth.format ? resolveFormat(*depth.format) : VK_FORMAT_UNDEFINED);
    // clang-format off
    const auto toStencilFace = [&depth](const StencilFace& face) {
        return VkStencilOpState{.failOp = Conversions::toStencilOperation(face.fail), .passOp = Conversions::toStencilOperation(face.pass), .depthFailOp = Conversions::toStencilOperation(face.depthFail), .compareOp = Conversions::toCompare(face.compare), .compareMask = depth.stencilReadMask, .writeMask = depth.stencilWriteMask};
    };
    // clang-format on
    const bool depthTested = depthFormat != VK_FORMAT_UNDEFINED && (depth.depthCompare != CompareFunction::Always || depth.depthWrite);
    const VkPipelineDepthStencilStateCreateInfo depthStencil{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .depthTestEnable = depthTested ? VK_TRUE : VK_FALSE, .depthWriteEnable = depthTested && depth.depthWrite ? VK_TRUE : VK_FALSE, .depthCompareOp = Conversions::toCompare(depth.depthCompare), .stencilTestEnable = depthFormat != VK_FORMAT_UNDEFINED && depth.stencilEnabled ? VK_TRUE : VK_FALSE, .front = toStencilFace(depth.stencilFront), .back = toStencilFace(depth.stencilBack), .maxDepthBounds = 1.0F};

    const std::uint32_t colorCount = desc.swapchain ? 1U : static_cast<std::uint32_t>(desc.colors.size());
    std::array<VkPipelineColorBlendAttachmentState, kMaxColorTargets> blends{};
    std::array<VkFormat, kMaxColorTargets> colorFormats{};
    for (std::uint32_t index = 0; index < colorCount; ++index) {
        const std::size_t source = capabilities.independentBlend ? index : 0;
        const ColorTarget target = source < desc.colors.size() ? desc.colors[source] : ColorTarget{};
        const Blend& blend = target.blend;
        blends[index] = {.blendEnable = blend.enabled ? VK_TRUE : VK_FALSE, .srcColorBlendFactor = Conversions::toBlendFactor(blend.sourceColor), .dstColorBlendFactor = Conversions::toBlendFactor(blend.destinationColor), .colorBlendOp = Conversions::toBlendOperation(blend.colorOperation), .srcAlphaBlendFactor = Conversions::toBlendFactor(blend.sourceAlpha), .dstAlphaBlendFactor = Conversions::toBlendFactor(blend.destinationAlpha), .alphaBlendOp = Conversions::toBlendOperation(blend.alphaOperation), .colorWriteMask = Conversions::toColorMask(target.writeMask)};
        colorFormats[index] = desc.swapchain ? swapchainTarget.color : resolveFormat(desc.colors[index].format);
    }
    const VkPipelineColorBlendStateCreateInfo colorBlend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount = colorCount, .pAttachments = blends.data()};

    const std::array<VkDynamicState, 4> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS, VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    const VkPipelineDynamicStateCreateInfo dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

    const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .colorAttachmentCount = colorCount, .pColorAttachmentFormats = colorFormats.data(), .depthAttachmentFormat = depthFormat, .stencilAttachmentFormat = Conversions::hasStencil(depthFormat) ? depthFormat : VK_FORMAT_UNDEFINED};
    VkRenderPass compatible = VK_NULL_HANDLE;
    if (!dynamicRendering) {
        RenderPassCache::PassKey key{.colorCount = colorCount};
        for (std::uint32_t index = 0; index < colorCount; ++index) {
            key.colors[index] = {.format = colorFormats[index], .samples = samples};
        }
        if (depthFormat != VK_FORMAT_UNDEFINED) {
            key.depth = {.format = depthFormat, .samples = samples};
        }
        compatible = renderPasses.getRenderPass(key);
    }

    const VkGraphicsPipelineCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = dynamicRendering ? &rendering : nullptr,
        .stageCount = static_cast<std::uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamic,
        .layout = program.layout,
        .renderPass = compatible,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    Context::check(vk.vkCreateGraphicsPipelines(context->getDevice(), pipelineCache, 1, &info, nullptr, &pipeline), std::format("make the pipeline \"{}\"", desc.label));
    context->setName(VK_OBJECT_TYPE_PIPELINE, pipeline, desc.label);
    return pipeline;
}

// The frames in flight may still draw with the old pipeline, which waits for them.
void Device::rebuildPipeline(PipelineRecord& record) {
    const ProgramRecord& program = *programs.find(record.program);
    const VkPipeline rebuilt = buildPipeline(record.stored->desc, program);
    releases.push(ReleaseQueue::Pipeline{.pipeline = record.pipeline}, nextSerial);
    record.pipeline = rebuilt;
    record.generation = swapchainTarget.generation;
}

void Device::destroy(Buffer buffer) noexcept {
    std::unique_ptr<BufferRecord> record = buffers.remove(buffer.id);
    if (record) {
        releases.push(ReleaseQueue::Buffer{.buffer = record->buffer, .allocation = record->allocation}, nextSerial);
    }
}

void Device::destroy(Image image) noexcept {
    std::unique_ptr<ImageRecord> record = images.remove(image.id);
    if (!record) {
        return;
    }
    descriptors.forgetImage(image.id, nextSerial);
    releaseImage(*record, nextSerial);
}

void Device::releaseImage(ImageRecord& record, std::uint64_t serial) {
    for (const AttachmentView& view : record.attachmentViews) {
        renderPasses.forgetView(view.view, serial);
        releases.push(ReleaseQueue::ImageView{.view = view.view}, serial);
    }
    if (record.sampleView != VK_NULL_HANDLE) {
        releases.push(ReleaseQueue::ImageView{.view = record.sampleView}, serial);
    }
    releases.push(ReleaseQueue::Image{.image = record.image, .allocation = record.allocation}, serial);
}

void Device::destroy(Sampler sampler) noexcept {
    std::unique_ptr<SamplerRecord> record = samplers.remove(sampler.id);
    if (!record) {
        return;
    }
    descriptors.forgetSampler(sampler.id, nextSerial);
    releases.push(ReleaseQueue::Sampler{.sampler = record->sampler}, nextSerial);
}

// The modules of a program stay while pipelines that draw into the swapchain may need them to make themselves again.
void Device::destroy(Program program) noexcept {
    ProgramRecord* record = programs.find(program.id);
    if (record == nullptr || record->destroyed) {
        return;
    }
    record->destroyed = true;
    if (record->pipelines == 0) {
        releaseProgram(*record, nextSerial);
        programs.remove(program.id).reset();
    }
}

void Device::releaseProgram(ProgramRecord& record, std::uint64_t serial) {
    releases.push(ReleaseQueue::ShaderModule{.module = record.vertex}, serial);
    releases.push(ReleaseQueue::ShaderModule{.module = record.fragment}, serial);
}

void Device::forgetProgramUse(std::uint32_t program, std::uint64_t serial) noexcept {
    ProgramRecord* record = programs.find(program);
    if (record == nullptr) {
        return;
    }
    --record->pipelines;
    if (record->destroyed && record->pipelines == 0) {
        releaseProgram(*record, serial);
        programs.remove(program).reset();
    }
}

void Device::destroy(Pipeline pipeline) noexcept {
    std::unique_ptr<PipelineRecord> record = pipelines.remove(pipeline.id);
    if (!record) {
        return;
    }
    if (bound.pipeline == record.get()) {
        bound.pipeline = nullptr;
        bound.program = nullptr;
    }
    releases.push(ReleaseQueue::Pipeline{.pipeline = record->pipeline}, nextSerial);
    forgetProgramUse(record->program, nextSerial);
}

// The first write of a frame moves a dynamic buffer to its next copy, which the GPU finished reading, since a frame in flight reads every other copy at most.
void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    BufferRecord& record = requireBuffer(buffer);
    if (record.usage != Usage::Dynamic) {
        throw std::logic_error(std::format("The buffer {} is immutable, so it takes no writes.", buffer.id));
    }
    if (offset + data.size() > record.size) {
        throw std::out_of_range(std::format("A write of {} bytes at {} goes past the end of the buffer of {} bytes.", data.size(), offset, record.size));
    }
    if (record.writtenSerial != nextSerial) {
        record.writtenSerial = nextSerial;
        record.current = (record.current + 1) % record.copies;
    }
    const VkDeviceSize start = record.stride * record.current + offset;
    std::memcpy(record.mapped + start, data.data(), data.size());
    vmaFlushAllocation(context->getAllocator(), record.allocation, start, data.size());
}

// The pixels of every image go into the staging ring first, and the copies wait for the frames in flight that sample the images.
void Device::writeImages(std::span<const ImageWrite> writes) {
    if (writes.empty() || !ensureFrame()) {
        return;
    }
    const VkCommandBuffer commands = frames[frameIndex].uploads;
    copyRegions.clear();
    imageBarriers.clear();
    std::vector<VkBuffer>& sourceBuffers = stagingSources;
    sourceBuffers.clear();
    for (const ImageWrite& write : writes) {
        const ImageRecord& record = requireImage(write.image);
        const bool volume = record.type == TextureType::Volume;
        const std::size_t bytes = std::size_t{static_cast<std::uint32_t>(record.width)} * static_cast<std::uint32_t>(record.height) * static_cast<std::uint32_t>(record.layers) * Conversions::getBytesPerPixel(record.pixelFormat);
        if (write.pixels.size() != bytes) {
            throw std::invalid_argument(std::format("The pixels of the image {} hold {} bytes, not the {} it needs.", write.image.id, write.pixels.size(), bytes));
        }
        const UploadRing::Allocation source = staging.allocate(bytes, 16);
        std::memcpy(source.data, write.pixels.data(), bytes);
        sourceBuffers.push_back(source.buffer);
        copyRegions.push_back({.bufferOffset = source.offset, .imageSubresource = {.aspectMask = record.aspects, .layerCount = volume ? 1U : static_cast<std::uint32_t>(record.layers)}, .imageExtent = {static_cast<std::uint32_t>(record.width), static_cast<std::uint32_t>(record.height), volume ? static_cast<std::uint32_t>(record.layers) : 1U}});
        imageBarriers.push_back({.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = record.image, .subresourceRange = {.aspectMask = record.aspects, .levelCount = 1, .layerCount = VK_REMAINING_ARRAY_LAYERS}});
    }

    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
    for (std::size_t index = 0; index < writes.size(); ++index) {
        vk.vkCmdCopyBufferToImage(commands, sourceBuffers[index], imageBarriers[index].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegions[index]);
    }
    VkPipelineStageFlags restStages = 0;
    for (std::size_t index = 0; index < writes.size(); ++index) {
        VkImageMemoryBarrier& barrier = imageBarriers[index];
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = images.find(writes[index].image.id)->restLayout;
        barrier.dstAccessMask = ImageLayouts::getAccess(barrier.newLayout);
        restStages |= ImageLayouts::getStages(barrier.newLayout);
    }
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, restStages, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
}

void Device::beginPass(const PassDesc& desc) {
    if (pass.open) {
        throw std::logic_error("A pass is already open. Call \"endPass\" before \"beginPass\".");
    }
    pass.open = true;
    pass.skipped = false;
    pass.labeled = false;
    pass.transitionCount = 0;
    if (!ensureFrame()) {
        pass.skipped = true;
        return;
    }
    resetDrawState();

    const VkCommandBuffer commands = frames[frameIndex].graphics;
    if (labels && !desc.label.empty()) {
        labelText.assign(desc.label);
        const VkDebugUtilsLabelEXT label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = labelText.c_str()};
        vkCmdBeginDebugUtilsLabelEXT(commands, &label);
        pass.labeled = true;
    }

    if (desc.colors.empty() && !desc.depthStencil) {
        beginSwapchainPass(desc);
    } else {
        beginOffscreenPass(desc);
    }
    if (pass.skipped) {
        if (pass.labeled) {
            vkCmdEndDebugUtilsLabelEXT(commands);
            pass.labeled = false;
        }
        return;
    }

    const VkViewport viewport{.x = 0.0F, .y = static_cast<float>(pass.extent.height), .width = static_cast<float>(pass.extent.width), .height = -static_cast<float>(pass.extent.height), .minDepth = 0.0F, .maxDepth = 1.0F};
    const VkRect2D scissor{.offset = {0, 0}, .extent = pass.extent};
    const std::array<float, 4> blendColor{};
    vk.vkCmdSetViewport(commands, 0, 1, &viewport);
    vk.vkCmdSetScissor(commands, 0, 1, &scissor);
    vk.vkCmdSetBlendConstants(commands, blendColor.data());
    vk.vkCmdSetStencilReference(commands, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
}

// The depth and stencil and the multisampled color of the swapchain last for the pass, while its image keeps what the passes of the frame drew until it is presented.
void Device::beginSwapchainPass(const PassDesc& desc) {
    if (!acquireSwapchainImage()) {
        pass.skipped = true;
        return;
    }
    const VkCommandBuffer commands = frames[frameIndex].graphics;
    const bool multisampled = swapchain->getSamples() != VK_SAMPLE_COUNT_1_BIT;
    const VkAttachmentLoadOp load = Conversions::toLoad(desc.load);
    const VkImageLayout previous = swapchainDrawn && desc.load == Load::Keep ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;

    // The image of the swapchain keeps what earlier passes of the frame drew when the pass keeps it, while the attachments that last for one pass start anew.
    imageBarriers.clear();
    const VkImageSubresourceRange colorRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .levelCount = 1, .layerCount = 1};
    const VkImageLayout colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkImageLayout depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageBarriers.push_back(ImageLayouts::makeBarrier(swapchain->getImage(imageIndex), colorRange, swapchainDrawn ? colorLayout : VK_IMAGE_LAYOUT_UNDEFINED, previous, colorLayout));
    if (multisampled) {
        imageBarriers.push_back(ImageLayouts::makeBarrier(swapchain->getColorImage(), colorRange, colorLayout, VK_IMAGE_LAYOUT_UNDEFINED, colorLayout));
    }
    imageBarriers.push_back(ImageLayouts::makeBarrier(swapchain->getDepthImage(), {.aspectMask = Conversions::getAspects(swapchain->getDepthFormat()), .levelCount = 1, .layerCount = 1}, depthLayout, VK_IMAGE_LAYOUT_UNDEFINED, depthLayout));
    const VkPipelineStageFlags stages = ImageLayouts::getStages(colorLayout) | ImageLayouts::getStages(depthLayout);
    vk.vkCmdPipelineBarrier(commands, stages, stages, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
    swapchainDrawn = true;

    PassTarget target{.colorCount = 1, .hasDepth = true, .extent = swapchain->getExtent()};
    AttachmentTarget& color = target.colors[0];
    color = {.view = multisampled ? swapchain->getColorView() : swapchain->getView(imageIndex), .format = swapchain->getFormat(), .samples = swapchain->getSamples(), .load = load, .store = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE};
    color.clear.color = {{desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a}};
    if (multisampled) {
        color.resolveView = swapchain->getView(imageIndex);
        color.resolveFormat = swapchain->getFormat();
    }
    target.depth = {.view = swapchain->getDepthView(), .format = swapchain->getDepthFormat(), .samples = swapchain->getSamples(), .load = load, .store = VK_ATTACHMENT_STORE_OP_DONT_CARE, .stencilLoad = load, .stencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE};
    target.depth.clear.depthStencil = {.depth = desc.clearDepth, .stencil = desc.clearStencil};
    beginRendering(target);
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    PassTarget target{.colorCount = static_cast<std::uint32_t>(desc.colors.size())};
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& attachment = desc.colors[index];
        ImageRecord& record = requireImage(attachment.image);
        if (record.kind != ImageKind::RenderTarget) {
            throw std::logic_error(std::format("The image {} is not a render target, so no pass draws into it.", attachment.image.id));
        }
        AttachmentTarget& color = target.colors[index];
        color = {.view = getAttachmentView(record, attachment.mipLevel, attachment.layer), .format = record.format, .samples = record.samples, .load = Conversions::toLoad(attachment.load), .store = Conversions::toStore(attachment.store)};
        color.clear.color = {{attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a}};
        addTransition(record, attachment.mipLevel, attachment.layer, attachment.load == Load::Keep);
        if (index == 0) {
            target.extent = {static_cast<std::uint32_t>(std::max(record.width >> attachment.mipLevel, 1)), static_cast<std::uint32_t>(std::max(record.height >> attachment.mipLevel, 1))};
        }
        if (attachment.resolve.isValid()) {
            ImageRecord& resolve = requireImage(attachment.resolve);
            color.resolveView = getAttachmentView(resolve, 0, 0);
            color.resolveFormat = resolve.format;
            addTransition(resolve, 0, 0, false);
        }
    }
    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        ImageRecord& record = requireImage(attachment.image);
        if (record.kind != ImageKind::DepthStencil) {
            throw std::logic_error(std::format("The image {} is not a depth stencil image, so no pass tests against it.", attachment.image.id));
        }
        target.hasDepth = true;
        target.depth = {.view = getAttachmentView(record, 0, 0), .format = record.format, .samples = record.samples, .load = Conversions::toLoad(attachment.depthLoad), .store = Conversions::toStore(attachment.depthStore), .stencilLoad = Conversions::toLoad(attachment.stencilLoad), .stencilStore = Conversions::toStore(attachment.stencilStore)};
        target.depth.clear.depthStencil = {.depth = attachment.clearDepth, .stencil = attachment.clearStencil};
        const bool keeps = attachment.depthLoad == Load::Keep || attachment.stencilLoad == Load::Keep;
        addTransition(record, 0, 0, keeps);
        if (desc.colors.empty()) {
            target.extent = {static_cast<std::uint32_t>(record.width), static_cast<std::uint32_t>(record.height)};
        }
    }

    // Each attachment waits for what earlier passes and copies wrote into it and read from it.
    imageBarriers.clear();
    VkPipelineStageFlags before = 0;
    VkPipelineStageFlags after = 0;
    for (std::size_t index = 0; index < pass.transitionCount; ++index) {
        const Transition& transition = pass.transitions[index];
        imageBarriers.push_back(ImageLayouts::makeBarrier(transition.image, transition.range, transition.restLayout, transition.fromLayout, transition.attachmentLayout));
        before |= ImageLayouts::getStages(transition.restLayout);
        after |= ImageLayouts::getStages(transition.attachmentLayout);
    }
    vk.vkCmdPipelineBarrier(frames[frameIndex].graphics, before, after, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
    beginRendering(target);
}

// An attachment whose contents the pass discards leaves its resting layout as undefined, which spares the GPU keeping them.
void Device::addTransition(const ImageRecord& record, int mipLevel, int layer, bool keeps) {
    const bool depth = (record.aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;
    const bool volume = record.type == TextureType::Volume;
    pass.transitions[pass.transitionCount++] = {
        .image = record.image,
        .range = {.aspectMask = record.aspects, .baseMipLevel = static_cast<std::uint32_t>(mipLevel), .levelCount = 1, .baseArrayLayer = volume ? 0U : static_cast<std::uint32_t>(layer), .layerCount = 1},
        .attachmentLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .fromLayout = keeps ? record.restLayout : VK_IMAGE_LAYOUT_UNDEFINED,
        .restLayout = record.restLayout,
    };
}

VkImageView Device::getAttachmentView(ImageRecord& record, int mipLevel, int layer) {
    const auto found = std::ranges::find_if(record.attachmentViews, [mipLevel, layer](const AttachmentView& view) { return view.mipLevel == mipLevel && view.layer == layer; });
    if (found != record.attachmentViews.end()) {
        return found->view;
    }
    if (mipLevel < 0 || mipLevel >= record.mipLevels || layer < 0 || layer >= record.layers) {
        throw std::out_of_range(std::format("The image has no mip level {} with a layer {} to draw into.", mipLevel, layer));
    }
    const VkImageViewCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .image = record.image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = record.format, .subresourceRange = {.aspectMask = record.aspects, .baseMipLevel = static_cast<std::uint32_t>(mipLevel), .levelCount = 1, .baseArrayLayer = static_cast<std::uint32_t>(layer), .layerCount = 1}};
    VkImageView view = VK_NULL_HANDLE;
    Context::check(vk.vkCreateImageView(context->getDevice(), &info, nullptr, &view), "make the view of an attachment");
    record.attachmentViews.push_back({.mipLevel = mipLevel, .layer = layer, .view = view});
    return view;
}

void Device::beginRendering(const PassTarget& target) {
    const VkCommandBuffer commands = frames[frameIndex].graphics;
    const VkRect2D area{.offset = {0, 0}, .extent = target.extent};
    pass.extent = target.extent;
    if (dynamicRendering) {
        std::array<VkRenderingAttachmentInfo, kMaxColorTargets> colors{};
        for (std::uint32_t index = 0; index < target.colorCount; ++index) {
            const AttachmentTarget& color = target.colors[index];
            colors[index] = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = color.view, .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .resolveMode = color.resolveView != VK_NULL_HANDLE ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE, .resolveImageView = color.resolveView, .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = color.load, .storeOp = color.store, .clearValue = color.clear};
        }
        const AttachmentTarget& depth = target.depth;
        const VkRenderingAttachmentInfo depthInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = depth.view, .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .loadOp = depth.load, .storeOp = depth.store, .clearValue = depth.clear};
        const VkRenderingAttachmentInfo stencilInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .imageView = depth.view, .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .loadOp = depth.stencilLoad, .storeOp = depth.stencilStore, .clearValue = depth.clear};
        const bool stencil = target.hasDepth && Conversions::hasStencil(depth.format);
        const VkRenderingInfo info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .renderArea = area, .layerCount = 1, .colorAttachmentCount = target.colorCount, .pColorAttachments = colors.data(), .pDepthAttachment = target.hasDepth ? &depthInfo : nullptr, .pStencilAttachment = stencil ? &stencilInfo : nullptr};
        beginRenderingCommand(commands, &info);
        return;
    }

    // The attachments of a render pass come in the order of its creation: the colors, the images that resolve them and the depth and stencil image.
    RenderPassCache::PassKey key{.colorCount = target.colorCount};
    std::array<VkImageView, RenderPassCache::kMaxViews> views{};
    std::array<VkClearValue, RenderPassCache::kMaxViews> clears{};
    std::uint32_t count = 0;
    for (std::uint32_t index = 0; index < target.colorCount; ++index) {
        const AttachmentTarget& color = target.colors[index];
        key.colors[index] = {.format = color.format, .samples = color.samples, .load = color.load, .store = color.store};
        views[count] = color.view;
        clears[count++] = color.clear;
    }
    for (std::uint32_t index = 0; index < target.colorCount; ++index) {
        const AttachmentTarget& color = target.colors[index];
        if (color.resolveView != VK_NULL_HANDLE) {
            key.resolves[index] = color.resolveFormat;
            views[count++] = color.resolveView;
        }
    }
    if (target.hasDepth) {
        const AttachmentTarget& depth = target.depth;
        key.depth = {.format = depth.format, .samples = depth.samples, .load = depth.load, .store = depth.store, .stencilLoad = depth.stencilLoad, .stencilStore = depth.stencilStore};
        views[count] = depth.view;
        clears[count++] = depth.clear;
    }
    const VkRenderPass renderPass = renderPasses.getRenderPass(key);
    const VkFramebuffer framebuffer = renderPasses.getFramebuffer(renderPass, std::span(views.data(), count), target.extent);
    const VkRenderPassBeginInfo info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .renderPass = renderPass, .framebuffer = framebuffer, .renderArea = area, .clearValueCount = count, .pClearValues = clears.data()};
    vk.vkCmdBeginRenderPass(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
}

// The attachments go back to their resting layouts, where later passes sample them and readbacks copy them.
void Device::endPass() {
    if (!pass.open) {
        throw std::logic_error("No pass is open. Call \"beginPass\" before \"endPass\".");
    }
    pass.open = false;
    if (pass.skipped) {
        return;
    }

    const VkCommandBuffer commands = frames[frameIndex].graphics;
    if (dynamicRendering) {
        endRenderingCommand(commands);
    } else {
        vk.vkCmdEndRenderPass(commands);
    }

    if (pass.transitionCount > 0) {
        imageBarriers.clear();
        VkPipelineStageFlags before = 0;
        VkPipelineStageFlags after = 0;
        for (std::size_t index = 0; index < pass.transitionCount; ++index) {
            const Transition& transition = pass.transitions[index];
            imageBarriers.push_back(ImageLayouts::makeBarrier(transition.image, transition.range, transition.attachmentLayout, transition.attachmentLayout, transition.restLayout));
            before |= ImageLayouts::getStages(transition.attachmentLayout);
            after |= ImageLayouts::getStages(transition.restLayout);
        }
        vk.vkCmdPipelineBarrier(commands, before, after, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
    }
    if (pass.labeled) {
        vkCmdEndDebugUtilsLabelEXT(commands);
    }
}

void Device::applyViewport(const math::Rect& pixels) {
    if (!pass.open || pass.skipped) {
        return;
    }
    const VkViewport viewport{.x = pixels.x, .y = pixels.y + pixels.height, .width = pixels.width, .height = -pixels.height, .minDepth = 0.0F, .maxDepth = 1.0F};
    vk.vkCmdSetViewport(frames[frameIndex].graphics, 0, 1, &viewport);
}

// Vulkan takes a scissor inside the attachments with a positive origin, so the rectangle is rounded to pixels and clipped to them.
void Device::applyScissor(const math::Rect& pixels) {
    if (!pass.open || pass.skipped) {
        return;
    }
    const auto width = static_cast<long>(pass.extent.width);
    const auto height = static_cast<long>(pass.extent.height);
    const long left = std::clamp(std::lround(pixels.x), 0L, width);
    const long top = std::clamp(std::lround(pixels.y), 0L, height);
    const long right = std::clamp(std::lround(pixels.x + pixels.width), left, width);
    const long bottom = std::clamp(std::lround(pixels.y + pixels.height), top, height);
    const VkRect2D scissor{.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)}, .extent = {static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top)}};
    vk.vkCmdSetScissor(frames[frameIndex].graphics, 0, 1, &scissor);
}

void Device::applyPipeline(Pipeline pipeline) {
    if (!pass.open || pass.skipped) {
        return;
    }
    PipelineRecord* record = pipelines.find(pipeline.id);
    if (record == nullptr) {
        throw std::logic_error(std::format("The pipeline {} does not exist.", pipeline.id));
    }
    if (record->stored && record->generation != swapchainTarget.generation) {
        rebuildPipeline(*record);
    }
    if (bound.pipeline == record && bound.boundPipeline == record->pipeline) {
        return;
    }

    ProgramRecord* program = programs.find(record->program);
    vk.vkCmdBindPipeline(frames[frameIndex].graphics, VK_PIPELINE_BIND_POINT_GRAPHICS, record->pipeline);
    if (bound.program == nullptr || bound.program->layout != program->layout) {
        bound.resourcesDirty = true;
        bound.uniformsDirty = true;
    }
    if (bound.program == nullptr || bound.program->uniformLayout != program->uniformLayout) {
        bound.uniformMask = 0;
        bound.uniformSet = VK_NULL_HANDLE;
    }
    bound.vertexDirty = true;
    bound.indexDirty = true;
    bound.pipeline = record;
    bound.program = program;
    bound.boundPipeline = record->pipeline;
}

void Device::applyBindings(const Bindings& bindings) {
    if (!pass.open || pass.skipped) {
        return;
    }
    Bindings& current = bound.bindings;
    if (current.vertexBuffers != bindings.vertexBuffers || current.vertexOffsets != bindings.vertexOffsets) {
        bound.vertexDirty = true;
    }
    if (current.indexBuffer != bindings.indexBuffer || current.indexOffset != bindings.indexOffset) {
        bound.indexDirty = true;
    }
    if (current.textures != bindings.textures || current.samplers != bindings.samplers) {
        bound.resourcesDirty = true;
    }
    current = bindings;
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    if (!pass.open || pass.skipped) {
        return;
    }
    if (bound.program == nullptr) {
        throw std::logic_error("A pipeline applies before its uniforms. Call \"applyPipeline\" first.");
    }
    const int index = slot >= 0 && slot < static_cast<int>(kMaxBlocks) ? bound.program->blockIndices[static_cast<std::size_t>(slot)] : -1;
    if (index < 0) {
        throw std::logic_error(std::format("The program \"{}\" has no uniform block at slot {}.", bound.program->label, slot));
    }
    const VkDeviceSize size = bound.program->uniformBlocks[static_cast<std::size_t>(index)].size;
    if (data.size() > size) {
        throw std::invalid_argument(std::format("The uniforms of {} bytes do not fit the block of {} bytes at slot {}.", data.size(), size, slot));
    }
    const UploadRing::Allocation allocation = allocateUniform(static_cast<std::size_t>(index), size);
    std::memcpy(allocation.data, data.data(), data.size());
}

// Every block of a draw lies in one buffer of the ring, which its descriptor set names, so the blocks applied before the ring moved to another buffer move along.
UploadRing::Allocation Device::allocateUniform(std::size_t index, VkDeviceSize size) {
    const VkDeviceSize alignment = context->getProperties().limits.minUniformBufferOffsetAlignment;
    const UploadRing::Allocation allocation = uniforms.allocate(size, alignment);
    if (allocation.buffer != bound.uniformBuffer && bound.uniformBuffer != VK_NULL_HANDLE && bound.program != nullptr) {
        for (std::size_t other = 0; other < bound.program->uniformBlocks.size(); ++other) {
            if (other == index || (bound.uniformMask & (1U << other)) == 0) {
                continue;
            }
            const VkDeviceSize otherSize = bound.program->uniformBlocks[other].size;
            const UploadRing::Allocation moved = uniforms.allocate(otherSize, alignment);
            std::memcpy(moved.data, bound.uniformData[other], static_cast<std::size_t>(otherSize));
            bound.uniformOffsets[other] = static_cast<std::uint32_t>(moved.offset);
            bound.uniformData[other] = moved.data;
        }
    }
    if (allocation.buffer != bound.uniformBuffer) {
        bound.uniformBuffer = allocation.buffer;
        bound.uniformSet = VK_NULL_HANDLE;
    }
    bound.uniformOffsets[index] = static_cast<std::uint32_t>(allocation.offset);
    bound.uniformData[index] = allocation.data;
    bound.uniformMask |= 1U << index;
    bound.uniformsDirty = true;
    return allocation;
}

void Device::applyStencilReference(std::uint8_t value) {
    if (!pass.open || pass.skipped) {
        return;
    }
    vk.vkCmdSetStencilReference(frames[frameIndex].graphics, VK_STENCIL_FACE_FRONT_AND_BACK, value);
}

void Device::applyBlendColor(math::Color value) {
    if (!pass.open || pass.skipped) {
        return;
    }
    const std::array<float, 4> constants{value.r, value.g, value.b, value.a};
    vk.vkCmdSetBlendConstants(frames[frameIndex].graphics, constants.data());
}

void Device::draw(int first, int count, int instances) {
    if (!pass.open) {
        throw std::logic_error("A draw happens inside a pass. Call \"beginPass\" first.");
    }
    if (pass.skipped || count <= 0 || instances <= 0) {
        return;
    }
    if (bound.pipeline == nullptr) {
        throw std::logic_error("A draw needs a pipeline. Call \"applyPipeline\" first.");
    }

    const bool indexed = bound.pipeline->indexType != IndexType::None;
    if (bound.vertexDirty) {
        bindVertexBuffers();
    }
    if (bound.indexDirty && indexed) {
        bindIndexBuffer();
    }
    if (bound.resourcesDirty) {
        bindResources();
    }
    if (bound.uniformsDirty) {
        bindUniforms();
    }

    const VkCommandBuffer commands = frames[frameIndex].graphics;
    if (indexed) {
        vk.vkCmdDrawIndexed(commands, static_cast<std::uint32_t>(count), static_cast<std::uint32_t>(instances), static_cast<std::uint32_t>(first), 0, 0);
    } else {
        vk.vkCmdDraw(commands, static_cast<std::uint32_t>(count), static_cast<std::uint32_t>(instances), static_cast<std::uint32_t>(first), 0);
    }
}

void Device::resetDrawState() noexcept {
    bound.pipeline = nullptr;
    bound.program = nullptr;
    bound.boundPipeline = VK_NULL_HANDLE;
    bound.bindings = {};
    bound.vertexDirty = true;
    bound.indexDirty = true;
    bound.resourcesDirty = true;
    bound.uniformsDirty = true;
    bound.uniformSet = VK_NULL_HANDLE;
    bound.uniformMask = 0;
}

void Device::bindVertexBuffers() {
    const std::uint32_t count = bound.pipeline->bufferCount;
    std::array<VkBuffer, kMaxVertexBuffers> handles{};
    std::array<VkDeviceSize, kMaxVertexBuffers> offsets{};
    for (std::uint32_t index = 0; index < count; ++index) {
        const BufferRecord& record = requireBuffer(bound.bindings.vertexBuffers[index]);
        handles[index] = record.buffer;
        offsets[index] = record.stride * record.current + static_cast<VkDeviceSize>(bound.bindings.vertexOffsets[index]);
    }
    if (count > 0) {
        vk.vkCmdBindVertexBuffers(frames[frameIndex].graphics, 0, count, handles.data(), offsets.data());
    }
    bound.vertexDirty = false;
}

void Device::bindIndexBuffer() {
    const BufferRecord& record = requireBuffer(bound.bindings.indexBuffer);
    const VkDeviceSize offset = record.stride * record.current + static_cast<VkDeviceSize>(bound.bindings.indexOffset);
    vk.vkCmdBindIndexBuffer(frames[frameIndex].graphics, record.buffer, offset, Conversions::toIndexType(bound.pipeline->indexType));
    bound.indexDirty = false;
}

// A set of textures and samplers is written once for its choice of them, and later draws find it in the cache.
void Device::bindResources() {
    const ProgramRecord& program = *bound.program;
    bound.resourcesDirty = false;
    if (program.resourceLayout == VK_NULL_HANDLE) {
        return;
    }

    DescriptorCache::ResourceKey key{.layout = program.resourceLayout};
    for (const SlotBinding& texture : program.textures) {
        key.images[static_cast<std::size_t>(texture.slot)] = bound.bindings.textures[static_cast<std::size_t>(texture.slot)].id;
    }
    for (const SlotBinding& sampler : program.samplers) {
        key.samplers[static_cast<std::size_t>(sampler.slot)] = bound.bindings.samplers[static_cast<std::size_t>(sampler.slot)].id;
    }
    VkDescriptorSet set = descriptors.findResourceSet(key, nextSerial);
    if (set == VK_NULL_HANDLE) {
        descriptorImages.resize(program.textures.size() + program.samplers.size());
        descriptorWrites.clear();
        std::size_t next = 0;
        for (const SlotBinding& texture : program.textures) {
            const ImageRecord* image = images.find(key.images[static_cast<std::size_t>(texture.slot)]);
            if (image == nullptr || image->sampleView == VK_NULL_HANDLE) {
                throw std::logic_error(std::format("The draw binds no texture that shaders sample at slot {}, which the program \"{}\" reads.", texture.slot, program.label));
            }
            descriptorImages[next] = {.imageView = image->sampleView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = texture.binding, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &descriptorImages[next++]});
        }
        for (const SlotBinding& sampler : program.samplers) {
            const SamplerRecord* record = samplers.find(key.samplers[static_cast<std::size_t>(sampler.slot)]);
            if (record == nullptr) {
                throw std::logic_error(std::format("The draw binds no sampler at slot {}, which the program \"{}\" reads.", sampler.slot, program.label));
            }
            descriptorImages[next] = {.sampler = record->sampler};
            descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = sampler.binding, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &descriptorImages[next++]});
        }
        set = descriptors.addResourceSet(key, descriptorWrites, nextSerial);
    }
    vk.vkCmdBindDescriptorSets(frames[frameIndex].graphics, VK_PIPELINE_BIND_POINT_GRAPHICS, program.layout, program.resourceSet, 1, &set, 0, nullptr);
}

// Blocks that no draw of the frame applied read zeros rather than another frame's data.
void Device::bindUniforms() {
    const ProgramRecord& program = *bound.program;
    bound.uniformsDirty = false;
    if (program.uniformLayout == VK_NULL_HANDLE) {
        return;
    }

    for (std::size_t index = 0; index < program.uniformBlocks.size(); ++index) {
        if ((bound.uniformMask & (1U << index)) == 0) {
            const UploadRing::Allocation allocation = allocateUniform(index, program.uniformBlocks[index].size);
            std::memset(allocation.data, 0, static_cast<std::size_t>(program.uniformBlocks[index].size));
        }
    }
    if (bound.uniformSet == VK_NULL_HANDLE || bound.uniformSetLayout != program.uniformLayout) {
        bound.uniformSet = descriptors.getUniformSet(program.uniformLayout, bound.uniformBuffer, program.uniformBlocks);
        bound.uniformSetLayout = program.uniformLayout;
    }
    bound.uniformsDirty = false;
    vk.vkCmdBindDescriptorSets(frames[frameIndex].graphics, VK_PIPELINE_BIND_POINT_GRAPHICS, program.layout, program.uniformSet, 1, &bound.uniformSet, static_cast<std::uint32_t>(program.uniformBlocks.size()), bound.uniformOffsets.data());
}

void Device::pushDebugGroup(std::string_view name) {
    if (!labels || !ensureFrame()) {
        return;
    }
    labelText.assign(name);
    const VkDebugUtilsLabelEXT label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = labelText.c_str()};
    vkCmdBeginDebugUtilsLabelEXT(frames[frameIndex].graphics, &label);
}

void Device::popDebugGroup() {
    if (!labels || !frameOpen) {
        return;
    }
    vkCmdEndDebugUtilsLabelEXT(frames[frameIndex].graphics);
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it reads back no image.");
    }
    const ImageRecord& record = requireImage(image);
    if (record.kind != ImageKind::RenderTarget || record.samples != VK_SAMPLE_COUNT_1_BIT) {
        throw std::invalid_argument(std::format("The image {} is not a render target of one sample, so it cannot be read back.", image.id));
    }
    if (width < 1 || height < 1 || width > record.width || height > record.height) {
        throw std::invalid_argument(std::format("A readback of {}x{} pixels does not fit the render target of {}x{} pixels.", width, height, record.width, record.height));
    }
    return std::make_unique<ImageReadback>(readbacks.start({.image = record.image, .layout = record.restLayout, .width = width, .height = height, .format = record.pixelFormat}));
}

} // namespace haylen::gpu::vulkan
