#include "gpu/vulkan/Device.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

#include "gpu/DriverIssues.hpp"
#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "gpu/ProgramSlots.hpp"
#include "gpu/SamplerRules.hpp"
#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "gpu/vulkan/CopyReadback.hpp"
#include "gpu/vulkan/ImageLayouts.hpp"
#include "gpu/vulkan/Instance.hpp"
#include "gpu/vulkan/PreparedModules.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/gpu/ContractError.hpp"
#include "haylen/gpu/SurfaceTurn.hpp"

namespace haylen::gpu::vulkan {

std::unique_ptr<gpu::Device> Device::create(const std::optional<Surface>& surface, const DeviceOptions& options) {
    return std::make_unique<Device>(surface, Settings{.validation = options.validation, .software = options.software, .lowPower = options.lowPower});
}

Device::Device(const std::optional<Surface>& surface, const Settings& settings) : instance(std::make_unique<Instance>(settings.validation)), chains(makeChains(*instance, surface)), context(std::make_unique<Context>(*instance, chains.front()->swapchain ? chains.front()->swapchain->getSurface() : VK_NULL_HANDLE, Context::Preference{.software = settings.software, .lowPower = settings.lowPower})), releases(*context), layouts(*context), descriptors(*context, releases), renderPasses(*context, releases), staging(*context, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, kStagingChunkSize, kFramesInFlight, "haylen-staging"), uniforms(*context, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, kUniformChunkSize, kFramesInFlight, "haylen-uniforms"), readbacks(*context), prepared(std::make_shared<PreparedModules>(*context)), vk(context->getFunctions()), dynamicRendering(settings.dynamicRendering && context->hasDynamicRendering()) {
    try {
        const VkPipelineCacheCreateInfo cacheInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, .pNext = nullptr, .flags = 0, .initialDataSize = 0, .pInitialData = nullptr};
        Context::check(vk.vkCreatePipelineCache(context->getDevice(), &cacheInfo, nullptr, &pipelineCache), "make its pipeline cache");

        if (dynamicRendering) {
            const bool core = context->getApiVersion() >= VK_API_VERSION_1_3;
            beginRenderingCommand = core ? vk.vkCmdBeginRendering : vk.vkCmdBeginRenderingKHR;
            endRenderingCommand = core ? vk.vkCmdEndRendering : vk.vkCmdEndRenderingKHR;
        }

        labels = instance->hasDebugUtils() && vkCmdBeginDebugUtilsLabelEXT != nullptr;
        createFrames();
        fillCapabilities();
        readDriverIssues();
        swapchainTarget.depth = context->getDepthStencilFormat();

        if (surface) {
            SurfaceChain& window = *chains.front();
            window.swapchain->attach(*context, frames.size());
            window.settings = {.size = surface->size, .sampleCount = surface->sampleCount, .vsync = surface->vsync, .transparent = surface->transparent};
            window.swapchain->configure(window.settings);
            updateSwapchainTarget();
        }
    } catch (...) {
        shutdown();
        throw;
    }

    core::Log::info("The Vulkan device draws with \"{}\" through Vulkan {}.{} with {}.", getAdapterName(), VK_API_VERSION_MAJOR(context->getApiVersion()), VK_API_VERSION_MINOR(context->getApiVersion()), dynamicRendering ? "dynamic rendering" : "render passes");
}

void Device::readDriverIssues() {
    const VkPhysicalDeviceProperties& properties = context->getProperties();
    const DriverIssues::Driver driver{.api = Api::Vulkan, .renderer = properties.deviceName, .versionNumber = properties.driverVersion};
    const std::optional<std::string_view> keptDepth = DriverIssues::find(driver, DriverIssues::Issue::StoreKeptDepth);
    if (keptDepth) {
        core::Log::info("The Vulkan device works around its driver. {}", *keptDepth);
    }

    storeKeptDepth = keptDepth.has_value();

    const std::optional<std::string_view> unboundedAcquire = DriverIssues::find(driver, DriverIssues::Issue::UnboundedAcquire);
    if (unboundedAcquire) {
        core::Log::info("The Vulkan device works around its driver. {}", *unboundedAcquire);
        acquireTimeout = std::numeric_limits<std::uint64_t>::max();
    }
}

Device::~Device() {
    shutdown();
}

std::vector<std::unique_ptr<Device::SurfaceChain>> Device::makeChains(const Instance& owner, const std::optional<Surface>& surface) {
    std::vector<std::unique_ptr<SurfaceChain>> made;
    made.push_back(std::make_unique<SurfaceChain>());
    if (surface) {
        made.front()->swapchain = std::make_unique<Swapchain>(owner.getHandle(), owner.createSurface(*surface));
    }

    return made;
}

void Device::createFrames() {
    const VkDevice device = context->getDevice();
    const VkCommandPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = nullptr, .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, .queueFamilyIndex = context->getQueueFamily()};
    const VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    const VkQueryPoolCreateInfo queryInfo{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .pNext = nullptr, .flags = 0, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2, .pipelineStatistics = 0};
    for (Frame& frame : frames) {
        Context::check(vk.vkCreateCommandPool(device, &poolInfo, nullptr, &frame.pool), "make the command pool of a frame");
        const VkCommandBufferAllocateInfo commandsInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .pNext = nullptr, .commandPool = frame.pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount = 2};
        std::array<VkCommandBuffer, 2> commands{};
        Context::check(vk.vkAllocateCommandBuffers(device, &commandsInfo, commands.data()), "allocate the command buffers of a frame");
        frame.uploads = commands[0];
        frame.graphics = commands[1];
        Context::check(vk.vkCreateFence(device, &fenceInfo, nullptr, &frame.fence), "make the fence of a frame");
        if (context->getTimestampBits() > 0) {
            Context::check(vk.vkCreateQueryPool(device, &queryInfo, nullptr, &frame.timestamps), "make the timestamp queries of a frame");
        }
    }
}

