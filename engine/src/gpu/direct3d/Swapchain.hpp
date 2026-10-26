#pragma once

#include <dcomp.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::direct3d {

// The flip model swapchain of a window of Windows, which a device of Direct3D 11 makes on its device and a device of Direct3D 12 on its command queue. A transparent window takes its frames through DirectComposition, which composites them with their premultiplied alpha over what lies behind the window, since only a swapchain for composition keeps the alpha. A swapchain without vertical sync presents with tearing where the display allows it, so frames reach the screen without waiting for its refresh.
class Swapchain final {
  public:
    static constexpr UINT kBufferCount = 3;

    // Makes the swapchain on the window of the surface, at the size of the surface, which is never zero.
    Swapchain(IDXGIFactory6& factory, IUnknown& presenter, const Surface& surface);
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    ~Swapchain();

    // Shows the buffer drawn last, with the result of DXGI, which a lost device fails.
    [[nodiscard]] HRESULT present();

    // Gives the buffers a new size, once the device let go of every reference to them.
    [[nodiscard]] HRESULT resize(int width, int height);

    [[nodiscard]] IDXGISwapChain3& get() const noexcept {
        return *swapchain.Get();
    }

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }

    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }

  private:
    using CreateComposition = HRESULT(WINAPI*)(IUnknown*, REFIID, void**);

    void compose(HWND window);

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain;
    HMODULE compositionLibrary = nullptr;
    Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> composition;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> visual;
    UINT flags = 0;
    bool vsync = true;
    bool tearing = false;
    int width = 0;
    int height = 0;
};

} // namespace haylen::gpu::direct3d
