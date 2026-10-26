#pragma once

#include <cstddef>
#include <string_view>

#include <volk.h>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::vulkan {

// The values of Vulkan that stand for the values of the contract of `gpu::Device`.
class Conversions final {
  public:
    Conversions() = delete;

    [[nodiscard]] static VkFormat toFormat(Device::PixelFormat format) noexcept;
    [[nodiscard]] static VkFormat toFormat(Device::VertexFormat format) noexcept;
    [[nodiscard]] static std::size_t getBytesPerPixel(Device::PixelFormat format) noexcept;
    [[nodiscard]] static bool isDepth(Device::PixelFormat format) noexcept;
    [[nodiscard]] static bool hasStencil(VkFormat format) noexcept;
    [[nodiscard]] static VkImageAspectFlags getAspects(VkFormat format) noexcept;
    [[nodiscard]] static VkPrimitiveTopology toTopology(Device::Primitive primitive) noexcept;
    [[nodiscard]] static VkBlendFactor toBlendFactor(Device::BlendFactor factor) noexcept;
    [[nodiscard]] static VkBlendOp toBlendOperation(Device::BlendOperation operation) noexcept;
    [[nodiscard]] static VkColorComponentFlags toColorMask(Device::ColorMask mask) noexcept;
    [[nodiscard]] static VkCompareOp toCompare(Device::CompareFunction function) noexcept;
    [[nodiscard]] static VkStencilOp toStencilOperation(Device::StencilOperation operation) noexcept;
    [[nodiscard]] static VkCullModeFlags toCullMode(Device::CullMode mode) noexcept;
    [[nodiscard]] static VkFrontFace toFrontFace(Device::FrontFace face) noexcept;
    [[nodiscard]] static VkFilter toFilter(Device::Filter filter) noexcept;
    [[nodiscard]] static VkSamplerMipmapMode toMipmapMode(Device::Filter filter) noexcept;
    [[nodiscard]] static VkSamplerAddressMode toAddressMode(Device::Wrap wrap) noexcept;
    [[nodiscard]] static VkShaderStageFlags toStage(Device::Stage stage) noexcept;
    [[nodiscard]] static VkImageViewType toViewType(Device::TextureType type) noexcept;
    [[nodiscard]] static VkAttachmentLoadOp toLoad(Device::Load load) noexcept;
    [[nodiscard]] static VkAttachmentStoreOp toStore(Device::Store store) noexcept;
    [[nodiscard]] static VkSampleCountFlagBits toSampleCount(int count) noexcept;
    [[nodiscard]] static VkIndexType toIndexType(Device::IndexType type) noexcept;
    [[nodiscard]] static std::size_t getIndexSize(Device::IndexType type) noexcept;

    // The name of a result code, which error messages show.
    [[nodiscard]] static std::string_view describe(VkResult result) noexcept;
};

} // namespace haylen::gpu::vulkan
