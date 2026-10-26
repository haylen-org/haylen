#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::gpu::d3d12 {

// Memory of upload heaps that stays mapped, which each frame fills from the start of its pages with uniforms and the data that copies take into buffers and images. Pages go back to the pool once the GPU finished the frame that filled them, so a frame never waits for the GPU and a frame like the one before takes pages from the pool without allocating. A piece larger than a page takes a page of its own, which goes once its frame finished.
class UploadAllocator final {
  public:
    struct Allocation {
        ID3D12Resource* resource = nullptr;
        UINT64 offset = 0;
        std::byte* data = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS address = 0;
    };

    explicit UploadAllocator(ID3D12Device& owner);

    [[nodiscard]] Allocation allocate(UINT64 size, UINT64 alignment);

    // Tags the pages that the frame filled with the fence value that marks its end, and takes back the pages of the frames the GPU finished.
    void endFrame(std::uint64_t fenceValue);
    void release(std::uint64_t completedValue);

  private:
    static constexpr UINT64 kPageSize = 4ULL << 20U;

    struct Page {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        std::byte* data = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS address = 0;
        UINT64 size = 0;
        std::uint64_t fence = 0;
    };

    [[nodiscard]] Page makePage(UINT64 size);

    ID3D12Device& device;
    std::vector<Page> free;
    std::vector<Page> filled;
    std::vector<Page> retired;
    Page current;
    UINT64 cursor = 0;
};

} // namespace haylen::gpu::d3d12
