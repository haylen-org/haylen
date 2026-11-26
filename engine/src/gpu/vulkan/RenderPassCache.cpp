#include "gpu/vulkan/RenderPassCache.hpp"

#include <algorithm>

#include "gpu/vulkan/Context.hpp"
#include "gpu/vulkan/Conversions.hpp"
#include "gpu/vulkan/ReleaseQueue.hpp"

namespace haylen::gpu::vulkan {

RenderPassCache::~RenderPassCache() {
    const VolkDeviceTable& vk = context.getFunctions();
    for (const auto& [key, framebuffer] : framebuffers) {
        vk.vkDestroyFramebuffer(context.getDevice(), framebuffer, nullptr);
    }

    for (const auto& [key, pass] : renderPasses) {
        vk.vkDestroyRenderPass(context.getDevice(), pass, nullptr);
    }
}

std::size_t RenderPassCache::FramebufferHash::operator()(const FramebufferKey& key) const noexcept {
    std::uint64_t hash = 14695981039346656037ULL ^ Context::toHandle(key.pass);
    for (std::uint32_t index = 0; index < key.viewCount; ++index) {
        hash = (hash ^ Context::toHandle(key.views[index])) * 1099511628211ULL;
    }

    hash = (hash ^ key.width) * 1099511628211ULL;
    hash = (hash ^ key.height) * 1099511628211ULL;
    return static_cast<std::size_t>(hash ^ (hash >> 32));
}

VkRenderPass RenderPassCache::getRenderPass(const PassKey& key) {
    const auto found = std::ranges::find(renderPasses, key, &std::pair<PassKey, VkRenderPass>::first);
    if (found != renderPasses.end()) {
        return found->second;
    }

    const VkRenderPass pass = makeRenderPass(key);
    renderPasses.emplace_back(key, pass);
    return pass;
}

VkRenderPass RenderPassCache::makeRenderPass(const PassKey& key) const {
    std::array<VkAttachmentDescription, kMaxViews> attachments{};
    std::array<VkAttachmentReference, Device::kMaxColorTargets> colorReferences{};
    std::array<VkAttachmentReference, Device::kMaxColorTargets> resolveReferences{};
    std::uint32_t count = 0;
    bool resolves = false;

    for (std::uint32_t index = 0; index < key.colorCount; ++index) {
        const Attachment& color = key.colors[index];
        attachments[count] = {.flags = 0, .format = color.format, .samples = color.samples, .loadOp = color.load, .storeOp = color.store, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        colorReferences[index] = {.attachment = count++, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }

    for (std::uint32_t index = 0; index < key.colorCount; ++index) {
        resolveReferences[index] = {.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

        if (key.resolves[index] == VK_FORMAT_UNDEFINED) {
            continue;
        }

        attachments[count] = {.flags = 0, .format = key.resolves[index], .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        resolveReferences[index].attachment = count++;
        resolves = true;
    }

    VkAttachmentReference depthReference{.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    if (key.depth.format != VK_FORMAT_UNDEFINED) {
        const Attachment& depth = key.depth;
        attachments[count] = {.flags = 0, .format = depth.format, .samples = depth.samples, .loadOp = depth.load, .storeOp = depth.store, .stencilLoadOp = depth.stencilLoad, .stencilStoreOp = depth.stencilStore, .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        depthReference.attachment = count++;
    }

    const VkSubpassDescription subpass{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = key.colorCount,
        .pColorAttachments = colorReferences.data(),
        .pResolveAttachments = resolves ? resolveReferences.data() : nullptr,
        .pDepthStencilAttachment = key.depth.format != VK_FORMAT_UNDEFINED ? &depthReference : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };

    if (key.depthResolve != VK_FORMAT_UNDEFINED) {
        return makeDepthResolvePass(key, std::span(attachments.data(), count), subpass);
    }

    const VkRenderPassCreateInfo info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .pNext = nullptr, .flags = 0, .attachmentCount = count, .pAttachments = attachments.data(), .subpassCount = 1, .pSubpasses = &subpass, .dependencyCount = 0, .pDependencies = nullptr};
    VkRenderPass pass = VK_NULL_HANDLE;
    Context::check(context.getFunctions().vkCreateRenderPass(context.getDevice(), &info, nullptr, &pass), "make a render pass");
    return pass;
}

// The second version describes the attachments and the subpass of the first, and after them the image of one sample that the depth resolves into by its mode when the subpass ends. The stencil resolves by no mode, which Vulkan allows for a depth format without stencil, the only one whose depth a pass resolves.
VkRenderPass RenderPassCache::makeDepthResolvePass(const PassKey& key, std::span<const VkAttachmentDescription> described, const VkSubpassDescription& subpass) const {
    std::array<VkAttachmentDescription2, kMaxViews> attachments{};
    for (std::size_t index = 0; index < described.size(); ++index) {
        attachments[index] = toVersion2(described[index]);
    }

    const auto resolveIndex = static_cast<std::uint32_t>(described.size());
    attachments[resolveIndex] = {.sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2, .pNext = nullptr, .flags = 0, .format = key.depthResolve, .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    std::array<VkAttachmentReference2, Device::kMaxColorTargets> colors{};
    std::array<VkAttachmentReference2, Device::kMaxColorTargets> resolves{};
    for (std::uint32_t index = 0; index < subpass.colorAttachmentCount; ++index) {
        colors[index] = toVersion2(subpass.pColorAttachments[index], VK_IMAGE_ASPECT_COLOR_BIT);
        if (subpass.pResolveAttachments != nullptr) {
            resolves[index] = toVersion2(subpass.pResolveAttachments[index], VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }

    const VkImageAspectFlags depthAspects = Conversions::getAspects(key.depth.format);
    const VkAttachmentReference2 depth = toVersion2(*subpass.pDepthStencilAttachment, depthAspects);
    const VkAttachmentReference2 depthResolve{.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, .pNext = nullptr, .attachment = resolveIndex, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .aspectMask = depthAspects};
    const VkSubpassDescriptionDepthStencilResolve resolve{.sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE, .pNext = nullptr, .depthResolveMode = key.depthResolveMode, .stencilResolveMode = VK_RESOLVE_MODE_NONE, .pDepthStencilResolveAttachment = &depthResolve};
    const VkSubpassDescription2 resolving{
        .sType = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2,
        .pNext = &resolve,
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .viewMask = 0,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = subpass.colorAttachmentCount,
        .pColorAttachments = colors.data(),
        .pResolveAttachments = subpass.pResolveAttachments != nullptr ? resolves.data() : nullptr,
        .pDepthStencilAttachment = &depth,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };
    const VkRenderPassCreateInfo2 info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2, .pNext = nullptr, .flags = 0, .attachmentCount = resolveIndex + 1, .pAttachments = attachments.data(), .subpassCount = 1, .pSubpasses = &resolving, .dependencyCount = 0, .pDependencies = nullptr, .correlatedViewMaskCount = 0, .pCorrelatedViewMasks = nullptr};

    // The second version is part of Vulkan 1.2, and before it the extension that the context turns on with the resolves of depth.
    const VolkDeviceTable& vk = context.getFunctions();
    const PFN_vkCreateRenderPass2 create = context.getApiVersion() >= VK_API_VERSION_1_2 ? vk.vkCreateRenderPass2 : vk.vkCreateRenderPass2KHR;
    VkRenderPass pass = VK_NULL_HANDLE;
    Context::check(create(context.getDevice(), &info, nullptr, &pass), "make a render pass that resolves depth");
    return pass;
}

VkAttachmentDescription2 RenderPassCache::toVersion2(const VkAttachmentDescription& attachment) noexcept {
    return {
        .sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2,
        .pNext = nullptr,
        .flags = attachment.flags,
        .format = attachment.format,
        .samples = attachment.samples,
        .loadOp = attachment.loadOp,
        .storeOp = attachment.storeOp,
        .stencilLoadOp = attachment.stencilLoadOp,
        .stencilStoreOp = attachment.stencilStoreOp,
        .initialLayout = attachment.initialLayout,
        .finalLayout = attachment.finalLayout,
    };
}

VkAttachmentReference2 RenderPassCache::toVersion2(const VkAttachmentReference& reference, VkImageAspectFlags aspects) noexcept {
    return {.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2, .pNext = nullptr, .attachment = reference.attachment, .layout = reference.layout, .aspectMask = aspects};
}

VkFramebuffer RenderPassCache::getFramebuffer(VkRenderPass pass, std::span<const VkImageView> views, VkExtent2D extent) {
    FramebufferKey key{.pass = pass, .viewCount = static_cast<std::uint32_t>(views.size()), .width = extent.width, .height = extent.height};
    std::ranges::copy(views, key.views.begin());

    if (const auto found = framebuffers.find(key); found != framebuffers.end()) {
        return found->second;
    }

    const VkFramebufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .pNext = nullptr, .flags = 0, .renderPass = pass, .attachmentCount = key.viewCount, .pAttachments = views.data(), .width = extent.width, .height = extent.height, .layers = 1};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    ReleaseQueue::Hold room = releaseQueue.hold();
    Context::check(context.getFunctions().vkCreateFramebuffer(context.getDevice(), &info, nullptr, &framebuffer), "make a framebuffer");
    try {
        framebuffers.emplace(key, framebuffer);
        room.keep();
        return framebuffer;
    } catch (...) {
        context.getFunctions().vkDestroyFramebuffer(context.getDevice(), framebuffer, nullptr);
        throw;
    }
}

void RenderPassCache::forgetView(VkImageView view, std::uint64_t serial) {
    for (auto entry = framebuffers.begin(); entry != framebuffers.end();) {
        const auto end = entry->first.views.begin() + entry->first.viewCount;
        if (std::find(entry->first.views.begin(), end, view) == end) {
            ++entry;
            continue;
        }

        releaseQueue.push(ReleaseQueue::Framebuffer{.framebuffer = entry->second}, serial);
        entry = framebuffers.erase(entry);
    }
}

} // namespace haylen::gpu::vulkan
