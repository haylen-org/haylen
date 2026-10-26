#include "gpu/d3d12/BufferArena.hpp"

#include <algorithm>
#include <format>

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d12 {

using Microsoft::WRL::ComPtr;

BufferArena::BufferArena(ID3D12Device& owner) : device(owner) {}

ComPtr<ID3D12Resource> BufferArena::makeResource(UINT64 size) const {
    const D3D12_HEAP_PROPERTIES heap{.Type = D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> resource;
    direct3d::HResult::check(device.CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(resource.GetAddressOf())), std::format("create a buffer of {} bytes", size));
    return resource;
}

BufferArena::Allocation BufferArena::allocate(UINT64 size) {
    if (size > kLargestRange) {
        return {.resource = makeResource(size), .range = {.size = size}};
    }

    const UINT64 room = (size + kAlignment - 1) / kAlignment * kAlignment;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        UINT64 offset = 0;
        if (take(blocks[index], room, offset)) {
            return {.resource = blocks[index].resource, .range = {.block = static_cast<std::uint32_t>(index), .offset = offset, .size = room}};
        }
    }

    Block& block = blocks.emplace_back(Block{.resource = makeResource(kBlockSize), .size = kBlockSize});
    block.resource->SetName(L"haylen-buffers");
    block.available.push_back({.block = static_cast<std::uint32_t>(blocks.size() - 1), .offset = room, .size = kBlockSize - room});
    return {.resource = block.resource, .range = {.block = static_cast<std::uint32_t>(blocks.size() - 1), .offset = 0, .size = room}};
}

// The first free range that fits gives its start, and what it has left stays free.
bool BufferArena::take(Block& block, UINT64 size, UINT64& offset) {
    const auto found = std::ranges::find_if(block.available, [size](const Range& range) { return range.size >= size; });
    if (found == block.available.end()) {
        return false;
    }
    offset = found->offset;
    found->offset += size;
    found->size -= size;
    if (found->size == 0) {
        block.available.erase(found);
    }
    return true;
}

void BufferArena::free(const Range& range) {
    if (range.block == Range::kOwnResource) {
        return;
    }
    std::vector<Range>& ranges = blocks[range.block].available;
    const auto next = std::ranges::lower_bound(ranges, range.offset, {}, &Range::offset);
    const auto inserted = ranges.insert(next, range);

    // A range that touches its neighbors joins them, so the block keeps its room in as few ranges as it can.
    const auto after = inserted + 1;
    if (after != ranges.end() && inserted->offset + inserted->size == after->offset) {
        inserted->size += after->size;
        ranges.erase(after);
    }
    if (inserted != ranges.begin()) {
        const auto before = inserted - 1;
        if (before->offset + before->size == inserted->offset) {
            before->size += inserted->size;
            ranges.erase(inserted);
        }
    }
}

} // namespace haylen::gpu::d3d12
