#include "gpu/d3d12/UploadAllocator.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d12 {

UploadAllocator::UploadAllocator(ID3D12Device& owner) : device(owner) {}

UploadAllocator::Page UploadAllocator::makePage(UINT64 size) {
    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Page page{.size = size};
    direct3d::HResult::check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(page.resource.GetAddressOf())), std::format("create {} bytes of upload memory", size));
    page.resource->SetName(L"haylen-upload");
    const D3D12_RANGE nothing{.Begin = 0, .End = 0};
    void* mapped = nullptr;
    direct3d::HResult::check(page.resource->Map(0, &nothing, &mapped), "map the upload memory");
    page.data = static_cast<std::byte*>(mapped);
    page.address = page.resource->GetGPUVirtualAddress();
    return page;
}

UploadAllocator::Allocation UploadAllocator::allocate(UINT64 size, UINT64 alignment) {
    UINT64 offset = (cursor + alignment - 1) / alignment * alignment;
    if (current.resource == nullptr || offset + size > current.size) {
        if (current.resource != nullptr) {
            filled.push_back(std::move(current));
        }
        if (size > kPageSize) {
            current = makePage((size + kPageSize - 1) / kPageSize * kPageSize);
        } else if (free.empty()) {
            current = makePage(kPageSize);
        } else {
            current = std::move(free.back());
            free.pop_back();
        }
        offset = 0;
    }
    cursor = offset + size;
    return {.resource = current.resource.Get(), .offset = offset, .data = current.data + offset, .address = current.address + offset};
}

// The page in use closes with the frame, so the next frame starts on a page of its own.
void UploadAllocator::endFrame(std::uint64_t fenceValue) {
    if (current.resource != nullptr) {
        filled.push_back(std::move(current));
        current = {};
    }
    for (Page& page : filled) {
        page.fence = fenceValue;
        retired.push_back(std::move(page));
    }
    filled.clear();
}

void UploadAllocator::release(std::uint64_t completedValue) {
    const auto finished = std::ranges::partition(retired, [completedValue](const Page& page) { return page.fence > completedValue; });
    for (auto page = finished.begin(); page != finished.end(); ++page) {
        if (page->size == kPageSize) {
            free.push_back(std::move(*page));
        }
    }
    retired.erase(finished.begin(), finished.end());
}

} // namespace haylen::gpu::d3d12
