#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::gpu::d3d12 {

// Places the small buffers of the device in ranges of large buffers of the default heap, since a buffer of its own costs a call to the kernel and 64 kilobytes of memory however small it is. Each block keeps its available ranges in the order of their offsets and merges neighbors when a range comes back, and a block that runs out of room is followed by a new one. Buffers live in the common state, so the ranges of one block share the states that each command list promotes them to. A buffer larger than `kLargestRange` takes a resource of its own.
class BufferArena final {
  public:
    static constexpr UINT64 kBlockSize = 4ULL << 20U;
    static constexpr UINT64 kLargestRange = 1ULL << 20U;

    // The part of a block that a buffer holds, or a whole resource when `block` is `kOwnResource`.
    struct Range {
        static constexpr std::uint32_t kOwnResource = UINT32_MAX;

        std::uint32_t block = kOwnResource;
        UINT64 offset = 0;
        UINT64 size = 0;
    };

    struct Allocation {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        Range range;
    };

    explicit BufferArena(ID3D12Device& owner);

    [[nodiscard]] Allocation allocate(UINT64 size);

    // Gives the range back, once the GPU finished every frame that used it.
    void free(const Range& range);

  private:
    static constexpr UINT64 kAlignment = 256;

    struct Block {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        UINT64 size = 0;
        std::vector<Range> available;
    };

    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12Resource> makeResource(UINT64 size) const;
    [[nodiscard]] static bool take(Block& block, UINT64 size, UINT64& offset);

    ID3D12Device& device;
    std::vector<Block> blocks;
};

} // namespace haylen::gpu::d3d12
