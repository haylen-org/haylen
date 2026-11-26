#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

namespace haylen::gpu::d3d12 {

// Descriptors of one type in heaps that only the CPU sees, where the views of images and the samplers live and from which the device copies them into the heaps the shaders read. It adds a heap when every descriptor is taken and reuses freed descriptors first, so descriptors never move.
class DescriptorHeap final {
  public:
    DescriptorHeap(ID3D12Device& owner, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT descriptorsPerHeap);

    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE allocate();
    void free(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    // How many descriptors are taken, out of the descriptors of every heap made so far.
    [[nodiscard]] int getUsed() const noexcept {
        return getSize() - static_cast<int>(freed.size());
    }

    [[nodiscard]] int getSize() const noexcept {
        return static_cast<int>(heaps.size() * pageSize);
    }

  private:
    ID3D12Device& device;
    D3D12_DESCRIPTOR_HEAP_TYPE type;
    UINT pageSize = 0;
    UINT stride = 0;
    std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> heaps;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> freed;
};

} // namespace haylen::gpu::d3d12
