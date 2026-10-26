#include "gpu/direct3d/Swapchain.hpp"

#include <stdexcept>

#include "gpu/direct3d/Adapter.hpp"
#include "gpu/direct3d/Formats.hpp"
#include "gpu/direct3d/HResult.hpp"

namespace haylen::gpu::direct3d {

using Microsoft::WRL::ComPtr;

Swapchain::Swapchain(IDXGIFactory6& factory, IUnknown& presenter, const Surface& surface) : vsync(surface.vsync), width(surface.width), height(surface.height) {
    if (surface.kind != Surface::Kind::Win32 || surface.window == nullptr) {
        throw std::runtime_error("Direct3D draws only into the window of a surface of Windows.");
    }
    const auto window = static_cast<HWND>(surface.window);

    // Composition swapchains never tear, so only the swapchains of windows take the flag, which every resize must repeat.
    tearing = !surface.transparent && Adapter::canTear(factory);
    flags = tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Format = Formats::kSwapchainColor;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = flags;

    ComPtr<IDXGISwapChain1> made;
    if (surface.transparent) {
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        HResult::check(factory.CreateSwapChainForComposition(&presenter, &desc, nullptr, made.GetAddressOf()), "create the swapchain of a transparent window");
    } else {
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        HResult::check(factory.CreateSwapChainForHwnd(&presenter, window, &desc, nullptr, nullptr, made.GetAddressOf()), "create the swapchain of the window");
        // The platform switches between windowed and fullscreen itself, so DXGI leaves Alt+Enter alone.
        HResult::check(factory.MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "take the window over from DXGI");
    }
    HResult::check(made.As(&swapchain), "reach the swapchain of DXGI 1.4");

    if (surface.transparent) {
        compose(window);
    }
}

Swapchain::~Swapchain() {
    visual.Reset();
    target.Reset();
    composition.Reset();
    if (compositionLibrary != nullptr) {
        FreeLibrary(compositionLibrary);
    }
}

// DirectComposition loads at run time, since only transparent windows need it. The visual shows the swapchain over the window, which the platform makes without a redirection bitmap so nothing else draws behind it.
void Swapchain::compose(HWND window) {
    compositionLibrary = LoadLibraryExW(L"dcomp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (compositionLibrary == nullptr) {
        throw std::runtime_error("The transparent window needs DirectComposition, and \"dcomp.dll\" is missing from the system.");
    }
    const auto create = reinterpret_cast<CreateComposition>(reinterpret_cast<void*>(GetProcAddress(compositionLibrary, "DCompositionCreateDevice2")));
    if (create == nullptr) {
        throw std::runtime_error("The transparent window needs DirectComposition, and \"dcomp.dll\" has no function \"DCompositionCreateDevice2\".");
    }
    HResult::check(create(nullptr, IID_PPV_ARGS(composition.GetAddressOf())), "start DirectComposition for the transparent window");
    HResult::check(composition->CreateTargetForHwnd(window, TRUE, target.GetAddressOf()), "compose the transparent window");
    HResult::check(composition->CreateVisual(visual.GetAddressOf()), "make the visual of the transparent window");
    HResult::check(visual->SetContent(swapchain.Get()), "show the swapchain in the transparent window");
    HResult::check(target->SetRoot(visual.Get()), "show the visual in the transparent window");
    HResult::check(composition->Commit(), "commit the composition of the transparent window");
}

HRESULT Swapchain::present() {
    return swapchain->Present(vsync ? 1 : 0, !vsync && tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
}

HRESULT Swapchain::resize(int newWidth, int newHeight) {
    const HRESULT result = swapchain->ResizeBuffers(0, static_cast<UINT>(newWidth), static_cast<UINT>(newHeight), DXGI_FORMAT_UNKNOWN, flags);
    if (SUCCEEDED(result)) {
        width = newWidth;
        height = newHeight;
    }
    return result;
}

} // namespace haylen::gpu::direct3d
