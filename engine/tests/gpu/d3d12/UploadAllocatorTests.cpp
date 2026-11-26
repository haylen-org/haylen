#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <gtest/gtest.h>

#include <cstdint>

#include "gpu/d3d12/UploadAllocator.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::gpu::d3d12 {

// Drives the upload memory of Direct3D 12 on WARP with the fence values that a GPU reports, without drawing.
class UploadAllocatorTest : public ::testing::Test {
  protected:
    static constexpr std::size_t kFramesInFlight = 2;

    void SetUp() override {
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        ASSERT_TRUE(SUCCEEDED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf()))));
        Microsoft::WRL::ComPtr<IDXGIAdapter> warp;
        ASSERT_TRUE(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf()))));
        ASSERT_TRUE(SUCCEEDED(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(device.GetAddressOf()))));
    }

    Microsoft::WRL::ComPtr<ID3D12Device> device;
};

// A GPU that finished every frame at first and then falls a frame behind, as a real GPU does once the frames outrun it, still lets each frame fill a page of the pool, so frames like the ones before allocate nothing.
TEST_F(UploadAllocatorTest, KeepsAPageForEveryFrameInFlight) {
    UploadAllocator uploads(*device.Get(), kFramesInFlight);
    std::uint64_t fence = 0;
    for (int frame = 0; frame < 4; ++frame) {
        uploads.release(fence);
        static_cast<void>(uploads.allocate(256, 16));
        uploads.endFrame(++fence);
    }

    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 8; ++frame) {
        uploads.release(fence - 1);
        EXPECT_NE(uploads.allocate(256, 16).data, nullptr);
        uploads.endFrame(++fence);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
}

// A piece larger than a page, such as the pixels of a whole atlas, takes a page of its own, which goes back to the pool once its frame finished, so frames that upload such a piece again allocate nothing.
TEST_F(UploadAllocatorTest, KeepsThePagesOfPiecesLargerThanAPage) {
    constexpr UINT64 kLarge = 6ULL << 20U;
    UploadAllocator uploads(*device.Get(), kFramesInFlight);
    std::uint64_t fence = 0;
    for (int frame = 0; frame < 4; ++frame) {
        uploads.release(fence > 0 ? fence - 1 : 0);
        static_cast<void>(uploads.allocate(kLarge, 512));
        uploads.endFrame(++fence);
    }

    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 8; ++frame) {
        uploads.release(fence - 1);
        EXPECT_NE(uploads.allocate(kLarge, 512).data, nullptr);
        uploads.endFrame(++fence);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::gpu::d3d12