// A format takes the kind of image that `makeImage` makes of it only with every use that `makeImage` gives that kind: a texture is sampled and copied into, a render target is drawn into, sampled, copied into and read back, and a depth stencil image is drawn into, sampled and copied into. The sample counts of a format are the ones the GPU takes for its attachments of that use.
void Device::fillCapabilities() {
    const VkPhysicalDeviceLimits& limits = context->getProperties().limits;
    capabilities.maxImageSize = static_cast<int>(limits.maxImageDimension2D);
    capabilities.maxArrayLayers = static_cast<int>(limits.maxImageArrayLayers);
    capabilities.maxVolumeSize = static_cast<int>(limits.maxImageDimension3D);
    capabilities.maxCubeSize = static_cast<int>(limits.maxImageDimensionCube);
    capabilities.originTopLeft = true;
    capabilities.independentBlend = context->getEnabledFeatures().independentBlend == VK_TRUE;
    capabilities.gpuTiming = context->getTimestampBits() > 0 && limits.timestampPeriod > 0.0F;
    capabilities.maxAnisotropy = context->getEnabledFeatures().samplerAnisotropy == VK_TRUE ? static_cast<int>(limits.maxSamplerAnisotropy) : 1;
    capabilities.maxLodBias = limits.maxSamplerLodBias;
    capabilities.borderColors = true;

    // Every Vulkan device binds storage buffers and reads the arguments of indirect draws, and a uniform block reads at most the range of one uniform buffer descriptor.
    capabilities.storageBuffers = true;
    capabilities.indirectBuffers = true;
    capabilities.maxUniformBlockSize = limits.maxUniformBufferRange;
    for (std::size_t index = 0; index < capabilities.formats.size(); ++index) {
        const auto format = static_cast<PixelFormat>(index);
        const VkFormat vulkanFormat = resolveFormat(format);
        const bool depth = ImageInfo::isDepth(format);
        const bool compressed = ImageInfo::isCompressed(format);
        const VkFormatFeatureFlags sampled = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
        const VkFormatFeatureFlags drawn = depth ? VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | sampled : VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | sampled;
        FormatSupport& support = capabilities.formats[index];
        support.sample = isFeatureEnabled(format) && context->hasFormatFeatures(vulkanFormat, sampled);
        support.render = !compressed && context->hasFormatFeatures(vulkanFormat, drawn);
        support.blend = !depth && !compressed && context->hasFormatFeatures(vulkanFormat, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT);
        support.filter = !depth && support.sample && context->hasFormatFeatures(vulkanFormat, VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT);
        support.storage = !depth && !compressed && context->hasFormatFeatures(vulkanFormat, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
        support.sampleCounts = support.render ? getSampleCounts(vulkanFormat, depth) : std::uint8_t{0};
        support.render = support.sampleCounts != 0;
    }
}

// A block compressed format samples only on a device that was made with the feature of its family.
bool Device::isFeatureEnabled(PixelFormat format) const noexcept {
    const VkPhysicalDeviceFeatures& enabled = context->getEnabledFeatures();
    switch (ImageInfo::getCompression(format)) {
    case ImageInfo::Compression::None:
        return true;
    case ImageInfo::Compression::Bc:
        return enabled.textureCompressionBC == VK_TRUE;
    case ImageInfo::Compression::Etc2:
        return enabled.textureCompressionETC2 == VK_TRUE;
    case ImageInfo::Compression::Astc:
        return enabled.textureCompressionASTC_LDR == VK_TRUE;
    case ImageInfo::Compression::AstcHdr:
        return context->hasAstcHdr();
    }

    return false;
}

// The counts come from the image format properties of an attachment of the format, among the counts of the contract, and a format whose attachments the GPU does not make takes none.
std::uint8_t Device::getSampleCounts(VkFormat format, bool depth) const {
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | (depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    VkImageFormatProperties supported{};
    if (vkGetPhysicalDeviceImageFormatProperties(context->getPhysicalDevice(), format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &supported) != VK_SUCCESS) {
        return 0;
    }

    return static_cast<std::uint8_t>(supported.sampleCounts & kContractSampleCounts);
}

// The device waits for the GPU and destroys what it made in the order its objects need: the ones that frames used, the frames, and the swapchains before the logical device.
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
    buffers.clear();
    images.clear();
    samplers.clear();
    pipelines.clear();
    programs.clear();

    for (const Frame& frame : frames) {
        vk.vkDestroyQueryPool(device, frame.timestamps, nullptr);
        vk.vkDestroyFence(device, frame.fence, nullptr);
        vk.vkDestroyCommandPool(device, frame.pool, nullptr);
    }

    frames = {};
    vk.vkDestroyPipelineCache(device, pipelineCache, nullptr);
    pipelineCache = VK_NULL_HANDLE;
    releases.clear();
    chains.clear();
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

void Device::readPools(std::vector<Pool>& pools) const {
    pools.assign({
        {.name = "images", .used = images.size(), .size = images.capacity()},
        {.name = "buffers", .used = buffers.size(), .size = buffers.capacity()},
        {.name = "samplers", .used = samplers.size(), .size = samplers.capacity()},
        {.name = "programs", .used = programs.size(), .size = programs.capacity()},
        {.name = "pipelines", .used = pipelines.size(), .size = pipelines.capacity()},
        {.name = "descriptorSets", .used = descriptors.getSetCount(), .size = descriptors.getSetCapacity()},
    });
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
    if (std::exchange(frame.trim, false)) {
        staging.trim(static_cast<int>(frameIndex));
        uniforms.trim(static_cast<int>(frameIndex));
        forgetDestroyedUniforms();
    }

    finishTrim();
    staging.begin(static_cast<int>(frameIndex));
    uniforms.begin(static_cast<int>(frameIndex));
    forgetDestroyedUniforms();

    Context::check(vk.vkResetCommandPool(context->getDevice(), frame.pool, 0), "reset the command pool of a frame");
    const VkCommandBufferBeginInfo begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .pNext = nullptr, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, .pInheritanceInfo = nullptr};
    Context::check(vk.vkBeginCommandBuffer(frame.uploads, &begin), "record the uploads of a frame");
    Context::check(vk.vkBeginCommandBuffer(frame.graphics, &begin), "record the passes of a frame");

    if (frame.timestamps != VK_NULL_HANDLE) {
        vk.vkCmdResetQueryPool(frame.uploads, frame.timestamps, 0, 2);
        vk.vkCmdWriteTimestamp(frame.uploads, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, frame.timestamps, 0);
    }

    frame.serial = nextSerial;
    frameOpen = true;
    bufferUploads = false;

    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain != nullptr) {
            chain->drawn = false;
        }
    }

    bound.uniformBuffers = {};
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
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (lost) {
        abandonFrame();
        return;
    }

    if (!frameOpen) {
        pollFrames();
        releases.collect(completedSerial);
        finishTrim();
        readbacks.sweep();
        return;
    }

    endSwapchainRendering();
    Frame& frame = frames[frameIndex];
    if (bufferUploads) {
        const VkMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = kBufferAccesses};
        vk.vkCmdPipelineBarrier(frame.uploads, VK_PIPELINE_STAGE_TRANSFER_BIT, kBufferStages, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    // Every image that the frame acquired goes to its presentation, which waits for the semaphore that the submission signals for it. A scaled swapchain first copies its drawable into the image, so its copy waits for the image instead of its passes.
    imageBarriers.clear();
    waitSemaphores.clear();
    waitStages.clear();
    signalSemaphores.clear();
    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain == nullptr || !chain->acquired) {
            continue;
        }

        waitSemaphores.push_back(chain->swapchain->getAcquired(frameIndex));
        signalSemaphores.push_back(chain->swapchain->getFinished(chain->imageIndex));

        if (chain->swapchain->isScaled()) {
            chain->swapchain->recordScale(frame.graphics, chain->imageIndex);
            waitStages.push_back(VK_PIPELINE_STAGE_TRANSFER_BIT);
            continue;
        }

        waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
        imageBarriers.push_back({.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, .dstAccessMask = 0, .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = chain->swapchain->getImage(chain->imageIndex), .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}});
    }

    if (!imageBarriers.empty()) {
        vk.vkCmdPipelineBarrier(frame.graphics, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
    }

    if (frame.timestamps != VK_NULL_HANDLE) {
        vk.vkCmdWriteTimestamp(frame.graphics, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, frame.timestamps, 1);
    }

    Context::check(vk.vkEndCommandBuffer(frame.uploads), "record the uploads of a frame");
    Context::check(vk.vkEndCommandBuffer(frame.graphics), "record the passes of a frame");
    staging.flush();
    uniforms.flush();

    const std::array<VkCommandBuffer, 2> commands{frame.uploads, frame.graphics};
    const VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = static_cast<std::uint32_t>(waitSemaphores.size()),
        .pWaitSemaphores = waitSemaphores.data(),
        .pWaitDstStageMask = waitStages.data(),
        .commandBufferCount = static_cast<std::uint32_t>(commands.size()),
        .pCommandBuffers = commands.data(),
        .signalSemaphoreCount = static_cast<std::uint32_t>(signalSemaphores.size()),
        .pSignalSemaphores = signalSemaphores.data(),
    };
    frameOpen = false;
    const VkResult submitted = vk.vkQueueSubmit(context->getQueue(), 1, &submit, frame.fence);
    if (submitted == VK_ERROR_DEVICE_LOST) {
        markLost(submitted);
        abandonFrame();
        return;
    }

    Context::check(submitted, "submit a frame");
    frame.submitted = true;

    // Every surface presents, and a present that fails for another reason than the ones that make a swapchain again or lose the device fails the commit once the frame is done.
    VkResult failure = VK_SUCCESS;
    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain == nullptr || !chain->acquired) {
            continue;
        }

        const VkResult presented = chain->swapchain->present(chain->imageIndex);
        if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR || presented == VK_ERROR_SURFACE_LOST_KHR || presented == VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT) {
            chain->dirty = true;
        } else if (presented == VK_ERROR_DEVICE_LOST) {
            markLost(presented);
        } else if (presented != VK_SUCCESS && failure == VK_SUCCESS) {
            failure = presented;
        }

        chain->acquired = false;
    }

    ++nextSerial;
    frameIndex = (frameIndex + 1) % frames.size();
    pollFrames();
    releases.collect(completedSerial);
    descriptors.trim(completedSerial);
    finishTrim();
    readbacks.sweep();
    Context::check(failure, "present the swapchain of a surface");
}

// The frame of a lost device submits nothing, so it lets go of the images it acquired, and the surfaces go and come back without a frame that holds them.
void Device::abandonFrame() noexcept {
    frameOpen = false;
    for (const std::unique_ptr<SurfaceChain>& chain : chains) {
        if (chain != nullptr) {
            chain->acquired = false;
        }
    }
}

// What the GPU finished goes at once, and the rest goes as the frames in flight finish, so a trim never waits for the GPU. Between frames, once every frame and every readback finished, the objects released since then carry the serial of a frame that no GPU work reads yet, so they go at once too.
void Device::trim() {
    if (pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (lost) {
        return;
    }

    pollFrames();
    const bool idle = !frameOpen && std::ranges::none_of(frames, [](const Frame& frame) { return frame.submitted; }) && !readbacks.isCopying();
    releases.collect(idle ? nextSerial : completedSerial);

    for (std::size_t index = 0; index < frames.size(); ++index) {
        Frame& frame = frames[index];
        if (frame.submitted || (frameOpen && index == frameIndex)) {
            frame.trim = true;
            continue;
        }

        staging.trim(static_cast<int>(index));
        uniforms.trim(static_cast<int>(index));
        forgetDestroyedUniforms();
    }

    trimSerial = idle ? completedSerial : nextSerial;
    finishTrim();
}

// The uniform sets name their buffers by their handles, which Vulkan may give to the buffers made after a chunk of the uniform ring went, whose frames finished.
void Device::forgetDestroyedUniforms() noexcept {
    for (const VkBuffer buffer : uniforms.getDestroyed()) {
        descriptors.forgetUniformBuffer(buffer, completedSerial);
    }
}

// The allocator keeps an empty block of each memory type for the next allocation, which the trim gives back once every object released before it and every chunk it let go of left its block.
void Device::finishTrim() {
    if (!trimSerial || completedSerial < *trimSerial || std::ranges::any_of(frames, [](const Frame& frame) { return frame.trim; })) {
        return;
    }

    vmaFreeEmptyBlocks(context->getAllocator());
    trimSerial.reset();
}

// The bytes of the memory blocks that hold no object, with the free room of the blocks that do, and the chunks of the rings that the last frames left empty.
std::size_t Device::getSpareBytes() const noexcept {
    const VkPhysicalDeviceMemoryProperties* memory = nullptr;
    vmaGetMemoryProperties(context->getAllocator(), &memory);
    std::array<VmaBudget, VK_MAX_MEMORY_HEAPS> budgets{};
    vmaGetHeapBudgets(context->getAllocator(), budgets.data());

    VkDeviceSize spare = staging.getSpareBytes() + uniforms.getSpareBytes();
    for (std::uint32_t heap = 0; heap < memory->memoryHeapCount; ++heap) {
        spare += budgets[heap].statistics.blockBytes - budgets[heap].statistics.allocationBytes;
    }

    return static_cast<std::size_t>(spare);
}

void Device::resizeSurface(const Surface::Size& size) {
    SurfaceChain& window = *chains.front();
    if (window.settings.size == size) {
        return;
    }

    window.settings.size = size;
    window.dirty = true;
}

void Device::detachSurface() {
    SurfaceChain& window = *chains.front();
    if (!window.swapchain) {
        return;
    }

    if (window.acquired) {
        throw ContractError(ContractError::Rule::Surface);
    }

    waitIdle();
    forgetViews(*window.swapchain);
    releases.collect(completedSerial);
    window.swapchain.reset();
}

void Device::attachSurface(const Surface& surface) {
    detachSurface();
    auto made = std::make_unique<Swapchain>(instance->getHandle(), instance->createSurface(surface));
    made->attach(*context, frames.size());
    SurfaceChain& window = *chains.front();
    window.swapchain = std::move(made);
    window.settings = {.size = surface.size, .sampleCount = surface.sampleCount, .vsync = surface.vsync, .transparent = surface.transparent};
    window.dirty = true;
}

// A surface beyond the window draws through the pipelines of the swapchain, so its swapchain takes the sample count of the window and must take its color format. It may come between the passes of a frame, whose later swapchain passes draw into it.
int Device::addSurface(const Surface& surface) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::Surface);
    }

    auto chain = std::make_unique<SurfaceChain>();
    chain->swapchain = std::make_unique<Swapchain>(instance->getHandle(), instance->createSurface(surface));
    chain->swapchain->attach(*context, frames.size());

    if (chain->swapchain->getFormat() != swapchainTarget.color) {
        throw std::runtime_error("The surface takes another color format than the swapchain of the window, whose pipelines the Vulkan device draws every surface with.");
    }

    chain->settings = {.size = surface.size, .sampleCount = chains.front()->settings.sampleCount, .vsync = surface.vsync, .transparent = surface.transparent};
    chain->dirty = true;

    const auto free = std::find(chains.begin() + 1, chains.end(), nullptr);
    if (free != chains.end()) {
        *free = std::move(chain);
        return static_cast<int>(free - chains.begin());
    }

    chains.push_back(std::move(chain));
    return static_cast<int>(chains.size()) - 1;
}

