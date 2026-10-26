#include "gpu/d3d12/DescriptorHeap.hpp"

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d12 {

DescriptorHeap::DescriptorHeap(ID3D12Device& owner, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT descriptorsPerHeap) : device(owner), type(heapType), pageSize(descriptorsPerHeap), stride(owner.GetDescriptorHandleIncrementSize(heapType)) {}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::allocate() {
    if (freed.empty()) {
        const D3D12_DESCRIPTOR_HEAP_DESC desc{.Type = type, .NumDescriptors = pageSize, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE, .NodeMask = 0};
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
        direct3d::HResult::check(device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.GetAddressOf())), "create a heap of descriptors");

        // The descriptors of the new heap go on the list from the last, so the first ones come out first.
        const D3D12_CPU_DESCRIPTOR_HANDLE start = heap->GetCPUDescriptorHandleForHeapStart();
        freed.reserve(freed.size() + pageSize);
        for (UINT index = pageSize; index > 0; --index) {
            freed.push_back({start.ptr + static_cast<SIZE_T>(index - 1) * stride});
        }
        heaps.push_back(std::move(heap));
    }
    const D3D12_CPU_DESCRIPTOR_HANDLE handle = freed.back();
    freed.pop_back();
    return handle;
}

void DescriptorHeap::free(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
    freed.push_back(handle);
}

} // namespace haylen::gpu::d3d12
