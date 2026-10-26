#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace haylen::gpu::d3d12 {

// A heap of descriptors that shaders read, which hands each frame the tables of its draws one after another and takes them back once the GPU finished the frame. A frame copies the descriptors of a table once, and every later table of the same descriptors in that frame reads the first one, so redundant bindings cost no copies and the small heap of samplers lasts. A heap that runs out of room is replaced by one twice as large while the frames in flight keep reading the old one.
class DescriptorRing final {
  public:
    static constexpr std::size_t kMaxTable = 32;

    DescriptorRing(ID3D12Device& owner, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT initialCapacity, UINT largestCapacity, std::string_view label);

    // The table holding copies of the descriptors, or nothing when the heap has no room for it until it grows.
    [[nodiscard]] std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> write(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> sources);

    // Makes a heap twice as large and returns the old one, which lives until the frames in flight finished. Throws `std::runtime_error` when the heap is already as large as Direct3D 12 allows.
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> grow();

    // Closes the tables of the frame that the fence value marks, and frees the tables of the frames the GPU finished.
    void endFrame(std::uint64_t fenceValue);
    void release(std::uint64_t completedValue);

    [[nodiscard]] ID3D12DescriptorHeap* get() const noexcept {
        return heap.Get();
    }

  private:
    static constexpr std::size_t kCacheSize = 256;
    static constexpr std::size_t kProbes = 8;
    static constexpr std::size_t kMaxFrames = 8;

    struct Frame {
        std::uint64_t fence = 0;
        std::uint64_t end = 0;
    };

    // A table that the frame of `generation` copied, under its descriptors.
    struct Entry {
        std::uint32_t generation = 0;
        std::uint32_t count = 0;
        std::uint64_t position = 0;
        std::array<SIZE_T, kMaxTable> key{};
    };

    void make(UINT size);
    [[nodiscard]] static std::size_t hash(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> sources) noexcept;
    [[nodiscard]] static bool matches(const Entry& entry, std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> sources) noexcept;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE locate(std::uint64_t position) const noexcept;

    ID3D12Device& device;
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    std::string_view name;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_GPU_DESCRIPTOR_HANDLE start{};
    UINT stride = 0;
    UINT capacity = 0;
    UINT largest = 0;
    // The positions count every descriptor ever handed out, and the heap holds them modulo its capacity.
    std::uint64_t head = 0;
    std::uint64_t tail = 0;
    std::array<Frame, kMaxFrames> frames{};
    std::size_t firstFrame = 0;
    std::size_t frameCount = 0;
    std::vector<Entry> cache;
    std::uint32_t generation = 1;
};

} // namespace haylen::gpu::d3d12