void Device::resizeSurface(int number, const Surface::Size& size) {
    SurfaceChain& chain = requireAddedChain(number);
    if (chain.settings.size == size) {
        return;
    }

    chain.settings.size = size;
    chain.dirty = true;
}

// The frames in flight may still draw into the swapchain, so the device waits for them before it destroys it.
void Device::removeSurface(int number) {
    SurfaceChain& chain = requireAddedChain(number);
    if (chain.acquired) {
        throw ContractError(ContractError::Rule::Surface);
    }

    waitIdle();
    forgetViews(*chain.swapchain);
    releases.collect(completedSerial);
    chains[static_cast<std::size_t>(number)].reset();
}

Device::SurfaceChain& Device::requireChain(int surface) const {
    if (surface < 0 || static_cast<std::size_t>(surface) >= chains.size() || chains[static_cast<std::size_t>(surface)] == nullptr) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return *chains[static_cast<std::size_t>(surface)];
}

std::optional<VkPresentModeKHR> Device::getPresentMode(int surface) const {
    const SurfaceChain& chain = requireChain(surface);
    if (!chain.swapchain || !chain.swapchain->hasImages()) {
        return std::nullopt;
    }

    return chain.swapchain->getPresentMode();
}

Device::SurfaceChain& Device::requireAddedChain(int surface) const {
    if (surface == 0) {
        throw ContractError(ContractError::Rule::Surface);
    }

    return requireChain(surface);
}

void Device::forgetViews(const Swapchain& chain) {
    for (VkImageView view : chain.getViews()) {
        renderPasses.forgetView(view, completedSerial);
    }
}

// The frames in flight may still draw into the old swapchain, so the device waits for them before it makes the new one.
void Device::reconfigureSwapchain(SurfaceChain& chain) {
    waitIdle();

    if (lost) {
        return;
    }

    forgetViews(*chain.swapchain);
    releases.collect(completedSerial);
    chain.swapchain->configure(chain.settings);
    chain.dirty = false;
    if (&chain == chains.front().get()) {
        updateSwapchainTarget();
    }
}

void Device::updateSwapchainTarget() {
    const Swapchain& window = *chains.front()->swapchain;
    if (!window.hasImages()) {
        return;
    }

    const SwapchainTarget next{.color = window.getFormat(), .depth = window.getDepthFormat(), .samples = window.getSamples(), .generation = swapchainTarget.generation};
    if (next.color != swapchainTarget.color || next.depth != swapchainTarget.depth || next.samples != swapchainTarget.samples) {
        swapchainTarget = next;
        ++swapchainTarget.generation;
    }
}

// A swapchain that went out of date is made again once, and a surface without pixels or a lost surface draws no frame.
bool Device::acquireSwapchainImage(SurfaceChain& chain) {
    if (chain.acquired) {
        return true;
    }

    if (!chain.swapchain || chain.refusedSerial == nextSerial) {
        return false;
    }

    chain.refusedSerial = nextSerial;

    if (chain.dirty) {
        reconfigureSwapchain(chain);
    }

    if (lost || !chain.swapchain->hasImages()) {
        return false;
    }

    VkResult result = chain.swapchain->acquire(frameIndex, acquireTimeout, chain.imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        reconfigureSwapchain(chain);

        if (lost || !chain.swapchain->hasImages()) {
            return false;
        }

        result = chain.swapchain->acquire(frameIndex, acquireTimeout, chain.imageIndex);
    }

    // A window system that keeps every image, such as the compositor of a hidden window, leaves this frame without an image of the swapchain, and the next frame asks again.
    if (result == VK_TIMEOUT || result == VK_NOT_READY) {
        return false;
    }

    if (result == VK_SUBOPTIMAL_KHR) {
        chain.dirty = true;
    } else if (result == VK_ERROR_DEVICE_LOST) {
        markLost(result);
        return false;
    } else if (result != VK_SUCCESS) {
        chain.dirty = true;
        return false;
    }

    chain.acquired = true;
    chain.refusedSerial = 0;
    return true;
}

VkFormat Device::resolveFormat(PixelFormat format) const noexcept {
    return format == PixelFormat::Depth24Stencil8 ? context->getDepthStencilFormat() : Conversions::toFormat(format);
}

// A stream buffer stays mapped with one copy for each frame in flight. An immutable or dynamic one lives where the GPU reads fastest, and takes its first data directly when the CPU sees that memory and through the staging ring otherwise, while the writes into a dynamic buffer copy from the staging ring among the passes of the frame.
gpu::Device::Buffer Device::makeBuffer(const BufferDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no buffer.");
    }

    const BufferRules::Description description = BufferRules::check(desc, capabilities);
    const bool stream = description.update == BufferUpdate::Stream;
    const VkDeviceSize size = description.size;
    BufferRecord record{.copies = stream ? static_cast<std::uint32_t>(kFramesInFlight) : 1U, .description = description};
    record.stride = stream ? (size + kStreamBufferAlignment - 1) / kStreamBufferAlignment * kStreamBufferAlignment : size;
    VkBufferUsageFlags usage = Conversions::toBufferUsage(description.usage);
    if (!stream) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .pNext = nullptr, .flags = 0, .size = record.stride * record.copies, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE, .queueFamilyIndexCount = 0, .pQueueFamilyIndices = nullptr};
    VmaAllocationCreateInfo memory{.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT, .usage = VMA_MEMORY_USAGE_AUTO, .requiredFlags = 0, .preferredFlags = 0, .memoryTypeBits = 0, .pool = VK_NULL_HANDLE, .pUserData = nullptr, .priority = 0.0F, .minAlignment = 0};
    if (!stream) {
        memory.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT;
    }

    VmaAllocationInfo allocated{};
    ReleaseQueue::Hold room = releases.hold();
    Context::check(vmaCreateBuffer(context->getAllocator(), &info, &memory, &record.buffer, &record.allocation, &allocated), std::format("make a buffer of {} bytes", size));
    room.keep();
    record.mapped = static_cast<std::byte*>(allocated.pMappedData);
    context->setName(VK_OBJECT_TYPE_BUFFER, record.buffer, desc.label);

    try {
        if (!desc.data.empty()) {
            uploadBuffer(record, desc.data);
        }

        return {buffers.add(std::move(record))};
    } catch (...) {
        releases.push(ReleaseQueue::Buffer{.buffer = record.buffer, .allocation = record.allocation}, nextSerial);
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
    bufferUploads = true;
}

// The GPU holds images of a format and a usage only up to the extent, the mip levels and the layers that it reports for them, which may stay below the capabilities of the device for some formats, so an image within the capabilities that the format cannot hold finds no room on the GPU.
void Device::checkImageLimits(const ImageDesc& desc, const VkImageCreateInfo& info, const VkImageFormatProperties& supported) {
    const VkExtent3D& extent = info.extent;
    const VkExtent3D& largest = supported.maxExtent;
    const bool fits = extent.width <= largest.width && extent.height <= largest.height && extent.depth <= largest.depth;
    if (!fits || info.mipLevels > supported.maxMipLevels || info.arrayLayers > supported.maxArrayLayers) {
        throw std::runtime_error(std::format("The GPU of the Vulkan device has no room for the image \"{}\" of {} by {} pixels with {} layers and {} mip levels in its format.", desc.label, desc.width, desc.height, desc.layers, desc.mipLevels));
    }
}

