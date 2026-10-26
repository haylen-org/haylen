#include "gpu/d3d12/FrameTimer.hpp"

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d12 {

FrameTimer::FrameTimer(ID3D12Device& device, ID3D12CommandQueue& queue, std::size_t frames) : measured(frames, false) {
    const D3D12_QUERY_HEAP_DESC heap{.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP, .Count = static_cast<UINT>(frames * 2), .NodeMask = 0};
    direct3d::HResult::check(device.CreateQueryHeap(&heap, IID_PPV_ARGS(queries.GetAddressOf())), "create the timestamp queries of the frames");

    const D3D12_HEAP_PROPERTIES readback{.Type = D3D12_HEAP_TYPE_READBACK};
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = frames * 2 * sizeof(std::uint64_t);
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    direct3d::HResult::check(device.CreateCommittedResource(&readback, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(results.GetAddressOf())), "create the buffer of the timestamps of the frames");
    results->SetName(L"haylen-timestamps");
    void* data = nullptr;
    direct3d::HResult::check(results->Map(0, nullptr, &data), "map the timestamps of the frames");
    mapped = static_cast<const std::uint64_t*>(data);

    UINT64 ticks = 0;
    queue.GetTimestampFrequency(&ticks);
    frequency = static_cast<double>(ticks);
}

void FrameTimer::begin(ID3D12GraphicsCommandList& list, std::size_t frame) {
    list.EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, static_cast<UINT>(frame * 2));
}

void FrameTimer::end(ID3D12GraphicsCommandList& list, std::size_t frame) {
    const auto first = static_cast<UINT>(frame * 2);
    list.EndQuery(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first + 1);
    list.ResolveQueryData(queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, first, 2, results.Get(), first * sizeof(std::uint64_t));
    measured[frame] = true;
}

void FrameTimer::collect(std::size_t frame) {
    if (!measured[frame] || frequency <= 0.0) {
        return;
    }
    const std::uint64_t start = mapped[frame * 2];
    const std::uint64_t finish = mapped[frame * 2 + 1];
    if (finish >= start) {
        seconds = static_cast<double>(finish - start) / frequency;
    }
}

} // namespace haylen::gpu::d3d12
