#include "gpu/d3d12/DescriptorRing.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "gpu/direct3d/HResult.hpp"
#include "gpu/direct3d/WideText.hpp"

namespace haylen::gpu::d3d12 {

DescriptorRing::DescriptorRing(ID3D12Device& owner, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT initialCapacity, UINT largestCapacity, std::string_view label) : device(owner), type(heapType), name(label), stride(owner.GetDescriptorHandleIncrementSize(heapType)), largest(largestCapacity), cache(kCacheSize) {
    make(initialCapacity);
}

void DescriptorRing::make(UINT size) {
    const D3D12_DESCRIPTOR_HEAP_DESC desc{.Type = type, .NumDescriptors = size, .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, .NodeMask = 0};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> made;
    direct3d::HResult::check(device.CreateDescriptorHeap(&desc, IID_PPV_ARGS(made.GetAddressOf())), std::format("create a heap of {} descriptors for the shaders", size));
    made->SetName(direct3d::WideText::toWide(name).c_str());
    heap = std::move(made);
    start = heap->GetGPUDescriptorHandleForHeapStart();
    capacity = size;
    head = 0;
    tail = 0;
    frameCount = 0;
    generation = generation == UINT32_MAX ? 1 : generation + 1;
}

std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> DescriptorRing::write(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> sources) {
    const std::size_t code = hash(sources);
    Entry* vacant = nullptr;
    for (std::size_t probe = 0; probe < kProbes; ++probe) {
        Entry& entry = cache[(code + probe) % kCacheSize];
        if (entry.generation != generation) {
            vacant = vacant != nullptr ? vacant : &entry;
            continue;
        }
        if (matches(entry, sources)) {
            return locate(entry.position);
        }
    }

    // A table never wraps around the end of the heap, so one that would skips to its start.
    const auto count = static_cast<UINT>(sources.size());
    std::uint64_t position = head;
    if (position % capacity + count > capacity) {
        position += capacity - position % capacity;
    }
    if (position + count - tail > capacity) {
        return std::nullopt;
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE destination{heap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(position % capacity) * stride};
    device.CopyDescriptors(1, &destination, &count, count, sources.data(), nullptr, type);
    head = position + count;

    if (vacant != nullptr) {
        vacant->generation = generation;
        vacant->count = count;
        vacant->position = position;
        std::ranges::transform(sources, vacant->key.begin(), &D3D12_CPU_DESCRIPTOR_HANDLE::ptr);
    }
    return locate(position);
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorRing::grow() {
    if (capacity >= largest) {
        throw std::runtime_error(std::format("The draws of one frame bind more than the {} descriptors that a heap of Direct3D 12 holds.", largest));
    }
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> old = heap;
    make(std::min(capacity * 2, largest));
    return old;
}

void DescriptorRing::endFrame(std::uint64_t fenceValue) {
    generation = generation == UINT32_MAX ? 1 : generation + 1;

    // The frames in flight stay fewer than the room for them, and a frame past it joins the last one, which the GPU finishes no sooner.
    if (frameCount == kMaxFrames) {
        frames[(firstFrame + frameCount - 1) % kMaxFrames] = {.fence = fenceValue, .end = head};
        return;
    }
    frames[(firstFrame + frameCount) % kMaxFrames] = {.fence = fenceValue, .end = head};
    ++frameCount;
}

void DescriptorRing::release(std::uint64_t completedValue) {
    while (frameCount > 0 && frames[firstFrame].fence <= completedValue) {
        tail = frames[firstFrame].end;
        firstFrame = (firstFrame + 1) % kMaxFrames;
        --frameCount;
    }
}

std::size_t DescriptorRing::hash(std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> sources) noexcept {
    std::size_t code = 14695981039346656037ULL;
    for (const D3D12_CPU_DESCRIPTOR_HANDLE& source : sources) {
        code = (code ^ source.ptr) * 1099511628211ULL;
    }
    return code;
}

bool DescriptorRing::matches(const Entry& entry, std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> sources) noexcept {
    return entry.count == sources.size() && std::ranges::equal(sources, std::span(entry.key).first(sources.size()), {}, &D3D12_CPU_DESCRIPTOR_HANDLE::ptr);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorRing::locate(std::uint64_t position) const noexcept {
    return {start.ptr + (position % capacity) * stride};
}

} // namespace haylen::gpu::d3d12