gpu::Device::Image Device::makeImage(const ImageDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no image.");
    }

    ImageRules::check(desc, capabilities);
    const VkFormat format = resolveFormat(desc.format);
    const bool depth = ImageInfo::isDepth(desc.format);
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
        .pNext = nullptr,
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
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    VkImageFormatProperties supported{};
    if (vkGetPhysicalDeviceImageFormatProperties(context->getPhysicalDevice(), format, info.imageType, info.tiling, usage, flags, &supported) != VK_SUCCESS || (supported.sampleCounts & static_cast<VkSampleCountFlags>(samples)) == 0) {
        throw std::runtime_error(std::format("The GPU of the Vulkan device cannot make the image \"{}\" in its format with {} samples.", desc.label, desc.sampleCount));
    }

    checkImageLimits(desc, info, supported);

    ImageRecord record{.format = format, .pixelFormat = desc.format, .kind = desc.kind, .type = desc.type, .width = desc.width, .height = desc.height, .layers = desc.layers, .mipLevels = desc.mipLevels, .samples = samples, .aspects = Conversions::getAspects(format)};
    if (!sampled) {
        record.restLayout = depth ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    const VmaAllocationCreateInfo memory{.flags = 0, .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, .requiredFlags = 0, .preferredFlags = 0, .memoryTypeBits = 0, .pool = VK_NULL_HANDLE, .pUserData = nullptr, .priority = 0.0F, .minAlignment = 0};
    ReleaseQueue::Hold imageRoom = releases.hold();
    Context::check(vmaCreateImage(context->getAllocator(), &info, &memory, &record.image, &record.allocation, nullptr), std::format("make the image \"{}\"", desc.label));
    imageRoom.keep();
    context->setName(VK_OBJECT_TYPE_IMAGE, record.image, desc.label);

    try {
        if (sampled) {
            // A view that shaders sample reads the depth of a depth and stencil image.
            const VkImageAspectFlags aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            const VkImageViewCreateInfo viewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr, .flags = 0, .image = record.image, .viewType = Conversions::toViewType(desc.type), .format = format, .components = {}, .subresourceRange = {.aspectMask = aspect, .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS, .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS}};
            ReleaseQueue::Hold viewRoom = releases.hold();
            Context::check(vk.vkCreateImageView(context->getDevice(), &viewInfo, nullptr, &record.sampleView), "make the view of an image");
            viewRoom.keep();
            context->setName(VK_OBJECT_TYPE_IMAGE_VIEW, record.sampleView, desc.label);
        }

        if (!ensureFrame()) {
            throw std::runtime_error("The Vulkan device stopped working, so it makes no image.");
        }

        if (desc.kind == ImageKind::Texture && !desc.levels.empty()) {
            uploadLevels(record, desc.levels);
        } else {
            clearImage(record);
        }

        return {images.add(std::move(record))};
    } catch (...) {
        releaseImage(record, nextSerial);
        throw;
    }
}

// Each mip level holds every layer of its level, and a volume every slice, packed row after row.
void Device::uploadLevels(const ImageRecord& record, std::span<const std::span<const std::byte>> levels) {
    if (levels.size() > static_cast<std::size_t>(record.mipLevels)) {
        throw ContractError(ContractError::Rule::Image);
    }

    const bool volume = record.type == TextureType::Volume;
    copyRegions.clear();
    for (std::size_t level = 0; level < levels.size(); ++level) {
        const auto width = static_cast<std::uint32_t>(std::max(record.width >> level, 1));
        const auto height = static_cast<std::uint32_t>(std::max(record.height >> level, 1));
        const auto depth = volume ? static_cast<std::uint32_t>(std::max(record.layers >> level, 1)) : 1U;
        const auto layers = volume ? 1U : static_cast<std::uint32_t>(record.layers);
        const std::size_t bytes = ImageInfo::getRegionBytes(record.pixelFormat, static_cast<int>(width), static_cast<int>(height)) * depth * layers;
        if (levels[level].size() < bytes) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        const UploadRing::Allocation source = staging.allocate(bytes, 16);
        std::memcpy(source.data, levels[level].data(), bytes);
        copyRegions.push_back({.bufferOffset = source.offset, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = {.aspectMask = record.aspects, .mipLevel = static_cast<std::uint32_t>(level), .baseArrayLayer = 0, .layerCount = layers}, .imageOffset = {}, .imageExtent = {width, height, depth}});

        // The ring may move to another chunk between levels, so each level copies from its own buffer.
        const VkCommandBuffer commands = frames[frameIndex].uploads;
        if (level == 0) {
            const VkImageMemoryBarrier toDestination{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = record.image, .subresourceRange = {.aspectMask = record.aspects, .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS, .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS}};
            vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDestination);
        }

        vk.vkCmdCopyBufferToImage(commands, source.buffer, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegions.back());
    }

    // Mip levels without pixels start cleared.
    const VkCommandBuffer commands = frames[frameIndex].uploads;
    if (levels.size() < static_cast<std::size_t>(record.mipLevels)) {
        const VkClearColorValue clear{};
        const VkImageSubresourceRange rest{.aspectMask = record.aspects, .baseMipLevel = static_cast<std::uint32_t>(levels.size()), .levelCount = VK_REMAINING_MIP_LEVELS, .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS};
        vk.vkCmdClearColorImage(commands, record.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &rest);
    }

    const VkImageSubresourceRange range{.aspectMask = record.aspects, .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS, .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS};
    const VkImageMemoryBarrier toRest = ImageLayouts::makeBarrier(record.image, range, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, record.restLayout);
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, ImageLayouts::getStages(record.restLayout), 0, 0, nullptr, 0, nullptr, 1, &toRest);
}

// Images without pixels start transparent, and depth and stencil images at the far depth and a stencil of 0, so every device starts them alike.
void Device::clearImage(const ImageRecord& record) {
    const VkCommandBuffer commands = frames[frameIndex].uploads;
    const VkImageSubresourceRange range{.aspectMask = record.aspects, .baseMipLevel = 0, .levelCount = VK_REMAINING_MIP_LEVELS, .baseArrayLayer = 0, .layerCount = VK_REMAINING_ARRAY_LAYERS};
    const VkImageMemoryBarrier toDestination{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED, .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .image = record.image, .subresourceRange = range};
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

    SamplerRules::check(desc, capabilities);
    const bool anisotropic = desc.maxAnisotropy > 1;
    const VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = Conversions::toFilter(desc.magnification),
        .minFilter = Conversions::toFilter(desc.minification),
        .mipmapMode = Conversions::toMipmapMode(desc.mipmap),
        .addressModeU = Conversions::toAddressMode(desc.wrapU),
        .addressModeV = Conversions::toAddressMode(desc.wrapV),
        .addressModeW = Conversions::toAddressMode(desc.wrapW),
        .mipLodBias = desc.lodBias,
        .anisotropyEnable = anisotropic ? VK_TRUE : VK_FALSE,
        .maxAnisotropy = static_cast<float>(desc.maxAnisotropy),
        .compareEnable = desc.compare ? VK_TRUE : VK_FALSE,
        .compareOp = desc.compare ? Conversions::toCompare(*desc.compare) : VK_COMPARE_OP_ALWAYS,
        .minLod = desc.minLod,
        .maxLod = desc.maxLod,
        .borderColor = Conversions::toBorderColor(desc.borderColor),
        .unnormalizedCoordinates = VK_FALSE,
    };
    SamplerRecord record;
    ReleaseQueue::Hold room = releases.hold();
    Context::check(vk.vkCreateSampler(context->getDevice(), &info, nullptr, &record.sampler), "make a sampler");
    context->setName(VK_OBJECT_TYPE_SAMPLER, record.sampler, desc.label);
    try {
        const Sampler made{samplers.add(std::move(record))};
        room.keep();
        return made;
    } catch (...) {
        vk.vkDestroySampler(context->getDevice(), record.sampler, nullptr);
        throw;
    }
}

