#pragma once

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <string>
#include <vector>

namespace haylen::gpu::direct3d {

// The DXGI factory and the GPUs that the devices of Direct3D 11 and 12 start on.
class Adapter final {
  public:
    // Makes the factory, with the debug layer of DXGI when `validation` is on and Windows has its Graphics Tools.
    [[nodiscard]] static Microsoft::WRL::ComPtr<IDXGIFactory6> createFactory(bool validation);

    // The GPUs to try in turn: the hardware ones from the most powerful, or from the one that uses the least power, then the software rasterizer of Windows, WARP.
    [[nodiscard]] static std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter1>> list(IDXGIFactory6& factory, bool lowPower);

    // The GPUs that the options of a device ask for in the order to try them: WARP alone when they ask for software, and every GPU otherwise.
    [[nodiscard]] static std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter1>> choose(IDXGIFactory6& factory, bool software, bool lowPower);

    // WARP alone, which runs where no GPU does and draws the same pixels on every machine. Throws `std::runtime_error` when Windows lacks it.
    [[nodiscard]] static Microsoft::WRL::ComPtr<IDXGIAdapter1> getSoftware(IDXGIFactory6& factory);

    [[nodiscard]] static std::string getName(IDXGIAdapter1& adapter);

    // Whether the display shows frames as soon as they are presented without waiting for its refresh, which a swapchain without vertical sync uses.
    [[nodiscard]] static bool canTear(IDXGIFactory6& factory) noexcept;
};

} // namespace haylen::gpu::direct3d
