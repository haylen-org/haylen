#include "gpu/d3d11/FrameTimer.hpp"

#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::d3d11 {

FrameTimer::FrameTimer(ID3D11Device& device) {
    const D3D11_QUERY_DESC disjoint{.Query = D3D11_QUERY_TIMESTAMP_DISJOINT, .MiscFlags = 0};
    const D3D11_QUERY_DESC timestamp{.Query = D3D11_QUERY_TIMESTAMP, .MiscFlags = 0};
    for (Slot& slot : slots) {
        direct3d::HResult::check(device.CreateQuery(&disjoint, slot.disjoint.GetAddressOf()), "create a query of the GPU clock");
        direct3d::HResult::check(device.CreateQuery(&timestamp, slot.start.GetAddressOf()), "create a timestamp query");
        direct3d::HResult::check(device.CreateQuery(&timestamp, slot.finish.GetAddressOf()), "create a timestamp query");
    }
}

void FrameTimer::begin(ID3D11DeviceContext& context) {
    collect(context);
    Slot& slot = slots[current];
    open = !slot.pending;
    if (!open) {
        return;
    }
    context.Begin(slot.disjoint.Get());
    context.End(slot.start.Get());
}

void FrameTimer::end(ID3D11DeviceContext& context) {
    if (!open) {
        return;
    }
    Slot& slot = slots[current];
    context.End(slot.finish.Get());
    context.End(slot.disjoint.Get());
    slot.pending = true;
    current = (current + 1) % kSlots;
    open = false;
}

// The slots finish in the order the frames used them, so reading stops at the first one the GPU has not finished.
void FrameTimer::collect(ID3D11DeviceContext& context) {
    while (slots[oldest].pending) {
        Slot& slot = slots[oldest];
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT clock{};
        if (context.GetData(slot.disjoint.Get(), &clock, sizeof(clock), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            return;
        }
        // Some GPUs finish the disjoint query before its timestamps, so the slot waits for all three before it is used again.
        UINT64 start = 0;
        UINT64 finish = 0;
        if (context.GetData(slot.start.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK || context.GetData(slot.finish.Get(), &finish, sizeof(finish), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) {
            return;
        }
        if (clock.Disjoint == FALSE && clock.Frequency > 0 && finish >= start) {
            seconds = static_cast<double>(finish - start) / static_cast<double>(clock.Frequency);
        }
        slot.pending = false;
        oldest = (oldest + 1) % kSlots;
    }
}

} // namespace haylen::gpu::d3d11
