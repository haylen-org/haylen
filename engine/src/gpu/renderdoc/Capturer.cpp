#include "gpu/renderdoc/Capturer.hpp"

#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace haylen::gpu::renderdoc {

// RenderDoc names its library `renderdoc.dll` on Windows, `librenderdoc.so` on Linux and `libVkLayer_GLES_RenderDoc.so` on Android, where it is the layer it injects into the app.
void* Capturer::findEntryPoint() {
#if defined(_WIN32)
    const HMODULE library = GetModuleHandleW(L"renderdoc.dll");
    return library != nullptr ? reinterpret_cast<void*>(GetProcAddress(library, "RENDERDOC_GetAPI")) : nullptr;
#else
#if defined(__ANDROID__)
    void* library = dlopen("libVkLayer_GLES_RenderDoc.so", RTLD_NOW | RTLD_NOLOAD);
#else
    void* library = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
#endif
    if (library == nullptr) {
        return nullptr;
    }
    void* entry = dlsym(library, "RENDERDOC_GetAPI");
    dlclose(library);
    return entry;
#endif
}

std::unique_ptr<Capturer> Capturer::find() {
    void* entry = findEntryPoint();
    if (entry == nullptr) {
        return nullptr;
    }
    const auto getApi = reinterpret_cast<int (*)(int version, void** functions)>(entry);
    void* functions = nullptr;
    if (getApi(kVersion, &functions) != 1 || functions == nullptr) {
        return nullptr;
    }
    return std::unique_ptr<Capturer>(new Capturer(*static_cast<const Functions*>(functions)));
}

void Capturer::begin(void* device, const std::filesystem::path& path) {
    if (capturing) {
        throw std::logic_error("A capture began while another one records.");
    }
    const std::u8string text = path.u8string();
    functions.setCaptureFilePathTemplate(reinterpret_cast<const char*>(text.c_str()));
    capturesBefore = functions.getNumCaptures();
    functions.startFrameCapture(device, nullptr);
    if (functions.isFrameCapturing() == 0) {
        throw std::runtime_error("RenderDoc did not start the capture, since it captures no device of the app.");
    }
    capturing = true;
}

std::filesystem::path Capturer::end(void* device) {
    if (!capturing) {
        throw std::logic_error("A capture ended while none records.");
    }
    capturing = false;
    if (functions.endFrameCapture(device, nullptr) == 0) {
        throw std::runtime_error("RenderDoc failed to write the capture.");
    }
    const std::uint32_t captures = functions.getNumCaptures();
    if (captures == capturesBefore) {
        throw std::runtime_error("RenderDoc wrote no capture.");
    }

    std::uint32_t length = 0;
    if (functions.getCapture(captures - 1, nullptr, &length, nullptr) == 0 || length == 0) {
        throw std::runtime_error("RenderDoc gave no path for the capture it wrote.");
    }
    std::string name(length, '\0');
    functions.getCapture(captures - 1, name.data(), &length, nullptr);
    name.resize(name.find('\0') == std::string::npos ? name.size() : name.find('\0'));
    return std::filesystem::path(std::u8string(name.begin(), name.end()));
}

} // namespace haylen::gpu::renderdoc
