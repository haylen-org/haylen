#include "gpu/vulkan/RenderPassCache.hpp"

#include <algorithm>

#include "gpu/vulkan/Context.hpp"
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
        attachments[count] = {.format = color.format, .samples = color.samples, .loadOp = color.load, .storeOp = color.store, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        colorReferences[index] = {.attachment = count++, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    }
    for (std::uint32_t index = 0; index < key.colorCount; ++index) {
        resolveReferences[index] = {.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        if (key.resolves[index] == VK_FORMAT_UNDEFINED) {
            continue;
        }
        attachments[count] = {.format = key.resolves[index], .samples = VK_SAMPLE_COUNT_1_BIT, .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .storeOp = VK_ATTACHMENT_STORE_OP_STORE, .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE, .initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        resolveReferences[index].attachment = count++;
        resolves = true;
    }

    VkAttachmentReference depthReference{.attachment = VK_ATTACHMENT_UNUSED, .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    if (key.depth.format != VK_FORMAT_UNDEFINED) {
        const Attachment& depth = key.depth;
        attachments[count] = {.format = depth.format, .samples = depth.samples, .loadOp = depth.load, .storeOp = depth.store, .stencilLoadOp = depth.stencilLoad, .stencilStoreOp = depth.stencilStore, .initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        depthReference.attachment = count++;
    }

    const VkSubpassDescription subpass{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = key.colorCount,
        .pColorAttachments = colorReferences.data(),
        .pResolveAttachments = resolves ? resolveReferences.data() : nullptr,
        .pDepthStencilAttachment = key.depth.format != VK_FORMAT_UNDEFINED ? &depthReference : nullptr,
    };
    const VkRenderPassCreateInfo info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount = count, .pAttachments = attachments.data(), .subpassCount = 1, .pSubpasses = &subpass};
    VkRenderPass pass = VK_NULL_HANDLE;
    Context::check(context.getFunctions().vkCreateRenderPass(context.getDevice(), &info, nullptr, &pass), "make a render pass");
    return pass;
}

VkFramebuffer RenderPassCache::getFramebuffer(VkRenderPass pass, std::span<const VkImageView> views, VkExtent2D extent) {
    FramebufferKey key{.pass = pass, .viewCount = static_cast<std::uint32_t>(views.size()), .width = extent.width, .height = extent.height};
    std::ranges::copy(views, key.views.begin());
    if (const auto found = framebuffers.find(key); found != framebuffers.end()) {
        return found->second;
    }

    const VkFramebufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass = pass, .attachmentCount = key.viewCount, .pAttachments = views.data(), .width = extent.width, .height = extent.height, .layers = 1};
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    Context::check(context.getFunctions().vkCreateFramebuffer(context.getDevice(), &info, nullptr, &framebuffer), "make a framebuffer");
    framebuffers.emplace(key, framebuffer);
    return framebuffer;
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