// The blocks of a program take one set of dynamic uniform buffers, and its textures and samplers another, at the sets and bindings of the SPIR-V of its package. The layouts that a program skips are empty.
gpu::Device::Program Device::makeProgram(const ProgramDesc& desc) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it makes no program.");
    }

    ProgramSlots::check(desc);

    for (const StageCode* stage : {&desc.vertex, &desc.fragment}) {
        if (stage->bytecode.empty() || stage->bytecode.size() % 4 != 0) {
            throw ContractError(ContractError::Rule::Program);
        }
    }

    ProgramRecord record;
    record.label = desc.label;
    record.vertexEntry = desc.vertex.entry.empty() ? "main" : std::string(desc.vertex.entry);
    record.fragmentEntry = desc.fragment.entry.empty() ? "main" : std::string(desc.fragment.entry);
    record.blockIndices.fill(-1);

    std::vector<LayoutCache::Binding> uniformBindings;
    for (const BlockBinding& block : desc.blocks) {
        if (block.size > capabilities.maxUniformBlockSize) {
            throw ContractError(ContractError::Rule::Program);
        }

        if (record.uniformSet != kNoSet && record.uniformSet != static_cast<std::uint32_t>(block.set)) {
            throw ContractError(ContractError::Rule::Program);
        }

        record.uniformSet = static_cast<std::uint32_t>(block.set);
        uniformBindings.push_back({.binding = static_cast<std::uint32_t>(block.binding), .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .stages = Conversions::toStage(block.stage)});
        record.uniformBlocks.push_back({.binding = static_cast<std::uint32_t>(block.binding), .size = block.size});
    }

    std::ranges::sort(uniformBindings);
    std::ranges::sort(record.uniformBlocks, {}, &DescriptorCache::UniformBlock::binding);
    for (const BlockBinding& block : desc.blocks) {
        const auto found = std::ranges::find(record.uniformBlocks, static_cast<std::uint32_t>(block.binding), &DescriptorCache::UniformBlock::binding);
        record.blockIndices[static_cast<std::size_t>(block.slot)] = static_cast<int>(found - record.uniformBlocks.begin());
    }

    std::vector<LayoutCache::Binding> resourceBindings;
    // clang-format off
    const auto addResource = [&](int set, int binding, VkDescriptorType type, Stage stage) {
        if (record.resourceSet != kNoSet && record.resourceSet != static_cast<std::uint32_t>(set)) {
            throw ContractError(ContractError::Rule::Program);
        }

        record.resourceSet = static_cast<std::uint32_t>(set);
        resourceBindings.push_back({.binding = static_cast<std::uint32_t>(binding), .type = type, .stages = Conversions::toStage(stage)});
    };
    // clang-format on
    for (const TextureBinding& texture : desc.textures) {
        addResource(texture.set, texture.binding, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, texture.stage);
        record.textures.push_back({.slot = texture.slot, .binding = static_cast<std::uint32_t>(texture.binding)});
    }

    for (const SamplerBinding& sampler : desc.samplers) {
        addResource(sampler.set, sampler.binding, VK_DESCRIPTOR_TYPE_SAMPLER, sampler.stage);
        record.samplers.push_back({.slot = sampler.slot, .binding = static_cast<std::uint32_t>(sampler.binding)});
    }

    std::ranges::sort(resourceBindings);

    if (record.uniformSet != kNoSet && record.uniformSet == record.resourceSet) {
        throw ContractError(ContractError::Rule::Program);
    }

    if (record.uniformSet != kNoSet) {
        record.uniformLayout = layouts.getSetLayout(uniformBindings);
    }

    if (record.resourceSet != kNoSet) {
        record.resourceLayout = layouts.getSetLayout(resourceBindings);
    }

    const std::uint32_t setCount = std::max(record.uniformSet == kNoSet ? 0U : record.uniformSet + 1, record.resourceSet == kNoSet ? 0U : record.resourceSet + 1);
    std::vector<VkDescriptorSetLayout> sets(setCount, layouts.getSetLayout({}));
    if (record.uniformSet != kNoSet) {
        sets[record.uniformSet] = record.uniformLayout;
    }

    if (record.resourceSet != kNoSet) {
        sets[record.resourceSet] = record.resourceLayout;
    }

    record.layout = layouts.getPipelineLayout(sets);

    // The bytes of a module may lie anywhere, while Vulkan reads it as aligned words.
    // clang-format off
    const auto makeModule = [&](std::span<const std::byte> bytecode) {
        std::vector<std::uint32_t> words(bytecode.size() / 4);
        std::memcpy(words.data(), bytecode.data(), bytecode.size());
        VkShaderModule module = prepared->take(words);
        if (module != VK_NULL_HANDLE) {
            return module;
        }

        const VkShaderModuleCreateInfo info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr, .flags = 0, .codeSize = bytecode.size(), .pCode = words.data()};
        Context::check(vk.vkCreateShaderModule(context->getDevice(), &info, nullptr, &module), std::format("make the shader modules of the program \"{}\"", desc.label));
        context->setName(VK_OBJECT_TYPE_SHADER_MODULE, module, desc.label);
        return module;
    };
    // clang-format on
    ReleaseQueue::Hold vertexRoom = releases.hold();
    ReleaseQueue::Hold fragmentRoom = releases.hold();
    record.vertex = makeModule(desc.vertex.bytecode);
    try {
        record.fragment = makeModule(desc.fragment.bytecode);
        const Program made{programs.add(std::move(record))};
        vertexRoom.keep();
        fragmentRoom.keep();
        return made;
    } catch (...) {
        vk.vkDestroyShaderModule(context->getDevice(), record.vertex, nullptr);
        vk.vkDestroyShaderModule(context->getDevice(), record.fragment, nullptr);
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
        throw ContractError(ContractError::Rule::MissingObject);
    }

    if (desc.colors.size() > kMaxColorTargets || desc.buffers.size() > kMaxVertexBuffers || desc.attributes.size() > kMaxAttributes) {
        throw ContractError(ContractError::Rule::Pipeline);
    }

    PipelineRecord record{.program = desc.program.id, .layout = program->layout, .indexType = desc.indexType, .bufferCount = static_cast<std::uint32_t>(desc.buffers.size()), .generation = swapchainTarget.generation};
    if (desc.swapchain) {
        record.stored = storePipeline(desc);
    }

    ReleaseQueue::Hold room = releases.hold();
    record.pipeline = buildPipeline(desc, *program);
    try {
        const std::uint64_t id = pipelines.add(std::move(record));
        ++program->pipelines;
        room.keep();
        return {id};
    } catch (...) {
        vk.vkDestroyPipeline(context->getDevice(), record.pipeline, nullptr);
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
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = program.vertex, .pName = program.vertexEntry.c_str(), .pSpecializationInfo = nullptr},
        {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr, .flags = 0, .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = program.fragment, .pName = program.fragmentEntry.c_str(), .pSpecializationInfo = nullptr},
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

    const VkPipelineVertexInputStateCreateInfo vertexInput{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .vertexBindingDescriptionCount = static_cast<std::uint32_t>(desc.buffers.size()), .pVertexBindingDescriptions = bindings.data(), .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(desc.attributes.size()), .pVertexAttributeDescriptions = attributes.data()};

    // Strips restart at the largest index, as on Metal and Direct3D.
    const bool strip = desc.primitive == Primitive::TriangleStrip || desc.primitive == Primitive::LineStrip;
    const VkPipelineInputAssemblyStateCreateInfo inputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .topology = Conversions::toTopology(desc.primitive), .primitiveRestartEnable = strip && desc.indexType != IndexType::None ? VK_TRUE : VK_FALSE};
    const VkPipelineViewportStateCreateInfo viewport{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .viewportCount = 1, .pViewports = nullptr, .scissorCount = 1, .pScissors = nullptr};
    const bool biased = desc.depthStencil.depthBias != 0.0F || desc.depthStencil.depthBiasSlope != 0.0F;
    const VkPipelineRasterizationStateCreateInfo rasterization{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .depthClampEnable = VK_FALSE, .rasterizerDiscardEnable = VK_FALSE, .polygonMode = VK_POLYGON_MODE_FILL, .cullMode = Conversions::toCullMode(desc.cullMode), .frontFace = Conversions::toFrontFace(desc.frontFace), .depthBiasEnable = biased ? VK_TRUE : VK_FALSE, .depthBiasConstantFactor = desc.depthStencil.depthBias, .depthBiasClamp = 0.0F, .depthBiasSlopeFactor = desc.depthStencil.depthBiasSlope, .lineWidth = 1.0F};

    const VkSampleCountFlagBits samples = desc.swapchain ? swapchainTarget.samples : Conversions::toSampleCount(desc.sampleCount);
    const VkPipelineMultisampleStateCreateInfo multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .rasterizationSamples = samples, .sampleShadingEnable = VK_FALSE, .minSampleShading = 0.0F, .pSampleMask = nullptr, .alphaToCoverageEnable = desc.alphaToCoverage ? VK_TRUE : VK_FALSE, .alphaToOneEnable = VK_FALSE};

    const DepthStencil& depth = desc.depthStencil;
    const VkFormat depthFormat = desc.swapchain ? swapchainTarget.depth : (depth.format ? resolveFormat(*depth.format) : VK_FORMAT_UNDEFINED);
    // clang-format off
    const auto toStencilFace = [&depth](const StencilFace& face) {
        return VkStencilOpState{.failOp = Conversions::toStencilOperation(face.fail), .passOp = Conversions::toStencilOperation(face.pass), .depthFailOp = Conversions::toStencilOperation(face.depthFail), .compareOp = Conversions::toCompare(face.compare), .compareMask = depth.stencilReadMask, .writeMask = depth.stencilWriteMask, .reference = 0};
    };
    // clang-format on
    const bool depthTested = depthFormat != VK_FORMAT_UNDEFINED && (depth.depthCompare != CompareFunction::Always || depth.depthWrite);
    const VkPipelineDepthStencilStateCreateInfo depthStencil{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .depthTestEnable = depthTested ? VK_TRUE : VK_FALSE, .depthWriteEnable = depthTested && depth.depthWrite ? VK_TRUE : VK_FALSE, .depthCompareOp = Conversions::toCompare(depth.depthCompare), .depthBoundsTestEnable = VK_FALSE, .stencilTestEnable = depthFormat != VK_FORMAT_UNDEFINED && depth.stencilEnabled ? VK_TRUE : VK_FALSE, .front = toStencilFace(depth.stencilFront), .back = toStencilFace(depth.stencilBack), .minDepthBounds = 0.0F, .maxDepthBounds = 1.0F};

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

    const VkPipelineColorBlendStateCreateInfo colorBlend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .logicOpEnable = VK_FALSE, .logicOp = VK_LOGIC_OP_CLEAR, .attachmentCount = colorCount, .pAttachments = blends.data(), .blendConstants = {}};

    const std::array<VkDynamicState, 4> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_BLEND_CONSTANTS, VK_DYNAMIC_STATE_STENCIL_REFERENCE};
    const VkPipelineDynamicStateCreateInfo dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .pNext = nullptr, .flags = 0, .dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

    const VkPipelineRenderingCreateInfo rendering{.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO, .pNext = nullptr, .viewMask = 0, .colorAttachmentCount = colorCount, .pColorAttachmentFormats = colorFormats.data(), .depthAttachmentFormat = depthFormat, .stencilAttachmentFormat = Conversions::hasStencil(depthFormat) ? depthFormat : VK_FORMAT_UNDEFINED};
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
        .flags = 0,
        .stageCount = static_cast<std::uint32_t>(stages.size()),
        .pStages = stages.data(),
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pTessellationState = nullptr,
        .pViewportState = &viewport,
        .pRasterizationState = &rasterization,
        .pMultisampleState = &multisample,
        .pDepthStencilState = &depthStencil,
        .pColorBlendState = &colorBlend,
        .pDynamicState = &dynamic,
        .layout = program.layout,
        .renderPass = compatible,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };
    VkPipeline pipeline = VK_NULL_HANDLE;
    Context::check(vk.vkCreateGraphicsPipelines(context->getDevice(), pipelineCache, 1, &info, nullptr, &pipeline), std::format("make the pipeline \"{}\"", desc.label));
    context->setName(VK_OBJECT_TYPE_PIPELINE, pipeline, desc.label);
    return pipeline;
}

// The frames in flight may still draw with the old pipeline, which waits for them.
void Device::rebuildPipeline(PipelineRecord& record) {
    const ProgramRecord& program = *programs.find(record.program);
    ReleaseQueue::Hold room = releases.hold();
    const VkPipeline rebuilt = buildPipeline(record.stored->desc, program);
    room.keep();
    releases.push(ReleaseQueue::Pipeline{.pipeline = record.pipeline}, nextSerial);
    record.pipeline = rebuilt;
    record.generation = swapchainTarget.generation;
}

void Device::destroy(Buffer buffer) noexcept {
    std::optional<BufferRecord> record = buffers.remove(buffer.id);
    if (!record) {
        return;
    }

    if (record->description.usage.uniform) {
        descriptors.forgetUniformBuffer(record->buffer, nextSerial);
    }

    releases.push(ReleaseQueue::Buffer{.buffer = record->buffer, .allocation = record->allocation}, nextSerial);
}

