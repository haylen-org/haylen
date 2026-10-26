#pragma once

#include <windows.h>

#include <string>
#include <string_view>

namespace haylen::gpu::direct3d {

// Turns the results of the calls of Direct3D and DXGI into the sentences of the errors of the engine.
class HResult final {
  public:
    // Throws `std::runtime_error` saying that the device could not do the action, such as "create the swapchain", when the result is a failure.
    static void check(HRESULT result, std::string_view action);

    // The reason a result names, such as "the GPU was removed", or its code where it names none of the common ones.
    [[nodiscard]] static std::string describe(HRESULT result);

    // Whether the result says that the GPU stopped working, after which the device is lost.
    [[nodiscard]] static bool isDeviceLoss(HRESULT result) noexcept;
};

} // namespace haylen::gpu::direct3d
