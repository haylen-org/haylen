#include "support/OffscreenGlContext.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdint>
#include <stdexcept>

namespace haylen::test {

// Windows makes OpenGL contexts on the device context of a window, so the context draws into a hidden window, and the attributes of `WGL_ARB_create_context` come from a first context that exists only to load them. The context asks to be lost when the GPU resets, as the contexts of apps do, so a reset ends as a lost device instead of a wait for a frame the GPU will not finish. The display holds the device context and the surface holds the window.
class OffscreenGlContext::Native final {
  public:
    static constexpr int kContextMajorVersion = 0x2091;
    static constexpr int kContextMinorVersion = 0x2092;
    static constexpr int kContextFlags = 0x2094;
    static constexpr int kContextProfileMask = 0x9126;
    static constexpr int kContextResetNotificationStrategy = 0x8256;
    static constexpr int kContextDebugBit = 0x0001;
    static constexpr int kContextRobustAccessBit = 0x0004;
    static constexpr int kLoseContextOnReset = 0x8252;
    static constexpr int kContextCoreProfileBit = 0x0001;

    using CreateContextAttribs = HGLRC(WINAPI*)(HDC, HGLRC, const int*);
    using SwapInterval = BOOL(WINAPI*)(int);

    static HWND makeWindow(int width, int height) {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW windowClass{};
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = instance;
        windowClass.lpszClassName = L"HaylenOffscreenGl";
        RegisterClassW(&windowClass);
        return CreateWindowExW(0, windowClass.lpszClassName, L"Haylen", WS_OVERLAPPEDWINDOW, 0, 0, width, height, nullptr, nullptr, instance, nullptr);
    }

    static void choosePixelFormat(HDC deviceContext) {
        PIXELFORMATDESCRIPTOR format{};
        format.nSize = sizeof(format);
        format.nVersion = 1;
        format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        format.iPixelType = PFD_TYPE_RGBA;
        format.cColorBits = 32;
        format.cAlphaBits = 8;
        format.cDepthBits = 24;
        format.cStencilBits = 8;
        const int chosen = ChoosePixelFormat(deviceContext, &format);
        if (chosen == 0 || SetPixelFormat(deviceContext, chosen, &format) == FALSE) {
            throw std::runtime_error("Windows has no pixel format for an OpenGL context.");
        }
    }
};

OffscreenGlContext::OffscreenGlContext(gpu::Device::Api api, int pixelsWide, int pixelsHigh) : width(pixelsWide), height(pixelsHigh) {
    if (api != gpu::Device::Api::OpenGl) {
        throw std::runtime_error("Windows runs OpenGL ES only through a translation layer, which the tests do not use.");
    }
    const HWND window = Native::makeWindow(width, height);
    if (window == nullptr) {
        throw std::runtime_error("Windows could not open the hidden window of the OpenGL context.");
    }
    surface = window;
    const HDC deviceContext = GetDC(window);
    display = deviceContext;
    Native::choosePixelFormat(deviceContext);

    const HGLRC loader = wglCreateContext(deviceContext);
    wglMakeCurrent(deviceContext, loader);
    const auto createContextAttribs = reinterpret_cast<Native::CreateContextAttribs>(reinterpret_cast<void*>(wglGetProcAddress("wglCreateContextAttribsARB")));
    const std::array<int, 11> attributes{Native::kContextMajorVersion, 4, Native::kContextMinorVersion, 3, Native::kContextProfileMask, Native::kContextCoreProfileBit, Native::kContextFlags, Native::kContextDebugBit | Native::kContextRobustAccessBit, Native::kContextResetNotificationStrategy, Native::kLoseContextOnReset, 0};
    const HGLRC made = createContextAttribs == nullptr ? nullptr : createContextAttribs(deviceContext, nullptr, attributes.data());
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(loader);
    if (made == nullptr) {
        ReleaseDC(window, deviceContext);
        DestroyWindow(window);
        throw std::runtime_error("Windows could not make an OpenGL 4.3 core context.");
    }
    context = made;
    makeCurrent();
}

OffscreenGlContext::~OffscreenGlContext() {
    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(static_cast<HGLRC>(context));
    ReleaseDC(static_cast<HWND>(surface), static_cast<HDC>(display));
    DestroyWindow(static_cast<HWND>(surface));
}

void OffscreenGlContext::makeCurrent() {
    wglMakeCurrent(static_cast<HDC>(display), static_cast<HGLRC>(context));
}

void OffscreenGlContext::swapBuffers() {
    SwapBuffers(static_cast<HDC>(display));
}

void OffscreenGlContext::setSwapInterval(int value) {
    if (const auto swapInterval = reinterpret_cast<Native::SwapInterval>(reinterpret_cast<void*>(wglGetProcAddress("wglSwapIntervalEXT")))) {
        swapInterval(value);
    }
}

// The functions of OpenGL 1.1 come from the library of the system, and drivers answer a name they lack with one of the small values below instead of null.
void* OffscreenGlContext::getProcAddress(const char* name) {
    void* address = reinterpret_cast<void*>(wglGetProcAddress(name));
    const auto value = reinterpret_cast<std::intptr_t>(address);
    if (value >= -1 && value <= 3) {
        address = reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name));
    }
    return address;
}

gpu::Surface OffscreenGlContext::getSurface() {
    return {.kind = gpu::Surface::Kind::Gl, .gl = this, .width = width, .height = height};
}

} // namespace haylen::test