void Device::destroy(Image image) noexcept {
    std::optional<ImageRecord> record = images.remove(image.id);
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
    std::optional<SamplerRecord> record = samplers.remove(sampler.id);
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

void Device::forgetProgramUse(std::uint64_t program, std::uint64_t serial) noexcept {
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
    PipelineRecord* found = pipelines.find(pipeline.id);
    if (found == nullptr) {
        return;
    }

    if (bound.pipeline == found) {
        bound.pipeline = nullptr;
        bound.program = nullptr;
        bound.uniformSetProgram = nullptr;
        bound.uniformSet = VK_NULL_HANDLE;
    }

    const std::optional<PipelineRecord> record = pipelines.remove(pipeline.id);
    releases.push(ReleaseQueue::Pipeline{.pipeline = record->pipeline}, nextSerial);
    forgetProgramUse(record->program, nextSerial);
}

// A dynamic buffer takes the bytes through the staging ring, in a copy among the passes of the frame.
void Device::writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    BufferRecord& record = buffers.get(buffer.id);
    BufferRules::checkWrite(record.description, offset, data.size());
    if (data.empty() || !ensureFrame()) {
        return;
    }

    if (record.description.update == BufferUpdate::Stream) {
        writeStream(record, offset, data);
        return;
    }

    const UploadRing::Allocation source = staging.allocate(data.size(), 16);
    std::memcpy(source.data, data.data(), data.size());
    recordBufferCopy(source.buffer, source.offset, record.buffer, offset, data.size());
}

// The first write of a frame moves a stream buffer to its next copy, which the GPU finished reading once the frame waited for the fence of the frame before the last, since a frame in flight reads every other copy at most.
void Device::writeStream(BufferRecord& record, std::size_t offset, std::span<const std::byte> data) {
    if (record.writtenSerial != nextSerial) {
        record.writtenSerial = nextSerial;
        record.current = (record.current + 1) % record.copies;
    }

    const VkDeviceSize start = record.stride * record.current + offset;
    std::memcpy(record.mapped + start, data.data(), data.size());
    vmaFlushAllocation(context->getAllocator(), record.allocation, start, data.size());
}

void Device::copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::BufferWrite);
    }

    const BufferRecord& from = buffers.get(source.id);
    const BufferRecord& to = buffers.get(destination.id);
    BufferRules::checkCopy(source, from.description, sourceOffset, destination, to.description, destinationOffset, size);
    if (!ensureFrame()) {
        return;
    }

    recordBufferCopy(from.buffer, sourceOffset, to.buffer, destinationOffset, size);
}

// A copy into a buffer runs outside rendering, so it ends the rendering that a swapchain pass left open. It waits for the commands before it that read or write the range it writes, and the commands after it read what it wrote.
void Device::recordBufferCopy(VkBuffer source, VkDeviceSize sourceOffset, VkBuffer destination, VkDeviceSize destinationOffset, VkDeviceSize size) {
    endSwapchainRendering();
    const VkCommandBuffer commands = frames[frameIndex].graphics;
    VkBufferMemoryBarrier barrier{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, .pNext = nullptr, .srcAccessMask = 0, .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .buffer = destination, .offset = destinationOffset, .size = size};
    vk.vkCmdPipelineBarrier(commands, kBufferStages, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

    const VkBufferCopy region{.srcOffset = sourceOffset, .dstOffset = destinationOffset, .size = size};
    vk.vkCmdCopyBuffer(commands, source, destination, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = kBufferAccesses;
    vk.vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, kBufferStages, 0, 0, nullptr, 1, &barrier, 0, nullptr);
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
        const ImageRecord& record = images.get(write.image.id);
        if (record.kind != ImageKind::Dynamic) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        const Region& region = write.region;
        const bool inside = region.x >= 0 && region.y >= 0 && region.width > 0 && region.height > 0 && region.x + region.width <= record.width && region.y + region.height <= record.height;
        const std::size_t bytes = ImageInfo::getRegionBytes(record.pixelFormat, region.width, region.height);
        if (!inside || write.pixels.size() != bytes) {
            throw ContractError(ContractError::Rule::ImageWrite);
        }

        // The copy keeps the pixels outside the region, so the image leaves its resting layout with its contents.
        const UploadRing::Allocation source = staging.allocate(bytes, 16);
        std::memcpy(source.data, write.pixels.data(), bytes);
        sourceBuffers.push_back(source.buffer);
        const VkImageSubresourceRange range{.aspectMask = record.aspects, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
        copyRegions.push_back({.bufferOffset = source.offset, .bufferRowLength = 0, .bufferImageHeight = 0, .imageSubresource = {.aspectMask = record.aspects, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1}, .imageOffset = {region.x, region.y, 0}, .imageExtent = {static_cast<std::uint32_t>(region.width), static_cast<std::uint32_t>(region.height), 1U}});
        imageBarriers.push_back(ImageLayouts::makeBarrier(record.image, range, record.restLayout, record.restLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL));
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

// A swapchain pass that keeps the contents of the swapchain pass right before it goes on drawing in the same rendering, so its depth, stencil and multisampled color stay. Labels name the commands inside the rendering of their pass. The pass opens once its rendering began, so a pass that fails to begin leaves no pass open.
void Device::beginPass(const PassDesc& desc) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    pass.skipped = false;
    pass.labeled = false;
    pass.transitionCount = 0;
    if (!ensureFrame()) {
        pass.skipped = true;
        pass.open = true;
        return;
    }

    resetDrawState();

    const bool toSwapchain = desc.colors.empty() && !desc.depthStencil;
    pass.swapchain = toSwapchain;
    const bool continues = toSwapchain && swapchainRendering && renderingSurface == desc.surface && desc.load == Load::Keep;
    if (!continues) {
        endSwapchainRendering();
    }

    if (toSwapchain && !continues) {
        beginSwapchainPass(desc);
    } else if (!toSwapchain) {
        pass.turns = 0;
        beginOffscreenPass(desc);
    }

    swapchainRendering = false;
    pass.open = true;
    if (pass.skipped) {
        return;
    }

    const VkCommandBuffer commands = frames[frameIndex].graphics;
    if (labels && !desc.label.empty()) {
        labelText.assign(desc.label);
        const VkDebugUtilsLabelEXT label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pNext = nullptr, .pLabelName = labelText.c_str(), .color = {}};
        vkCmdBeginDebugUtilsLabelEXT(commands, &label);
        pass.labeled = true;
    }

    const VkViewport viewport{.x = 0.0F, .y = static_cast<float>(pass.extent.height), .width = static_cast<float>(pass.extent.width), .height = -static_cast<float>(pass.extent.height), .minDepth = 0.0F, .maxDepth = 1.0F};
    const VkRect2D scissor{.offset = {0, 0}, .extent = pass.extent};
    const std::array<float, 4> blendColor{};
    vk.vkCmdSetViewport(commands, 0, 1, &viewport);
    vk.vkCmdSetScissor(commands, 0, 1, &scissor);
    vk.vkCmdSetBlendConstants(commands, blendColor.data());
    vk.vkCmdSetStencilReference(commands, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
}

// The depth and stencil and the multisampled color of the swapchain last while swapchain passes that keep them follow each other, while its image keeps what the passes of the frame drew until it is presented.
void Device::beginSwapchainPass(const PassDesc& desc) {
    SurfaceChain& chain = requireChain(desc.surface);
    if (!acquireSwapchainImage(chain)) {
        pass.skipped = true;
        return;
    }

    const Swapchain& swapchain = *chain.swapchain;
    pass.turns = swapchain.getTurns();
    pass.window = swapchain.getWindowExtent();
    const std::uint32_t imageIndex = chain.imageIndex;
    const VkCommandBuffer commands = frames[frameIndex].graphics;
    const bool multisampled = swapchain.getSamples() != VK_SAMPLE_COUNT_1_BIT;
    const VkAttachmentLoadOp load = Conversions::toLoad(desc.load);
    const VkImageLayout previous = chain.drawn && desc.load == Load::Keep ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;

    // The image of the swapchain keeps what earlier passes of the frame drew when the pass keeps it, while the attachments that last for one pass start anew.
    imageBarriers.clear();
    const VkImageSubresourceRange colorRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1};
    const VkImageLayout colorLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkImageLayout depthLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    imageBarriers.push_back(ImageLayouts::makeBarrier(swapchain.getImage(imageIndex), colorRange, chain.drawn ? colorLayout : VK_IMAGE_LAYOUT_UNDEFINED, previous, colorLayout));
    if (multisampled) {
        imageBarriers.push_back(ImageLayouts::makeBarrier(swapchain.getColorImage(), colorRange, colorLayout, VK_IMAGE_LAYOUT_UNDEFINED, colorLayout));
    }

    imageBarriers.push_back(ImageLayouts::makeBarrier(swapchain.getDepthImage(), {.aspectMask = Conversions::getAspects(swapchain.getDepthFormat()), .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}, depthLayout, VK_IMAGE_LAYOUT_UNDEFINED, depthLayout));
    const VkPipelineStageFlags stages = ImageLayouts::getStages(colorLayout) | ImageLayouts::getStages(depthLayout);
    vk.vkCmdPipelineBarrier(commands, stages, stages, 0, 0, nullptr, 0, nullptr, static_cast<std::uint32_t>(imageBarriers.size()), imageBarriers.data());
    chain.drawn = true;
    renderingSurface = desc.surface;

    PassTarget target{.colorCount = 1, .hasDepth = true, .extent = swapchain.getExtent()};
    AttachmentTarget& color = target.colors[0];
    color = {.view = multisampled ? swapchain.getColorView() : swapchain.getView(imageIndex), .format = swapchain.getFormat(), .samples = swapchain.getSamples(), .load = load, .store = multisampled ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE};
    color.clear.color = {{desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a}};
    if (multisampled) {
        color.resolveView = swapchain.getView(imageIndex);
        color.resolveFormat = swapchain.getFormat();
    }

    target.depth = {.view = swapchain.getDepthView(), .format = swapchain.getDepthFormat(), .samples = swapchain.getSamples(), .load = load, .store = VK_ATTACHMENT_STORE_OP_DONT_CARE, .stencilLoad = load, .stencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE};
    target.depth.clear.depthStencil = {.depth = desc.clearDepth, .stencil = desc.clearStencil};
    beginRendering(target);
}

void Device::beginOffscreenPass(const PassDesc& desc) {
    if (desc.colors.size() > kMaxColorTargets) {
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    PassTarget target{.colorCount = static_cast<std::uint32_t>(desc.colors.size())};
    for (std::size_t index = 0; index < desc.colors.size(); ++index) {
        const ColorAttachment& attachment = desc.colors[index];
        ImageRecord& record = images.get(attachment.image.id);
        if (record.kind != ImageKind::RenderTarget) {
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        AttachmentTarget& color = target.colors[index];
        color = {.view = getAttachmentView(record, attachment.mipLevel, attachment.layer), .format = record.format, .samples = record.samples, .load = Conversions::toLoad(attachment.load), .store = Conversions::toStore(attachment.store)};
        color.clear.color = {{attachment.clear.r, attachment.clear.g, attachment.clear.b, attachment.clear.a}};
        addTransition(record, attachment.mipLevel, attachment.layer, attachment.load == Load::Keep);

        if (index == 0) {
            target.extent = {static_cast<std::uint32_t>(std::max(record.width >> attachment.mipLevel, 1)), static_cast<std::uint32_t>(std::max(record.height >> attachment.mipLevel, 1))};
        }

        if (attachment.resolve.isValid()) {
            ImageRecord& resolve = images.get(attachment.resolve.id);
            color.resolveView = getAttachmentView(resolve, 0, 0);
            color.resolveFormat = resolve.format;
            addTransition(resolve, 0, 0, false);
        }
    }

    if (desc.depthStencil) {
        const DepthStencilAttachment& attachment = *desc.depthStencil;
        ImageRecord& record = images.get(attachment.image.id);
        if (record.kind != ImageKind::DepthStencil) {
            throw ContractError(ContractError::Rule::PassAttachments);
        }

        target.hasDepth = true;
        target.depth = {.view = getAttachmentView(record, 0, 0), .format = record.format, .samples = record.samples, .load = Conversions::toLoad(attachment.depthLoad), .store = Conversions::toStore(attachment.depthStore), .stencilLoad = Conversions::toLoad(attachment.stencilLoad), .stencilStore = Conversions::toStore(attachment.stencilStore)};
        target.depth.clear.depthStencil = {.depth = attachment.clearDepth, .stencil = attachment.clearStencil};
        if (storeKeptDepth && attachment.depthLoad == Load::Keep && !Conversions::hasStencil(record.format)) {
            target.depth.store = VK_ATTACHMENT_STORE_OP_STORE;
            target.depth.stencilLoad = VK_ATTACHMENT_LOAD_OP_LOAD;
            target.depth.stencilStore = VK_ATTACHMENT_STORE_OP_STORE;
        }

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
        throw ContractError(ContractError::Rule::PassAttachments);
    }

    const VkImageViewCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr, .flags = 0, .image = record.image, .viewType = VK_IMAGE_VIEW_TYPE_2D, .format = record.format, .components = {}, .subresourceRange = {.aspectMask = record.aspects, .baseMipLevel = static_cast<std::uint32_t>(mipLevel), .levelCount = 1, .baseArrayLayer = static_cast<std::uint32_t>(layer), .layerCount = 1}};
    VkImageView view = VK_NULL_HANDLE;
    record.attachmentViews.reserve(record.attachmentViews.size() + 1);
    ReleaseQueue::Hold room = releases.hold();
    Context::check(vk.vkCreateImageView(context->getDevice(), &info, nullptr, &view), "make the view of an attachment");
    room.keep();
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
            colors[index] = {.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr, .imageView = color.view, .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .resolveMode = color.resolveView != VK_NULL_HANDLE ? VK_RESOLVE_MODE_AVERAGE_BIT : VK_RESOLVE_MODE_NONE, .resolveImageView = color.resolveView, .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .loadOp = color.load, .storeOp = color.store, .clearValue = color.clear};
        }

        const AttachmentTarget& depth = target.depth;
        const VkRenderingAttachmentInfo depthInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr, .imageView = depth.view, .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .resolveMode = VK_RESOLVE_MODE_NONE, .resolveImageView = VK_NULL_HANDLE, .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED, .loadOp = depth.load, .storeOp = depth.store, .clearValue = depth.clear};
        const VkRenderingAttachmentInfo stencilInfo{.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO, .pNext = nullptr, .imageView = depth.view, .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .resolveMode = VK_RESOLVE_MODE_NONE, .resolveImageView = VK_NULL_HANDLE, .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED, .loadOp = depth.stencilLoad, .storeOp = depth.stencilStore, .clearValue = depth.clear};
        const bool stencil = target.hasDepth && Conversions::hasStencil(depth.format);
        const VkRenderingInfo info{.sType = VK_STRUCTURE_TYPE_RENDERING_INFO, .pNext = nullptr, .flags = 0, .renderArea = area, .layerCount = 1, .viewMask = 0, .colorAttachmentCount = target.colorCount, .pColorAttachments = colors.data(), .pDepthAttachment = target.hasDepth ? &depthInfo : nullptr, .pStencilAttachment = stencil ? &stencilInfo : nullptr};
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
    const VkRenderPassBeginInfo info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, .pNext = nullptr, .renderPass = renderPass, .framebuffer = framebuffer, .renderArea = area, .clearValueCount = count, .pClearValues = clears.data()};
    vk.vkCmdBeginRenderPass(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
}

