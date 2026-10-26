#include "gpu/direct3d/HResult.hpp"

#include <dxgi.h>

#include <format>
#include <stdexcept>

namespace haylen::gpu::direct3d {

void HResult::check(HRESULT result, std::string_view action) {
    if (SUCCEEDED(result)) {
        return;
    }
    throw std::runtime_error(std::format("The graphics device could not {}, because {}.", action, describe(result)));
}

std::string HResult::describe(HRESULT result) {
    switch (result) {
    case E_OUTOFMEMORY:
        return "it ran out of memory";
    case E_INVALIDARG:
        return "the graphics API rejected the description";
    case DXGI_ERROR_DEVICE_REMOVED:
        return "the GPU was removed";
    case DXGI_ERROR_DEVICE_HUNG:
        return "the GPU stopped responding";
    case DXGI_ERROR_DEVICE_RESET:
        return "the GPU was reset";
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
        return "the driver of the GPU failed";
    case DXGI_ERROR_UNSUPPORTED:
        return "the GPU does not support it";
    case DXGI_ERROR_NOT_FOUND:
        return "the system found no such object";
    case DXGI_ERROR_NOT_CURRENTLY_AVAILABLE:
        return "Windows gives this process no desktop to show a window on, as for a service";
    case DXGI_ERROR_SDK_COMPONENT_MISSING:
        return "the Graphics Tools of Windows are not installed";
    default:
        return std::format("of the error 0x{:08X}", static_cast<unsigned long>(result));
    }
}

bool HResult::isDeviceLoss(HRESULT result) noexcept {
    return result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_HUNG || result == DXGI_ERROR_DEVICE_RESET || result == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

} // namespace haylen::gpu::direct3d
