#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <volk.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::vulkan {

class Context;
class ReleaseQueue;

// The render passes and framebuffers of a device whose GPU lacks dynamic rendering. A render pass is made once for each choice of formats, sample counts and load and store actions, and a framebuffer once for each render pass, set of image views and size. The attachments keep their layouts through a pass, since the device moves them in and out of those layouts with barriers of its own.
class RenderPassCache final {
  public:
    struct Attachment {
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp load = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp store = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp stencilLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        friend bool operator==(const Attachment&, const Attachment&) = default;
    };

    // The color attachments, the formats of the images that resolve them or `VK_FORMAT_UNDEFINED` for none, and the depth and stencil attachment, whose format is undefined when the pass has none.
    struct PassKey {
        std::array<Attachment, Device::kMaxColorTargets> colors{};
        std::array<VkFormat, Device::kMaxColorTargets> resolves{};
        std::uint32_t colorCount = 0;
        Attachment depth;

        friend bool operator==(const PassKey&, const PassKey&) = default;
    };

    // The views of a framebuffer in the order of the attachments of its render pass: the colors, the images that resolve them and the depth and stencil image.
    static constexpr std::size_t kMaxViews = Device::kMaxColorTargets * 2 + 1;

    RenderPassCache(const Context& owner, ReleaseQueue& releases) noexcept : context(owner), releaseQueue(releases) {}
    ~RenderPassCache();
    RenderPassCache(const RenderPassCache&) = delete;
    RenderPassCache& operator=(const RenderPassCache&) = delete;

    [[nodiscard]] VkRenderPass getRenderPass(const PassKey& key);
    [[nodiscard]] VkFramebuffer getFramebuffer(VkRenderPass pass, std::span<const VkImageView> views, VkExtent2D extent);

    // Lets go of the framebuffers that hold a destroyed view once the frames that may use them finished.
    void forgetView(VkImageView view, std::uint64_t serial);

  private:
    struct FramebufferKey {
        VkRenderPass pass = VK_NULL_HANDLE;
        std::array<VkImageView, kMaxViews> views{};
        std::uint32_t viewCount = 0;
        std::uint32_t width = 0;
        std::uint32_t height = 0;

        friend bool operator==(const FramebufferKey&, const FramebufferKey&) = default;
    };

    struct FramebufferHash {
        [[nodiscard]] std::size_t operator()(const FramebufferKey& key) const noexcept;
    };

    [[nodiscard]] VkRenderPass makeRenderPass(const PassKey& key) const;

    const Context& context;
    ReleaseQueue& releaseQueue;
    std::vector<std::pair<PassKey, VkRenderPass>> renderPasses;
    std::unordered_map<FramebufferKey, VkFramebuffer, FramebufferHash> framebuffers;
};

} // namespace haylen::gpu::vulkan