// The attachments go back to their resting layouts, where later passes sample them and readbacks copy them.
// The rendering of a swapchain pass stays open until another pass, a debug group outside passes or the commit, so a swapchain pass that keeps it goes on in it.
void Device::endPass() {
    if (!pass.open) {
        throw ContractError(ContractError::Rule::PassOrder);
    }

    pass.open = false;
    if (pass.skipped) {
        return;
    }

    const VkCommandBuffer commands = frames[frameIndex].graphics;
    if (pass.labeled) {
        vkCmdEndDebugUtilsLabelEXT(commands);
    }

    if (pass.swapchain) {
        swapchainRendering = true;
        return;
    }

    endRendering();

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

void Device::endRendering() {
    const VkCommandBuffer commands = frames[frameIndex].graphics;
    if (dynamicRendering) {
        endRenderingCommand(commands);
    } else {
        vk.vkCmdEndRenderPass(commands);
    }
}

void Device::endSwapchainRendering() {
    if (swapchainRendering) {
        endRendering();
        swapchainRendering = false;
    }
}

void Device::applyViewport(const math::Rect& window) {
    if (!pass.open || pass.skipped) {
        return;
    }

    const math::Rect pixels = getPassPixels(window);
    const VkViewport viewport{.x = pixels.x, .y = pixels.y + pixels.height, .width = pixels.width, .height = -pixels.height, .minDepth = 0.0F, .maxDepth = 1.0F};
    vk.vkCmdSetViewport(frames[frameIndex].graphics, 0, 1, &viewport);
}

// Vulkan takes a scissor inside the attachments with a positive origin, so the rectangle is rounded to pixels and clipped to them.
void Device::applyScissor(const math::Rect& window) {
    if (!pass.open || pass.skipped) {
        return;
    }

    const math::Rect pixels = getPassPixels(window);
    const auto width = static_cast<long>(pass.extent.width);
    const auto height = static_cast<long>(pass.extent.height);
    const long left = std::clamp(std::lround(pixels.x), 0L, width);
    const long top = std::clamp(std::lround(pixels.y), 0L, height);
    const long right = std::clamp(std::lround(pixels.x + pixels.width), left, width);
    const long bottom = std::clamp(std::lround(pixels.y + pixels.height), top, height);
    const VkRect2D scissor{.offset = {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top)}, .extent = {static_cast<std::uint32_t>(right - left), static_cast<std::uint32_t>(bottom - top)}};
    vk.vkCmdSetScissor(frames[frameIndex].graphics, 0, 1, &scissor);
}

// The renderer gives viewports and scissors in the pixels of the window, which the turned images of a swapchain pass hold turned.
math::Rect Device::getPassPixels(const math::Rect& window) const noexcept {
    const math::Vec2 size{static_cast<float>(pass.window.width), static_cast<float>(pass.window.height)};
    return pass.turns != 0 ? SurfaceTurn::turnPixels(window, size, pass.turns) : window;
}

