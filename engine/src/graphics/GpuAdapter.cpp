#include "graphics/GpuAdapter.hpp"

#if defined(SOKOL_D3D11)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstddef>
#elif defined(SOKOL_WGPU)
#include <emscripten/emscripten.h>

#include <cstdlib>
#elif defined(SOKOL_GLES3)
#include <GLES3/gl3.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "sokol_gfx.h"

#if defined(SOKOL_WGPU)
// Browsers often leave the description of the adapter empty, and then its vendor and architecture name it. A browser whose devices tell nothing about their adapter names none.
// clang-format off
EM_JS(char*, haylen_js_webgpu_adapter, (const void* device), {
    const info = WebGPU.getJsObject(device).adapterInfo;
    if (!info) {
        return 0;
    }
    return stringToNewUTF8(info.description || [info.vendor, info.architecture].filter(Boolean).join(" "));
});
// clang-format on
#endif

namespace haylen::graphics {

// The build compiles the backend it chose, while the dummy backend of the headless host reports none of its devices.
std::string GpuAdapter::getName() {
#if defined(SOKOL_D3D11)
    const void* device = sg_d3d11_device();
    if (device == nullptr) {
        return {};
    }
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC description{};
    if (FAILED(static_cast<ID3D11Device*>(const_cast<void*>(device))->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()))) || FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) || FAILED(adapter->GetDesc(&description))) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, nullptr, 0, nullptr, nullptr);
    std::string name(static_cast<std::size_t>(std::max(0, size - 1)), '\0');
    WideCharToMultiByte(CP_UTF8, 0, description.Description, -1, name.data(), size, nullptr, nullptr);
    return name;
#elif defined(SOKOL_WGPU)
    const void* device = sg_wgpu_device();
    if (device == nullptr) {
        return {};
    }
    char* text = haylen_js_webgpu_adapter(device);
    if (text == nullptr) {
        return {};
    }
    std::string name(text);
    std::free(text);
    return name;
#else
    if (sg_query_backend() != SG_BACKEND_GLCORE && sg_query_backend() != SG_BACKEND_GLES3) {
        return {};
    }
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    return renderer != nullptr ? renderer : "";
#endif
}

} // namespace haylen::graphics
