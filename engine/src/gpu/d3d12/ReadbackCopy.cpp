#include "gpu/d3d12/ReadbackCopy.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::gpu::d3d12 {

ReadbackCopy::ReadbackCopy(Microsoft::WRL::ComPtr<ID3D12Resource> copy, Microsoft::WRL::ComPtr<ID3D12Fence> frames, std::uint64_t frameValue) noexcept : buffer(std::move(copy)), fence(std::move(frames)), value(frameValue) {}

std::optional<std::span<const std::byte>> ReadbackCopy::map(std::size_t bytes, std::string_view failure) {
    if (fence->GetCompletedValue() < value) {
        return std::nullopt;
    }

    const D3D12_RANGE range{.Begin = 0, .End = static_cast<SIZE_T>(bytes)};
    void* mapped = nullptr;
    if (FAILED(buffer->Map(0, &range, &mapped))) {
        throw std::runtime_error(std::string(failure));
    }

    return std::span<const std::byte>(static_cast<const std::byte*>(mapped), bytes);
}

void ReadbackCopy::unmap() noexcept {
    const D3D12_RANGE nothing{.Begin = 0, .End = 0};
    buffer->Unmap(0, &nothing);
}

} // namespace haylen::gpu::d3d12