void Device::applyPipeline(Pipeline pipeline) {
    if (!pass.open || pass.skipped) {
        return;
    }

    PipelineRecord* record = pipelines.find(pipeline.id);
    if (record == nullptr) {
        throw ContractError(ContractError::Rule::MissingObject);
    }

    if (record->stored && record->generation != swapchainTarget.generation) {
        rebuildPipeline(*record);
    }

    bound.appliedSlots = 0;
    bound.bufferSlots = 0;
    if (bound.pipeline == record && bound.boundPipeline == record->pipeline) {
        return;
    }

    ProgramRecord* program = programs.find(record->program);
    vk.vkCmdBindPipeline(frames[frameIndex].graphics, VK_PIPELINE_BIND_POINT_GRAPHICS, record->pipeline);

    if (bound.program == nullptr || bound.program->layout != program->layout) {
        bound.resourcesDirty = true;
        bound.uniformsDirty = true;
    }

    if (bound.program == nullptr || !sharesUniforms(*bound.program, *program)) {
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

    if (bound.pipeline == nullptr) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    checkBindings(bindings);
    feedUniformBlocks(bindings);

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

void Device::checkBindings(const Bindings& bindings) {
    const PipelineRecord& pipeline = *bound.pipeline;
    for (std::uint32_t index = 0; index < pipeline.bufferCount; ++index) {
        BufferRules::checkVertex(buffers.get(bindings.vertexBuffers[index].id).description);
    }

    if (pipeline.indexType != IndexType::None) {
        BufferRules::checkIndex(buffers.get(bindings.indexBuffer.id).description);
    }
}

// A block whose slot the bindings name a uniform buffer for reads the buffer from the offset, within the copy of the frame of a stream buffer, and the other blocks read what `applyUniforms` gives them.
void Device::feedUniformBlocks(const Bindings& bindings) {
    const ProgramRecord& program = *bound.program;
    bound.bufferSlots = 0;
    for (std::size_t slot = 0; slot < kMaxBlocks; ++slot) {
        const int index = program.blockIndices[slot];
        const BufferRange& range = bindings.uniformBuffers[slot];
        if (index < 0 || !range.buffer.isValid()) {
            continue;
        }

        const std::uint32_t bit = 1U << slot;
        if ((bound.appliedSlots & bit) != 0) {
            throw ContractError(ContractError::Rule::Uniforms);
        }

        const BufferRecord& record = buffers.get(range.buffer.id);
        const auto block = static_cast<std::size_t>(index);
        BufferRules::checkUniform(record.description, range.offset, static_cast<std::size_t>(program.uniformBlocks[block].size));
        setUniformBlock(block, record.buffer, record.stride * record.current + range.offset);
        bound.bufferSlots |= bit;
    }
}

void Device::applyUniforms(int slot, std::span<const std::byte> data) {
    if (!pass.open || pass.skipped) {
        return;
    }

    if (bound.program == nullptr) {
        throw ContractError(ContractError::Rule::PipelineState);
    }

    const int index = slot >= 0 && slot < static_cast<int>(kMaxBlocks) ? bound.program->blockIndices[static_cast<std::size_t>(slot)] : -1;
    if (index < 0) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    const VkDeviceSize size = bound.program->uniformBlocks[static_cast<std::size_t>(index)].size;
    if (data.size() != size) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    const std::uint32_t bit = 1U << static_cast<unsigned>(slot);
    if ((bound.bufferSlots & bit) != 0) {
        throw ContractError(ContractError::Rule::Uniforms);
    }

    const UploadRing::Allocation allocation = allocateUniform(static_cast<std::size_t>(index), size);
    std::memcpy(allocation.data, data.data(), data.size());
    bound.appliedSlots |= bit;
}

// Each block names the buffer of the ring that holds its bytes, so the blocks applied before the ring moved to another buffer stay where they are.
UploadRing::Allocation Device::allocateUniform(std::size_t index, VkDeviceSize size) {
    const VkDeviceSize alignment = context->getProperties().limits.minUniformBufferOffsetAlignment;
    const UploadRing::Allocation allocation = uniforms.allocate(size, alignment);
    setUniformBlock(index, allocation.buffer, allocation.offset);
    return allocation;
}

// A block that keeps its buffer and its offset leaves the uniform set of the draws bound as it is.
void Device::setUniformBlock(std::size_t index, VkBuffer buffer, VkDeviceSize offset) noexcept {
    const std::uint32_t bit = 1U << index;
    const auto dynamicOffset = static_cast<std::uint32_t>(offset);
    if ((bound.uniformMask & bit) != 0 && bound.uniformBuffers[index] == buffer && bound.uniformOffsets[index] == dynamicOffset) {
        return;
    }

    if (bound.uniformBuffers[index] != buffer) {
        bound.uniformBuffers[index] = buffer;
        bound.uniformSet = VK_NULL_HANDLE;
    }

    bound.uniformOffsets[index] = dynamicOffset;
    bound.uniformMask |= bit;
    bound.uniformsDirty = true;
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
        throw ContractError(ContractError::Rule::PassOrder);
    }

    if (pass.skipped || count <= 0 || instances <= 0) {
        return;
    }

    if (bound.pipeline == nullptr) {
        throw ContractError(ContractError::Rule::PipelineState);
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
    bound.uniformSetProgram = nullptr;
    bound.uniformMask = 0;
}

// Programs share the uniform blocks of a draw when they read blocks of the same sizes at the same bindings, the ranges that their descriptor sets give the blocks.
bool Device::sharesUniforms(const ProgramRecord& first, const ProgramRecord& second) noexcept {
    return first.uniformLayout == second.uniformLayout && first.uniformBlocks == second.uniformBlocks;
}

void Device::bindVertexBuffers() {
    const std::uint32_t count = bound.pipeline->bufferCount;
    std::array<VkBuffer, kMaxVertexBuffers> handles{};
    std::array<VkDeviceSize, kMaxVertexBuffers> offsets{};
    for (std::uint32_t index = 0; index < count; ++index) {
        const BufferRecord& record = buffers.get(bound.bindings.vertexBuffers[index].id);
        handles[index] = record.buffer;
        offsets[index] = record.stride * record.current + static_cast<VkDeviceSize>(bound.bindings.vertexOffsets[index]);
    }

    if (count > 0) {
        vk.vkCmdBindVertexBuffers(frames[frameIndex].graphics, 0, count, handles.data(), offsets.data());
    }

    bound.vertexDirty = false;
}

void Device::bindIndexBuffer() {
    const BufferRecord& record = buffers.get(bound.bindings.indexBuffer.id);
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
                throw ContractError(ContractError::Rule::Bindings);
            }

            descriptorImages[next] = {.sampler = VK_NULL_HANDLE, .imageView = image->sampleView, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = VK_NULL_HANDLE, .dstBinding = texture.binding, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .pImageInfo = &descriptorImages[next++], .pBufferInfo = nullptr, .pTexelBufferView = nullptr});
        }

        for (const SlotBinding& sampler : program.samplers) {
            const SamplerRecord* record = samplers.find(key.samplers[static_cast<std::size_t>(sampler.slot)]);
            if (record == nullptr) {
                throw ContractError(ContractError::Rule::Bindings);
            }

            descriptorImages[next] = {.sampler = record->sampler, .imageView = VK_NULL_HANDLE, .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED};
            descriptorWrites.push_back({.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr, .dstSet = VK_NULL_HANDLE, .dstBinding = sampler.binding, .dstArrayElement = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .pImageInfo = &descriptorImages[next++], .pBufferInfo = nullptr, .pTexelBufferView = nullptr});
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

    if (bound.uniformSet == VK_NULL_HANDLE || bound.uniformSetProgram == nullptr || !sharesUniforms(*bound.uniformSetProgram, program)) {
        bound.uniformSet = descriptors.getUniformSet(program.uniformLayout, program.uniformBlocks, std::span(bound.uniformBuffers).first(program.uniformBlocks.size()));
        bound.uniformSetProgram = &program;
    }

    bound.uniformsDirty = false;
    vk.vkCmdBindDescriptorSets(frames[frameIndex].graphics, VK_PIPELINE_BIND_POINT_GRAPHICS, program.layout, program.uniformSet, 1, &bound.uniformSet, static_cast<std::uint32_t>(program.uniformBlocks.size()), bound.uniformOffsets.data());
}

void Device::pushDebugGroup(std::string_view name) {
    if (!labels || !ensureFrame()) {
        return;
    }

    if (!pass.open) {
        endSwapchainRendering();
    }

    labelText.assign(name);
    const VkDebugUtilsLabelEXT label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pNext = nullptr, .pLabelName = labelText.c_str(), .color = {}};
    vkCmdBeginDebugUtilsLabelEXT(frames[frameIndex].graphics, &label);
}

void Device::popDebugGroup() {
    if (!labels || !frameOpen) {
        return;
    }

    if (!pass.open) {
        endSwapchainRendering();
    }

    vkCmdEndDebugUtilsLabelEXT(frames[frameIndex].graphics);
}

std::unique_ptr<gpu::Device::Readback> Device::readImage(Image image, int width, int height) {
    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it reads back no image.");
    }

    const ImageRecord& record = images.get(image.id);
    if (record.kind != ImageKind::RenderTarget || record.samples != VK_SAMPLE_COUNT_1_BIT) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (width < 1 || height < 1 || width > record.width || height > record.height) {
        throw ContractError(ContractError::Rule::Readback);
    }

    return std::make_unique<CopyReadback>(readbacks.start(ReadbackQueue::ImageSource{.image = record.image, .layout = record.restLayout, .width = width, .height = height, .format = record.pixelFormat}));
}

// The copy goes to the queue after the frames that the device committed, so it reads what they left in the buffer.
std::unique_ptr<gpu::Device::Readback> Device::readBuffer(Buffer buffer, std::size_t offset, std::size_t size) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::Readback);
    }

    if (lost) {
        throw std::runtime_error("The Vulkan device stopped working, so it reads back no buffer.");
    }

    const BufferRecord& record = buffers.get(buffer.id);
    BufferRules::checkRead(record.description, offset, size);
    return std::make_unique<CopyReadback>(readbacks.start(ReadbackQueue::BufferSource{.buffer = record.buffer, .offset = offset, .size = size}));
}

// RenderDoc names a Vulkan device by the dispatch table of its instance.
void* Device::getRenderDocDevice() const noexcept {
    return *reinterpret_cast<void**>(instance->getHandle());
}

gpu::Device::CaptureSupport Device::getCaptureSupport() const {
    if (!renderDoc) {
        return {.tool = CaptureTool::None, .missing = std::string(renderdoc::Capturer::kMissing)};
    }

    return {.tool = CaptureTool::RenderDoc, .missing = {}};
}

void Device::beginCapture(const std::filesystem::path& path) {
    if (pass.open) {
        throw ContractError(ContractError::Rule::Capture);
    }

    if (!renderDoc) {
        throw std::logic_error(std::string(renderdoc::Capturer::kMissing));
    }

    renderDoc->begin(getRenderDocDevice(), path);
}

std::filesystem::path Device::endCapture() {
    if (!renderDoc) {
        throw ContractError(ContractError::Rule::Capture);
    }

    return renderDoc->end(getRenderDocDevice());
}

} // namespace haylen::gpu::vulkan
