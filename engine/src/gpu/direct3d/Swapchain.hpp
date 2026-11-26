#pragma once

#include <dcomp.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::direct3d {

// The flip model swapchain of a window of Windows, which a device of Direct3D 11 makes on its device and a device of Direct3D 12 on its command queue. A transparent window takes its frames through DirectComposition, which composites them with their premultiplied alpha over what lies behind the window, since only a swapchain for composition keeps the alpha. A swapchain without vertical sync presents with tearing where the display allows it, so frames reach the screen without waiting for its refresh. Its buffers take the size of the drawable, which DXGI stretches over the window and DirectComposition scales to it, so a drawable smaller than its window fills the window. The swapchain paces the frames through its frame latency waitable object, which lets one frame wait to be shown at a time, so a frame starts, and reads its input, only once the display can take it.
class Swapchain final {
  public:
    static constexpr UINT kBufferCount = 3;

    // The frames that wait to be shown at once, the fewest that DXGI allows, so input reaches the screen as soon as the display lets it.
    static constexpr UINT kFrameLatency = 1;

    // How long a frame waits for the display at most, the bound that the documentation of DXGI waits with, after which the frame goes on.
    static constexpr DWORD kFrameWaitMilliseconds = 1000;

    // Makes the swapchain on the window of the surface, at the size of the drawable of the surface, which is never zero.
    Swapchain(IDXGIFactory6& factory, IUnknown& presenter, const Surface& surface);
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Shows the buffer drawn last, with the result of DXGI, which a lost device fails, and keeps the count of the present with the moment its frame read its input.
    [[nodiscard]] HRESULT present();

    // Waits until the display can take another frame, which the device calls once each frame presented, so the next frame reads its input as late as the display allows.
    void waitForFrame() const noexcept;

    // Notes the moment the next frame reads its input, which the device calls as the wait for the display ends.
    void markInput() noexcept;

    // Reads from the statistics of DXGI when the display showed the frame that it showed last, which gives the age of the input of that frame where the swapchain still keeps its present.
    void readStatistics() noexcept;

    [[nodiscard]] std::optional<double> getInputAge() const noexcept {
        return inputAge;
    }

    // Gives the buffers the size of the drawable and shows them at the size of the window, once the device let go of every reference to them.
    [[nodiscard]] HRESULT resize(const Surface::Size& value);

    [[nodiscard]] IDXGISwapChain3& get() const noexcept {
        return *swapchain.Get();
    }

    [[nodiscard]] const Surface::Size& getSize() const noexcept {
        return size;
    }

  private:
    using CreateComposition = HRESULT(WINAPI*)(IUnknown*, REFIID, void**);

    // A present by its count, with the ticks of the performance counter when its frame read its input.
    struct Present {
        UINT count = 0;
        LONGLONG inputTicks = 0;
    };

    // The presents the swapchain keeps, more than the frames that DXGI queues before the display shows them.
    static constexpr std::size_t kKeptPresents = 8;

    // Frees DirectComposition once the objects it made went before it, also when the swapchain fails to compose.
    struct LibraryRelease {
        void operator()(HMODULE library) const noexcept {
            FreeLibrary(library);
        }
    };

    void compose(HWND window);
    void scaleVisual();

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain;
    std::unique_ptr<std::remove_pointer_t<HMODULE>, LibraryRelease> compositionLibrary;
    Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> composition;
    Microsoft::WRL::ComPtr<IDCompositionTarget> target;
    Microsoft::WRL::ComPtr<IDCompositionVisual2> visual;
    Microsoft::WRL::Wrappers::Event frameReady;
    UINT flags = 0;
    bool vsync = true;
    bool tearing = false;
    Surface::Size size;
    std::array<Present, kKeptPresents> presents{};
    std::size_t nextPresent = 0;
    LONGLONG inputTicks = 0;
    LONGLONG ticksPerSecond = 1;
    std::optional<double> inputAge;
};

} // namespace haylen::gpu::direct3d
