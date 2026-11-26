#pragma once

#include <volk.h>

namespace haylen::gpu::vulkan {

// The pipeline stages and memory accesses that use an image in each layout the device moves images through, from which its barriers wait for the earlier uses of an image and make its writes visible to the next ones.
class ImageLayouts final {
  public:
    // The accesses that write an image, which a barrier makes available before the image leaves the use that wrote it.
    static constexpr VkAccessFlags kWrites = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

    // The stage and the accesses of multisample resolves, which Vulkan runs in the stage of color attachments with their accesses for depth attachments too.
    static constexpr VkPipelineStageFlags kResolveStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    static constexpr VkAccessFlags kResolveAccesses = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    ImageLayouts() = delete;

    [[nodiscard]] static VkPipelineStageFlags getStages(VkImageLayout layout) noexcept;
    [[nodiscard]] static VkAccessFlags getAccess(VkImageLayout layout) noexcept;

    // The writes among the accesses of the layout, which a barrier makes available before the image leaves it.
    [[nodiscard]] static VkAccessFlags getWrites(VkImageLayout layout) noexcept;

    // A barrier that moves the part of the image out of the layout it rested in, from the layout it names, which is undefined when its contents go.
    [[nodiscard]] static VkImageMemoryBarrier makeBarrier(VkImage image, const VkImageSubresourceRange& range, VkImageLayout rest, VkImageLayout from, VkImageLayout to) noexcept;
};

} // namespace haylen::gpu::vulkan
