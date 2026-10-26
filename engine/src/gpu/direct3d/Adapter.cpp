#include "gpu/direct3d/Adapter.hpp"

#include "gpu/direct3d/HResult.hpp"
#include "gpu/direct3d/WideText.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::gpu::direct3d {

using Microsoft::WRL::ComPtr;

ComPtr<IDXGIFactory6> Adapter::createFactory(bool validation) {
    ComPtr<IDXGIFactory6> factory;
    if (validation) {
        const HRESULT result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(factory.GetAddressOf()));
        if (SUCCEEDED(result)) {
            return factory;
        }
        core::Log::warning("The graphics device runs without the debug layer of DXGI, because {}. Add the optional feature \"Graphics Tools\" of Windows to validate it.", HResult::describe(result));
    }
    HResult::check(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf())), "create the factory of DXGI, which needs Windows 10 version 1803 or later");
    return factory;
}

std::vector<ComPtr<IDXGIAdapter1>> Adapter::list(IDXGIFactory6& factory, bool software) {
    std::vector<ComPtr<IDXGIAdapter1>> adapters;
    for (UINT index = 0; !software; ++index) {
        ComPtr<IDXGIAdapter1> adapter;
        if (factory.EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.GetAddressOf())) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        adapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
            adapters.push_back(std::move(adapter));
        }
    }

    ComPtr<IDXGIAdapter1> warp;
    if (SUCCEEDED(factory.EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf())))) {
        adapters.push_back(std::move(warp));
    }
    return adapters;
}

std::string Adapter::getName(IDXGIAdapter1& adapter) {
    DXGI_ADAPTER_DESC1 desc{};
    adapter.GetDesc1(&desc);
    return WideText::toUtf8(desc.Description);
}

bool Adapter::canTear(IDXGIFactory6& factory) noexcept {
    BOOL allowed = FALSE;
    return SUCCEEDED(factory.CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowed, sizeof(allowed))) && allowed == TRUE;
}

} // namespace haylen::gpu::direct3d
